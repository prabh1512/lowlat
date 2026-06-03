#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <absl/container/flat_hash_map.h>

#include <lowlat/itch/parser.hpp>
#include <lowlat/book/book_handler.hpp>
#include <lowlat/book/order_book.hpp>
#include <lowlat/book/order_book_0.hpp>
#include <lowlat/book/commodity_book_1.hpp>
#include <lowlat/book/commodity_book_2.hpp>
#include <lowlat/book/commodity_book_3.hpp>
#include <lowlat/book/commodity_book_4.hpp>
#include <lowlat/book/commodity_book_5.hpp>
#include <lowlat/book/commodity_book_6.hpp>

using namespace lowlat::book;
using StlMap  = std::unordered_map<OrderId, std::uint32_t>;
using AbslMap = absl::flat_hash_map<OrderId, std::uint32_t>;

static void dump_vector(const std::string& path, const std::vector<std::uint32_t>& v) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(v.data()),
            static_cast<std::streamsize>(v.size() * sizeof(std::uint32_t)));
}

template <typename Book, typename CB>
int run(const std::string& itch_path, const std::string& tag) {
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
    dump_vector(base + "_add.bin",     handler.add_cycles);
    dump_vector(base + "_reduce.bin",  handler.reduce_cycles);
    dump_vector(base + "_delete.bin",  handler.delete_cycles);
    dump_vector(base + "_replace.bin", handler.replace_cycles);
    dump_vector(base + "_cb_add.bin",    CB::add_cycles);
    dump_vector(base + "_cb_reduce.bin", CB::reduce_cycles);
    return 0;
}

template <typename CB>
int dispatch_ob(const std::string& ob, const std::string& idmap,
                const std::string& itch_path, const std::string& tag) {
    if (ob == "v0" && idmap == "stl")
        return run<OrderBook_0<CB, StlMap>, CB>(itch_path, tag);
    if (ob == "v0" && idmap == "absl")
        return run<OrderBook_0<CB, AbslMap>, CB>(itch_path, tag);
    if (ob == "v1" && idmap == "stl")
        return run<OrderBook<CB, StlMap>, CB>(itch_path, tag);
    if (ob == "v1" && idmap == "absl")
        return run<OrderBook<CB, AbslMap>, CB>(itch_path, tag);
    std::cerr << "bad ob/idmap\n"; return 1;
}

int main(int argc, char* argv[]) {
    std::string variant, ob, idmap, itch_path;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a.rfind("--variant=", 0) == 0) variant = a.substr(10);
        else if (a.rfind("--ob=", 0) == 0) ob = a.substr(5);
        else if (a.rfind("--idmap=", 0) == 0) idmap = a.substr(8);
        else itch_path = a;
    }

    if (variant.empty() || ob.empty() || idmap.empty() || itch_path.empty()) {
        std::cerr << "usage: " << argv[0]
                  << " --variant=<v1..v6> --ob=<v0|v1> --idmap=<stl|absl> <itch_file>\n"
                  << "  ob=v0: unordered_map for stock_to_book\n"
                  << "  ob=v1: array for stock_to_book\n";
        return 1;
    }

    std::string tag = variant + "_ob" + ob + "_" + idmap;

    if (variant == "v1") return dispatch_ob<CommodityBookV1>(ob, idmap, itch_path, tag);
    if (variant == "v2") return dispatch_ob<CommodityBookV2>(ob, idmap, itch_path, tag);
    if (variant == "v3") return dispatch_ob<CommodityBookV3>(ob, idmap, itch_path, tag);
    if (variant == "v4") return dispatch_ob<CommodityBookV4>(ob, idmap, itch_path, tag);
    if (variant == "v5") return dispatch_ob<CommodityBookV5>(ob, idmap, itch_path, tag);
    if (variant == "v6") return dispatch_ob<CommodityBookV6>(ob, idmap, itch_path, tag);

    std::cerr << "unknown variant: " << variant << '\n';
    return 1;
}