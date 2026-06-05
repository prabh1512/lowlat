#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <lowlat/itch/messages.hpp>
#include <lowlat/itch/dispatcher.hpp>

using namespace lowlat::itch;

template <typename Msg>
static std::array<std::byte, sizeof(Msg)> make_msg(Msg m) {
    std::array<std::byte, sizeof(Msg)> buf;
    std::memcpy(buf.data(), &m, sizeof(Msg));
    return buf;
}

template <typename T>
static T be(T v) {
    if constexpr (std::endian::native == std::endian::little)
        return std::byteswap(v);
    return v;
}

static Timestamp48 ts48(std::uint64_t ns) {
    Timestamp48 t{};
    for (int i = 5; i >= 0; --i) { t.bytes[i] = ns & 0xFF; ns >>= 8; }
    return t;
}

// Handler that invokes a callback only for type T; ignores all others.
// Template catch-all avoids duplicate overloads when T is one of the message types.
template <typename T, typename Fn>
struct SingleHandler {
    Fn fn;
    void operator()(const T& m) { fn(m); }
    template <typename U>
    void operator()(const U&) {}
};

template <typename T, typename Fn>
static auto make_handler(Fn fn) { return SingleHandler<T, Fn>{fn}; }

// ── AddOrder ────────────────────────────────────────────────────────────────

TEST(ITCHDispatcher, AddOrder_FieldsDecoded) {
    AddOrder m{};
    m.message_type = 'A';
    m.stock_locate  = {be<uint16_t>(7)};
    m.order_ref     = {be<uint64_t>(0xDEADBEEF)};
    m.buy_sell      = 'B';
    m.shares        = {be<uint32_t>(500)};
    m.price         = {be<uint32_t>(123456)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<AddOrder>([&](const AddOrder& r) {
        EXPECT_EQ(r.stock_locate.get(), 7u);
        EXPECT_EQ(r.order_ref.get(), 0xDEADBEEFu);
        EXPECT_EQ(r.buy_sell, 'B');
        EXPECT_EQ(r.shares.get(), 500u);
        EXPECT_EQ(r.price.get(), 123456u);
        called = true;
    }));
    EXPECT_TRUE(called);
}

TEST(ITCHDispatcher, AddOrder_SellSide) {
    AddOrder m{};
    m.message_type = 'A';
    m.buy_sell = 'S';
    m.shares = {be<uint32_t>(100)};
    m.price  = {be<uint32_t>(999)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<AddOrder>([&](const AddOrder& r) {
        EXPECT_EQ(r.buy_sell, 'S');
        called = true;
    }));
    EXPECT_TRUE(called);
}

TEST(ITCHDispatcher, AddOrderAttributed_Dispatched) {
    AddOrderAttributed m{};
    m.message_type = 'F';
    m.order_ref    = {be<uint64_t>(42)};
    m.shares       = {be<uint32_t>(200)};
    m.price        = {be<uint32_t>(5000)};
    m.buy_sell     = 'B';

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<AddOrderAttributed>([&](const AddOrderAttributed& r) {
        EXPECT_EQ(r.order_ref.get(), 42u);
        EXPECT_EQ(r.shares.get(), 200u);
        EXPECT_EQ(r.price.get(), 5000u);
        called = true;
    }));
    EXPECT_TRUE(called);
}

// ── OrderExecuted ────────────────────────────────────────────────────────────

TEST(ITCHDispatcher, OrderExecuted_FieldsDecoded) {
    OrderExecuted m{};
    m.message_type     = 'E';
    m.order_ref        = {be<uint64_t>(1234)};
    m.executed_shares  = {be<uint32_t>(50)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<OrderExecuted>([&](const OrderExecuted& r) {
        EXPECT_EQ(r.order_ref.get(), 1234u);
        EXPECT_EQ(r.executed_shares.get(), 50u);
        called = true;
    }));
    EXPECT_TRUE(called);
}

TEST(ITCHDispatcher, OrderExecutedWithPrice_FieldsDecoded) {
    OrderExecutedWithPrice m{};
    m.message_type    = 'C';
    m.order_ref       = {be<uint64_t>(999)};
    m.executed_shares = {be<uint32_t>(25)};
    m.execution_price = {be<uint32_t>(88888)};
    m.printable       = 'Y';

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<OrderExecutedWithPrice>([&](const OrderExecutedWithPrice& r) {
        EXPECT_EQ(r.order_ref.get(), 999u);
        EXPECT_EQ(r.executed_shares.get(), 25u);
        EXPECT_EQ(r.execution_price.get(), 88888u);
        EXPECT_EQ(r.printable, 'Y');
        called = true;
    }));
    EXPECT_TRUE(called);
}

// ── OrderCancel / Delete / Replace ──────────────────────────────────────────

TEST(ITCHDispatcher, OrderCancel_FieldsDecoded) {
    OrderCancel m{};
    m.message_type      = 'X';
    m.order_ref         = {be<uint64_t>(555)};
    m.cancelled_shares  = {be<uint32_t>(10)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<OrderCancel>([&](const OrderCancel& r) {
        EXPECT_EQ(r.order_ref.get(), 555u);
        EXPECT_EQ(r.cancelled_shares.get(), 10u);
        called = true;
    }));
    EXPECT_TRUE(called);
}

TEST(ITCHDispatcher, OrderDelete_FieldsDecoded) {
    OrderDelete m{};
    m.message_type = 'D';
    m.order_ref    = {be<uint64_t>(777)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<OrderDelete>([&](const OrderDelete& r) {
        EXPECT_EQ(r.order_ref.get(), 777u);
        called = true;
    }));
    EXPECT_TRUE(called);
}

TEST(ITCHDispatcher, OrderReplace_FieldsDecoded) {
    OrderReplace m{};
    m.message_type         = 'U';
    m.original_order_ref   = {be<uint64_t>(100)};
    m.new_order_ref        = {be<uint64_t>(200)};
    m.shares               = {be<uint32_t>(300)};
    m.price                = {be<uint32_t>(400)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<OrderReplace>([&](const OrderReplace& r) {
        EXPECT_EQ(r.original_order_ref.get(), 100u);
        EXPECT_EQ(r.new_order_ref.get(), 200u);
        EXPECT_EQ(r.shares.get(), 300u);
        EXPECT_EQ(r.price.get(), 400u);
        called = true;
    }));
    EXPECT_TRUE(called);
}

// ── Return sizes ─────────────────────────────────────────────────────────────

struct NullHandler {
    void operator()(const AddOrder&)               {}
    void operator()(const AddOrderAttributed&)     {}
    void operator()(const OrderExecuted&)          {}
    void operator()(const OrderExecutedWithPrice&) {}
    void operator()(const OrderCancel&)            {}
    void operator()(const OrderDelete&)            {}
    void operator()(const OrderReplace&)           {}
};

TEST(ITCHDispatcher, ReturnsSizeOfDispatchedMessage) {
    NullHandler h;
    AddOrder a{}; a.message_type = 'A';
    auto ba = make_msg(a);
    EXPECT_EQ(dispatch(ba.data(), h), sizeof(AddOrder));

    OrderDelete d{}; d.message_type = 'D';
    auto bd = make_msg(d);
    EXPECT_EQ(dispatch(bd.data(), h), sizeof(OrderDelete));

    OrderReplace r{}; r.message_type = 'U';
    auto br = make_msg(r);
    EXPECT_EQ(dispatch(br.data(), h), sizeof(OrderReplace));
}

TEST(ITCHDispatcher, UnknownTypeByte_ReturnsNonzeroSkipSize) {
    std::array<std::byte, 50> buf{};
    buf[0] = static_cast<std::byte>('S'); // system event — skipped, non-zero
    NullHandler h;
    EXPECT_EQ(dispatch(buf.data(), h), 12u); // message_size('S') == 12
}

// ── Timestamp48 ──────────────────────────────────────────────────────────────

TEST(ITCHMessages, Timestamp48_Roundtrip) {
    std::uint64_t ns = 0x123456789ABCULL;
    Timestamp48 t = ts48(ns);
    EXPECT_EQ(t.get(), ns);
}

TEST(ITCHMessages, Timestamp48_Zero) {
    Timestamp48 t = ts48(0);
    EXPECT_EQ(t.get(), 0u);
}

TEST(ITCHMessages, Timestamp48_MaxValue) {
    std::uint64_t ns = 0xFFFFFFFFFFFFULL;
    Timestamp48 t = ts48(ns);
    EXPECT_EQ(t.get(), ns);
}

// ── Timestamp field carried through dispatch ─────────────────────────────────

TEST(ITCHDispatcher, AddOrder_TimestampDecoded) {
    std::uint64_t ns = 0xABCDEF012345ULL;
    AddOrder m{};
    m.message_type = 'A';
    m.timestamp = ts48(ns);

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<AddOrder>([&](const AddOrder& r) {
        EXPECT_EQ(r.timestamp.get(), ns);
        called = true;
    }));
    EXPECT_TRUE(called);
}

// ── match_number field ───────────────────────────────────────────────────────

TEST(ITCHDispatcher, OrderExecuted_MatchNumberDecoded) {
    OrderExecuted m{};
    m.message_type  = 'E';
    m.order_ref     = {be<uint64_t>(1)};
    m.match_number  = {be<uint64_t>(0xCAFEBABEDEAD0001ULL)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<OrderExecuted>([&](const OrderExecuted& r) {
        EXPECT_EQ(r.match_number.get(), 0xCAFEBABEDEAD0001ULL);
        called = true;
    }));
    EXPECT_TRUE(called);
}

TEST(ITCHDispatcher, OrderExecutedWithPrice_MatchNumberDecoded) {
    OrderExecutedWithPrice m{};
    m.message_type    = 'C';
    m.order_ref       = {be<uint64_t>(2)};
    m.match_number    = {be<uint64_t>(9999999999ULL)};
    m.printable       = 'N';
    m.execution_price = {be<uint32_t>(50000)};

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<OrderExecutedWithPrice>([&](const OrderExecutedWithPrice& r) {
        EXPECT_EQ(r.match_number.get(), 9999999999ULL);
        EXPECT_EQ(r.printable, 'N');
        called = true;
    }));
    EXPECT_TRUE(called);
}

// ── attribution field (AddOrderAttributed) ───────────────────────────────────

TEST(ITCHDispatcher, AddOrderAttributed_Attribution) {
    AddOrderAttributed m{};
    m.message_type = 'F';
    m.order_ref    = {be<uint64_t>(1)};
    m.shares       = {be<uint32_t>(1)};
    m.price        = {be<uint32_t>(1)};
    m.buy_sell     = 'B';
    m.attribution[0] = 'G'; m.attribution[1] = 'S';
    m.attribution[2] = ' '; m.attribution[3] = ' ';

    bool called = false;
    auto buf = make_msg(m);
    dispatch(buf.data(), make_handler<AddOrderAttributed>([&](const AddOrderAttributed& r) {
        EXPECT_EQ(r.attribution[0], 'G');
        EXPECT_EQ(r.attribution[1], 'S');
        called = true;
    }));
    EXPECT_TRUE(called);
}

// ── Sequential multi-message parse ───────────────────────────────────────────

TEST(ITCHDispatcher, SequentialParse_ConsumedSizesAdvanceCorrectly) {
    // Pack AddOrder + OrderDelete + OrderReplace back-to-back.
    constexpr std::size_t total = sizeof(AddOrder) + sizeof(OrderDelete) + sizeof(OrderReplace);
    std::array<std::byte, total> buf{};

    AddOrder a{};    a.message_type = 'A';
    OrderDelete d{}; d.message_type = 'D';
    OrderReplace r{}; r.message_type = 'U';

    std::memcpy(buf.data(),                                   &a, sizeof(a));
    std::memcpy(buf.data() + sizeof(AddOrder),                &d, sizeof(d));
    std::memcpy(buf.data() + sizeof(AddOrder) + sizeof(OrderDelete), &r, sizeof(r));

    NullHandler h;
    const std::byte* ptr = buf.data();

    std::size_t s1 = dispatch(ptr, h);
    EXPECT_EQ(s1, sizeof(AddOrder));
    ptr += s1;

    std::size_t s2 = dispatch(ptr, h);
    EXPECT_EQ(s2, sizeof(OrderDelete));
    ptr += s2;

    std::size_t s3 = dispatch(ptr, h);
    EXPECT_EQ(s3, sizeof(OrderReplace));
    ptr += s3;

    EXPECT_EQ(ptr, buf.data() + total);
}

TEST(ITCHDispatcher, SequentialParse_FieldsPreservedAcrossMessages) {
    // Two AddOrders with different order_refs in one buffer.
    constexpr std::size_t total = sizeof(AddOrder) * 2;
    std::array<std::byte, total> buf{};

    AddOrder m1{}, m2{};
    m1.message_type = 'A'; m1.order_ref = {be<uint64_t>(111)};
    m2.message_type = 'A'; m2.order_ref = {be<uint64_t>(222)};
    std::memcpy(buf.data(),               &m1, sizeof(m1));
    std::memcpy(buf.data() + sizeof(m1),  &m2, sizeof(m2));

    std::vector<uint64_t> refs;
    auto h = make_handler<AddOrder>([&](const AddOrder& r) { refs.push_back(r.order_ref.get()); });

    const std::byte* ptr = buf.data();
    ptr += dispatch(ptr, h);
    ptr += dispatch(ptr, h);

    ASSERT_EQ(refs.size(), 2u);
    EXPECT_EQ(refs[0], 111u);
    EXPECT_EQ(refs[1], 222u);
}
