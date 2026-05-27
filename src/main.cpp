#include <lowlat/itch/messages.hpp>
#include <cstdio>

int main() {
    using namespace lowlat::itch;
    Price p = 1234567;  // represents $123.4567
    std::printf("lowlat: price scale = %u, sample price = %u\n", PRICE_SCALE, p);
    return 0;
}
