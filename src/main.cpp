#include <chrono>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <lowlat/itch/parser.hpp>
#include <lowlat/book/order_book.hpp>
#include <lowlat/book/commodity_book_5.hpp>
#include <lowlat/book/book_handler.hpp>

static void dump_vector(const std::string& path, const std::vector<std::uint32_t>& v) {
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(v.data()),
            static_cast<std::streamsize>(v.size() * sizeof(std::uint32_t)));
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "usage: " << argv[0] << " <itch_file>\n";
        return 1;
    }

    lowlat::book::OrderBook<lowlat::book::CommodityBookV2> book;
    lowlat::book::BookHandler<lowlat::book::CommodityBookV2> handler(book);

    lowlat::itch::ParseStats ps;
    auto wall_start = std::chrono::steady_clock::now();
    try {
        ps = lowlat::itch::parse_file(argv[1], handler);
    } catch (const std::runtime_error& e) {
        std::cerr << "error: " << e.what() << '\n';
        return 1;
    }
    auto wall_end = std::chrono::steady_clock::now();
    double seconds = std::chrono::duration<double>(wall_end - wall_start).count();

    std::cout << "--- Summary ---\n";
    std::cout << "messages processed:  " << ps.messages << '\n';
    // std::cout << "stocks tracked:      " << book.stock_to_book.size() << '\n';
    std::cout << "peak live orders:    " << book.peak_live_orders << '\n';
    std::cout << "wall time (sec):     " << seconds << '\n';
    std::cout << "throughput (msg/s):  " << static_cast<std::uint64_t>(static_cast<double>(ps.messages) / seconds)  << '\n';
    std::cout << "--- Op counts ---\n";
    std::cout << "  add:     " << handler.add_cycles.size() << '\n';
    std::cout << "  reduce:  " << handler.reduce_cycles.size() << '\n';
    std::cout << "  delete:  " << handler.delete_cycles.size() << '\n';
    std::cout << "  replace: " << handler.replace_cycles.size() << '\n';

    std::cout << "--- Dumping cycle data ---\n";
    dump_vector("bench-results/add_cycles.bin",     handler.add_cycles);
    dump_vector("bench-results/reduce_cycles.bin",  handler.reduce_cycles);
    dump_vector("bench-results/delete_cycles.bin",  handler.delete_cycles);
    dump_vector("bench-results/replace_cycles.bin", handler.replace_cycles);
    dump_vector("bench-results/cb_add_cycles.bin", lowlat::book::CommodityBookV2::add_cycles);
    dump_vector("bench-results/cb_reduce_cycles.bin", lowlat::book::CommodityBookV2::reduce_cycles);

    std::cout << "done.\n";
    return 0;
}