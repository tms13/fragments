#ifndef ENDIAN_HPP
#define ENDIAN_HPP

#include <array>
#include <bit>
#include <climits>
#include <concepts>
#include <ranges>
#include <type_traits>

#ifndef ENDIAN_SUPPORT_NON_8BIT
static_assert(CHAR_BIT == 8,
              "This header splits into chars, not octets. "
              "Define ENDIAN_SUPPORT_NON_8BIT to enable.");
#endif

namespace endian
{
    namespace detail {
        template<std::integral T, // type to represent
                 auto BigFirst,    // view that presents MSB first
                 auto LittleFirst> // view that presents LSB first
        struct Endian
        {
            // We use unsigned T for bitwise operations
            using U = std::make_unsigned_t<T>;

            // The underlying storage
            std::array<unsigned char, sizeof (T)> data = {};

            constexpr Endian() = default;

            // implicit conversion from T
            constexpr Endian(T value)
            {
                // unpack value starting with the least-significant bits
                auto uval = static_cast<U>(value);
                for (auto& c: data | LittleFirst) {
                    c = static_cast<unsigned char>(uval);
                    uval >>= CHAR_BIT;
                }
            }

            // implicit conversion to T
            constexpr operator T() const
            {
                // compose value starting with most-significant bits
                U value = 0;
                for (auto c: data | BigFirst) {
                    value <<= CHAR_BIT;
                    value |= c;
                }
                return static_cast<T>(value);
            }
        };
    }

    template<std::integral T>
    using BigEndian =
        std::conditional_t<std::endian::native == std::endian::big,
                           T,   // no conversion needed
                           detail::Endian<T, std::views::all, std::views::reverse>>;

    template<std::integral T>
    using LittleEndian =
        std::conditional_t<std::endian::native == std::endian::little,
                           T,   // no conversion needed
                           detail::Endian<T, std::views::reverse, std::views::all>>;

}

#endif // ENDIAN_HPP


#if 1

#include <unistd.h>

int fd;
std::uint16_t counter;

using endian::BigEndian;
using endian::LittleEndian;

struct alignas(std::uint16_t) Response
{
    BigEndian<std::uint16_t> seq_no = {};
    BigEndian<std::uint16_t> sample_value = {};
};

void send_result(std::uint16_t value)
{
    Response r;
    r.seq_no = counter++;
    r.sample_value = value;
    write(fd, &r, sizeof r);
}

std::uint16_t recv_result()
{
    Response r;
    read(fd, &r, sizeof r);
    // ignore seq_no, for now
    return r.sample_value;
}


#endif


using endian::BigEndian;
using endian::LittleEndian;

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

// Ensure there's no padding
static_assert(sizeof (BigEndian<int>) == sizeof (int));
static_assert(sizeof (LittleEndian<int>) == sizeof (int));

// Helper function to inspect representation
template<typename T>
auto byte_array(const T& t)
{
    std::array<unsigned char, sizeof t> bytes;
    std::memcpy(bytes.data(), &t, sizeof t);
    return bytes;
}

// Now the tests themselves
TEST(big_endian, uint8)
{
    const std::uint8_t x = 2;
    auto be = BigEndian<std::uint8_t>{x};
    std::array<unsigned char, 1> expected{{2}};
    EXPECT_EQ(byte_array(be), expected);

    // round trip back to native
    std::uint8_t y = be;
    EXPECT_EQ(y, x);
}

TEST(little_endian, uint8)
{
    const std::uint8_t x = 2;
    auto le = LittleEndian<std::uint8_t>{x};
    std::array<unsigned char, 1> expected{{2}};
    EXPECT_EQ(byte_array(le), expected);

    std::uint8_t y = le;
    EXPECT_EQ(y, x);
}

TEST(big_endian, uint16)
{
    const std::uint16_t x = 0x1234;
    BigEndian<std::uint16_t> be = x;
    std::array<unsigned char, 2> expected{{0x12, 0x34}};
    EXPECT_EQ(byte_array(be), expected);

    std::uint16_t y = be;
    EXPECT_EQ(y, x);
}

TEST(little_endian, uint16)
{
    const std::uint16_t x =  0x1234;
    auto le = LittleEndian<std::uint16_t>{x};
    std::array<unsigned char, 2> expected{{0x34, 0x12}};
    EXPECT_EQ(byte_array(le), expected);

    std::uint16_t y = le;
    EXPECT_EQ(y, x);
}

TEST(big_endian, uint32)
{
    const std::uint32_t x = 0x12345678;
    auto be = BigEndian<std::uint32_t>{x};
    std::array<unsigned char, 4> expected{{ 0x12, 0x34, 0x56, 0x78 }};
    EXPECT_EQ(byte_array(be), expected);

    std::uint32_t y = be;
    EXPECT_EQ(y, x);
}

TEST(little_endian, uint32)
{
    const std::uint32_t x = 0x12345678;
    auto le = LittleEndian<std::uint32_t>{x};
    std::array<unsigned char, 4> expected{{ 0x78, 0x56, 0x34, 0x12 }};
    EXPECT_EQ(byte_array(le), expected);

    std::uint32_t y = le;
    EXPECT_EQ(y, x);
}
