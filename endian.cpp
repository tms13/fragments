#ifndef ENDIAN_HPP
#define ENDIAN_HPP

#include <array>
#include <concepts>
#include <cstdint>
#include <ranges>
#include <type_traits>

namespace endian
{

    template<std::integral T, auto ReadView, auto WriteView>
    struct Endian
    {
        // We use unsigned T for bitwise operations
        using U = std::make_unsigned_t<T>;

        // The underlying storage
        std::array<unsigned char, sizeof (T)> data = {};

        // implicit conversion from T
        Endian(T value = 0)
        {
            auto uval = static_cast<U>(value);
            for (auto& c: data | WriteView) {
                c = static_cast<std::uint8_t>(uval);
                uval >>= 8;
            }
        }

        // implicit conversion to T
        operator T() const
        {
            U value = 0;
            for (auto c: data | ReadView) {
                value <<= 8;
                value |= c;
            }
            return static_cast<T>(value);
        }
    };

    template<std::integral T>
    using BigEndian = Endian<T, std::views::all, std::views::reverse>;

    template<std::integral T>
    using LittleEndian = Endian<T, std::views::reverse, std::views::all>;

}

#endif // ENDIAN_HPP


using endian::BigEndian;
using endian::LittleEndian;

#include <gtest/gtest.h>

// Ensure there's no padding
static_assert(sizeof (BigEndian<int>) == sizeof (int));
static_assert(sizeof (LittleEndian<int>) == sizeof (int));

TEST(big_endian, uint8)
{
    std::uint8_t x = 2;
    auto be = BigEndian{x};
    std::array<unsigned char, 1> expected{{2}};
    EXPECT_EQ(be.data,expected);

    for (auto& c: be.data) { ++c; }
    std::uint8_t y = be;
    EXPECT_EQ(y, 3);
}

TEST(little_endian, uint8)
{
    std::uint8_t x = 2;
    auto le = LittleEndian{x};
    std::array<unsigned char, 1> expected{{2}};
    EXPECT_EQ(le.data,expected);

    for (auto& c: le.data) { ++c; }
    std::uint8_t y = le;
    EXPECT_EQ(y, 3);
}

TEST(big_endian, uint16)
{
    std::uint16_t x = 0x1234;
    BigEndian be = x;
    std::array<unsigned char, 2> expected{{0x12, 0x34}};
    EXPECT_EQ(be.data,expected);

    for (auto& c: be.data) { ++c; }
    std::uint16_t y = be;
    EXPECT_EQ(y, 0x1335);
}

TEST(little_endian, uint16)
{
    std::uint16_t x =  0x1234;
    auto le = LittleEndian{x};
    std::array<unsigned char, 2> expected{{0x34, 0x12}};
    EXPECT_EQ(le.data,expected);

    for (auto& c: le.data) { ++c; }
    std::uint16_t y = le;
    EXPECT_EQ(y, 0x1335);
}

TEST(big_endian, uint32)
{
    std::uint32_t x = 0x12345678;
    auto be = BigEndian{x};
    std::array<unsigned char, 4> expected{{ 0x12, 0x34, 0x56, 0x78 }};
    EXPECT_EQ(be.data,expected);

    for (auto& c: be.data) { ++c; }
    std::uint32_t y = be;
    EXPECT_EQ(y, 0x13355779);
}

TEST(little_endian, uint32)
{
    std::uint32_t x = 0x12345678;
    auto le = LittleEndian{x};
    std::array<unsigned char, 4> expected{{ 0x78, 0x56, 0x34, 0x12 }};
    EXPECT_EQ(le.data,expected);

    for (auto& c: le.data) { ++c; }
    std::uint32_t y = le;
    EXPECT_EQ(y, 0x13355779);
}
