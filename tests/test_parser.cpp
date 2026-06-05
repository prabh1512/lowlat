#include <gtest/gtest.h>
#include <cstring>
#include <cstdio>
#include <stdexcept>
#include <vector>
#include <lowlat/itch/parser.hpp>
#include <lowlat/itch/messages.hpp>

using namespace lowlat::itch;

// Write a SoupBinTCP frame: [2-byte BE length][payload]
static void write_frame(FILE* f, const void* msg, std::size_t len) {
    std::uint8_t hdr[2] = {
        static_cast<std::uint8_t>(len >> 8),
        static_cast<std::uint8_t>(len & 0xFF)
    };
    fwrite(hdr, 1, 2, f);
    fwrite(msg, 1, len, f);
}

struct CountHandler {
    std::vector<char> seen;
    void operator()(const AddOrder&)               { seen.push_back('A'); }
    void operator()(const AddOrderAttributed&)     { seen.push_back('F'); }
    void operator()(const OrderExecuted&)          { seen.push_back('E'); }
    void operator()(const OrderExecutedWithPrice&) { seen.push_back('C'); }
    void operator()(const OrderCancel&)            { seen.push_back('X'); }
    void operator()(const OrderDelete&)            { seen.push_back('D'); }
    void operator()(const OrderReplace&)           { seen.push_back('U'); }
};

// ── read_be16 ────────────────────────────────────────────────────────────────

TEST(ITCHParser, ReadBe16) {
    std::array<std::byte, 2> buf{std::byte{0x00}, std::byte{0x24}};
    EXPECT_EQ(read_be16(buf.data()), 36u);

    std::array<std::byte, 2> buf2{std::byte{0x01}, std::byte{0x00}};
    EXPECT_EQ(read_be16(buf2.data()), 256u);
}

// ── single message round-trip ────────────────────────────────────────────────

TEST(ITCHParser, SingleAddOrderParsed) {
    char path[] = "/tmp/itch_test_XXXXXX";
    int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    FILE* f = fdopen(fd, "wb");

    AddOrder m{};
    m.message_type = 'A';
    // big-endian encode a known order_ref
    uint64_t ref = 0xDEADBEEF;
    for (int i = 7; i >= 0; --i) { reinterpret_cast<uint8_t*>(&m.order_ref)[i] = ref & 0xFF; ref >>= 8; }
    m.buy_sell = 'B';
    write_frame(f, &m, sizeof(m));
    fclose(f);

    CountHandler h;
    auto stats = parse_file(path, h);
    std::remove(path);

    EXPECT_EQ(stats.total_packets, 1u);
    EXPECT_EQ(stats.messages, 1u);
    ASSERT_EQ(h.seen.size(), 1u);
    EXPECT_EQ(h.seen[0], 'A');
}

// ── multiple messages in sequence ────────────────────────────────────────────

TEST(ITCHParser, MultipleMessagesInSequence) {
    char path[] = "/tmp/itch_test_XXXXXX";
    int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    FILE* f = fdopen(fd, "wb");

    AddOrder    a{}; a.message_type = 'A';
    OrderDelete d{}; d.message_type = 'D';
    OrderCancel x{}; x.message_type = 'X';
    write_frame(f, &a, sizeof(a));
    write_frame(f, &d, sizeof(d));
    write_frame(f, &x, sizeof(x));
    fclose(f);

    CountHandler h;
    auto stats = parse_file(path, h);
    std::remove(path);

    EXPECT_EQ(stats.total_packets, 3u);
    EXPECT_EQ(stats.messages, 3u);
    ASSERT_EQ(h.seen.size(), 3u);
    EXPECT_EQ(h.seen[0], 'A');
    EXPECT_EQ(h.seen[1], 'D');
    EXPECT_EQ(h.seen[2], 'X');
}

// ── nonexistent file throws ───────────────────────────────────────────────────

TEST(ITCHParser, NonexistentFileThrows) {
    CountHandler h;
    EXPECT_THROW(parse_file("/tmp/does_not_exist_lowlat.bin", h), std::runtime_error);
}

// ── truncated frame stops cleanly ────────────────────────────────────────────

TEST(ITCHParser, TruncatedFrameStopsCleanly) {
    char path[] = "/tmp/itch_test_XXXXXX";
    int fd = mkstemp(path);
    ASSERT_GE(fd, 0);
    FILE* f = fdopen(fd, "wb");

    // Write one good message, then a truncated frame (length says 36, only 4 bytes follow)
    AddOrder a{}; a.message_type = 'A';
    write_frame(f, &a, sizeof(a));
    std::uint8_t bad_hdr[2] = {0x00, 0x24}; // claims 36 bytes
    std::uint8_t stub[4] = {};
    fwrite(bad_hdr, 1, 2, f);
    fwrite(stub, 1, 4, f);
    fclose(f);

    CountHandler h;
    auto stats = parse_file(path, h);
    std::remove(path);

    EXPECT_EQ(stats.messages, 1u); // only the good one counted
}
