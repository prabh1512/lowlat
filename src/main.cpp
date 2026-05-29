#include <cstdio>
#include <stdexcept>
#include <iostream>
#include <lowlat/itch/parser.hpp>
#include <lowlat/itch/count_handler.hpp>

int main(int argc, char* argv[]) {
    if (argc != 2){
        std::cerr << "pass file name as the one and only argument\n";
        return 1;
    }

    using namespace lowlat::itch;

    CountingHandler ch;
    const std::string path = argv[1];
    ParseStats ps;

    try{
        ps = parse_file(path, ch);
    }

    catch(const std::runtime_error&e ){
        std::cerr << e.what() << '\n'; return 1;
    }

    std::cout << "--- ParseStats ---\n"
              << "  total_packets:  " << ps.total_packets  << '\n'
              << "  messages:       " << ps.messages       << '\n'
              << "  bytes_consumed: " << ps.bytes_consumed << '\n';

    std::cout << "--- Message counts ---\n"
              << "  A  AddOrder:                " << ch.adds        << '\n'
              << "  F  AddOrderAttributed:      " << ch.add_attribs << '\n'
              << "  E  OrderExecuted:           " << ch.executes    << '\n'
              << "  C  OrderExecutedWithPrice:  " << ch.executes_p  << '\n'
              << "  X  OrderCancel:             " << ch.cancels     << '\n'
              << "  D  OrderDelete:             " << ch.deletes     << '\n'
              << "  U  OrderReplace:            " << ch.replaces    << '\n';
    
    return 0;
}