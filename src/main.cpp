#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <absl/container/flat_hash_map.h>

#include <lowlat/itch/parser.hpp>
#include <lowlat/book/book_handler.hpp>
#include <lowlat/book/order_book.hpp>
#include <lowlat/book/order_book_0.hpp>
#include <lowlat/book/book_update.hpp>
#include <lowlat/book/commodity_book_1.hpp>
#include <lowlat/book/commodity_book_2.hpp>
#include <lowlat/book/commodity_book_3.hpp>
#include <lowlat/book/commodity_book_4.hpp>
#include <lowlat/book/commodity_book_5.hpp>
#include <lowlat/book/commodity_book_6.hpp>
#include <lowlat/log/async_logger.hpp>

using namespace lowlat::book;
using StlMap  = std::unordered_map<OrderId, std::uint32_t>;
using AbslMap = absl::flat_hash_map<OrderId, std::uint32_t>;

static void dump_vector(const std::string& path, const std::vector<std::uint32_t>& v) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(v.data()),
            static_cast<std::streamsize>(v.size() * sizeof(std::uint32_t)));
}

// === Mode A: bench (variant dispatch, no sink) ===

template <typename Book, typename CB>
int run_bench(const std::string& itch_path, const std::string& tag) {
    auto book = std::make_unique<Book>();
    BookHandler<Book> handler(*book);

    auto t0 = std::chrono::steady_clock::now();
    auto ps = lowlat::itch::parse_file(itch_path, handler);
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();

    std::cout << "--- " << tag << " ---\n";
    std::cout << "messages:    " << ps.messages << '\n';
    std::cout << "peak orders: " << book->peak_live_orders << '\n';
    std::cout << "wall (s):    " << secs << '\n';
    std::cout << "msg/s:       "
              << static_cast<std::uint64_t>(static_cast<double>(ps.messages) / secs) << '\n';
    std::cout << "live end:    " << book->id_to_pool.size() << '\n';

    std::string base = "bench-results/" + tag;
    
    std::ofstream csv("bench-results/summary.csv", std::ios::app);
    csv << tag << "," << ps.messages << "," << secs << ","
    << static_cast<std::uint64_t>(static_cast<double>(ps.messages) / secs) << "\n";

    dump_vector(base + "_add.bin",     handler.add_cycles);
    dump_vector(base + "_reduce.bin",  handler.reduce_cycles);
    dump_vector(base + "_delete.bin",  handler.delete_cycles);
    dump_vector(base + "_replace.bin", handler.replace_cycles);
    dump_vector(base + "_cb_add.bin",    CB::add_cycles);
    dump_vector(base + "_cb_reduce.bin", CB::reduce_cycles);
    return 0;
}

template <typename CB>
int dispatch_bench(const std::string& ob, const std::string& idmap,
                   const std::string& itch_path, const std::string& tag) {
    if (ob == "v0" && idmap == "stl")
        return run_bench<OrderBook_0<CB, StlMap>, CB>(itch_path, tag);
    if (ob == "v0" && idmap == "absl")
        return run_bench<OrderBook_0<CB, AbslMap>, CB>(itch_path, tag);
    if (ob == "v1" && idmap == "stl")
        return run_bench<OrderBook<CB, StlMap>, CB>(itch_path, tag);
    if (ob == "v1" && idmap == "absl")
        return run_bench<OrderBook<CB, AbslMap>, CB>(itch_path, tag);
    std::cerr << "bad ob/idmap\n"; return 1;
}

// === Mode B: pipeline (v6 + absl + async logger) ===

struct LoggerSink {
    lowlat::log::AsyncLogger::Queue* q = nullptr;
    void operator()(const BookUpdate& u) noexcept { q->try_push(u); }
};

template <typename CB>
int run_pipeline_for(const std::string& itch_path, const std::string& log_path,
                     const std::string& tag) {
    using Book = OrderBook<CB, AbslMap, LoggerSink>;

    lowlat::log::AsyncLogger logger(log_path);
    auto book = std::make_unique<Book>();
    book->sink = LoggerSink{&logger.queue()};
    BookHandler<Book> handler(*book);

    auto t0 = std::chrono::steady_clock::now();
    auto ps = lowlat::itch::parse_file(itch_path, handler);
    auto t1 = std::chrono::steady_clock::now();
    double secs = std::chrono::duration<double>(t1 - t0).count();
    logger.queue().flush_writes();

    std::cout << "--- pipeline " << tag << " ---\n";
    std::cout << "messages:    " << ps.messages << '\n';
    std::cout << "peak orders: " << book->peak_live_orders << '\n';
    std::cout << "wall (s):    " << secs << '\n';
    std::cout << "msg/s:       "
              << static_cast<std::uint64_t>(static_cast<double>(ps.messages) / secs) << '\n';
    std::cout << "live end:    " << book->id_to_pool.size() << '\n';
    std::cout << "log file:    " << log_path << '\n';

    // Append summary CSV
    std::ofstream csv("bench-results/summary.csv", std::ios::app);
    csv << tag << "," << ps.messages << "," << secs << ","
        << static_cast<std::uint64_t>(static_cast<double>(ps.messages) / secs) << "\n";
    return 0;
}

int main(int argc, char* argv[]) {
    std::string mode = "bench";
    std::string variant = "v6", ob = "v1", idmap = "absl";
    std::string log_path = "bench-results/book_updates.log";
    std::string itch_path;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--mode=", 0) == 0)         mode = a.substr(7);
        else if (a.rfind("--variant=", 0) == 0) variant = a.substr(10);
        else if (a.rfind("--ob=", 0) == 0)      ob = a.substr(5);
        else if (a.rfind("--idmap=", 0) == 0)   idmap = a.substr(8);
        else if (a.rfind("--log=", 0) == 0)     log_path = a.substr(6);
        else itch_path = a;
    }

    if (itch_path.empty()) {
        std::cerr << "usage: " << argv[0]
                  << " [--mode=bench|pipeline] [--variant=v1..v6] [--ob=v0|v1]"
                  << " [--idmap=stl|absl] [--log=path] <itch_file>\n";
        return 1;
    }

    if (mode == "pipeline") {
        std::string tag = "pipe_" + variant;
        if (variant == "v1") return run_pipeline_for<CommodityBookV1>(itch_path, log_path, tag);
        if (variant == "v2") return run_pipeline_for<CommodityBookV2>(itch_path, log_path, tag);
        if (variant == "v3") return run_pipeline_for<CommodityBookV3>(itch_path, log_path, tag);
        if (variant == "v4") return run_pipeline_for<CommodityBookV4>(itch_path, log_path, tag);
        if (variant == "v5") return run_pipeline_for<CommodityBookV5>(itch_path, log_path, tag);
        if (variant == "v6") return run_pipeline_for<CommodityBookV6>(itch_path, log_path, tag);
        std::cerr << "unknown variant\n"; return 1;
    }

    // bench mode
    std::string tag = variant + "_ob" + ob + "_" + idmap;
    if (variant == "v1") return dispatch_bench<CommodityBookV1>(ob, idmap, itch_path, tag);
    if (variant == "v2") return dispatch_bench<CommodityBookV2>(ob, idmap, itch_path, tag);
    if (variant == "v3") return dispatch_bench<CommodityBookV3>(ob, idmap, itch_path, tag);
    if (variant == "v4") return dispatch_bench<CommodityBookV4>(ob, idmap, itch_path, tag);
    if (variant == "v5") return dispatch_bench<CommodityBookV5>(ob, idmap, itch_path, tag);
    if (variant == "v6") return dispatch_bench<CommodityBookV6>(ob, idmap, itch_path, tag);

    std::cerr << "unknown variant: " << variant << '\n';
    return 1;
}