#include <concepts>
#include <cstdint>
#include <iterator>
#include <ranges>
#include <string_view>
#include <type_traits>
#include <utility>

namespace typeutil
{
    // Select first of First, Rest... that is a baseclass of T
    template<typename T, typename First, typename... Rest>
    struct first_baseclass
    {
        using type = std::conditional_t<std::is_base_of_v<First, T>, First, typename first_baseclass<T, Rest...>::type>;
    };
    template<typename T, typename First>
    struct first_baseclass<T, First>
    {
        using type = std::enable_if_t<std::is_base_of_v<First, T>, First>;
    };

    template<typename... Args>
    using first_baseclass_t = first_baseclass<Args...>::type;
}

namespace codecvt
{
    using unit_count = std::int_least8_t;
    static constexpr char16_t replacement_char = u'\uFFFD';

    // Iterators

    template<std::input_iterator Input>
    requires std::is_same_v<typename std::iterator_traits<Input>::value_type, char32_t>
    class to_utf16_iterator
    {
    public:
        using iterator_category = typeutil::first_baseclass_t<typename std::iterator_traits<Input>::iterator_category,
                                                              std::bidirectional_iterator_tag,
                                                              std::forward_iterator_tag,
                                                              std::input_iterator_tag>;
        using difference_type = std::ptrdiff_t;
        using value_type = char16_t;
        using pointer = value_type const*;
        using reference = value_type const&;

    private:
        static constexpr unit_count first_unit = 0; // magic values for length and remaining
        static constexpr unit_count last_unit = -1;
        Input pos = {};                // underlying position
        mutable unit_count length = first_unit; // number of UTF-16 units for this pos (<=0 means unknown)
        mutable unit_count remaining = first_unit; // number of UTF-16 units left (0 means all, -1 means last)
        mutable value_type current = 0;   // result of * operator

        auto compare_key() const
        { return std::tuple{ pos, remaining == length ? 0 : remaining }; }

    public:
        to_utf16_iterator() = default;
        explicit to_utf16_iterator(Input pos)
            : pos{pos}
        {}

        bool operator==(const to_utf16_iterator& other) const
        {
            return compare_key() == other.compare_key();
        }

        reference operator*() const
        {
            if (current) {
                return current;
            }
            auto c = *pos;
            if (c <= 0xFFFF) {
                remaining = length = 1;
                if (0xD800 <= c && c <= 0xDFFF) {
                    // non-character UTF-32 units
                    return current = replacement_char;
                }
                return current = static_cast<value_type>(c);
            }
            if (c > U'\U0010ffff') {
                // out of range
                remaining = length = 1;
                return current = replacement_char;
            }
            if (length <= 0) {
                length = 2;
                if (remaining == first_unit) {
                    remaining = length;
                } else {
                    remaining = 1;
                }
            }
            c -= 0x10000;
            if (remaining == 2) {
                return current = static_cast<value_type>(c >> 10) | 0xD800;
            } else {
                return current = static_cast<value_type>(c & 0x3FF) | 0xDC00;
            }
        }

        pointer operator->() const
        {
            return &**this;
        }

        to_utf16_iterator& operator++()
        {
            if (length == first_unit) {
                // compute the length
                operator*();
            }
            if (length == last_unit || --remaining == 0) {
                ++pos;
                remaining = length = first_unit;
            }
            current = 0;
            return *this;
        }

        to_utf16_iterator operator++(int)
        {
            if (!current) {
                // ensure current is valid in copy
                operator*();
            }
            auto const it = *this;
            ++*this;
            return it;
        }

        to_utf16_iterator& operator--()
            requires std::is_base_of_v<std::bidirectional_iterator_tag, iterator_category>
        {
            if (length == last_unit) {
                // compute the length
                operator*();
            }
            if (length == first_unit || remaining++ == length) {
                --pos;
                remaining = length = last_unit;
            }
            current = 0;
            return *this;
        }

        to_utf16_iterator operator--(int)
            requires std::is_base_of_v<std::bidirectional_iterator_tag, iterator_category>
        {
            auto const it = *this;
            --*this;
            return it;
        }
    };

    template<std::input_iterator Input>
    requires std::is_same_v<typename std::iterator_traits<Input>::value_type, char32_t>
    class to_utf8_iterator
    {
    public:
        using iterator_category = typeutil::first_baseclass_t<typename std::iterator_traits<Input>::iterator_category,
                                                              std::bidirectional_iterator_tag,
                                                              std::forward_iterator_tag,
                                                              std::input_iterator_tag>;
        using difference_type = std::ptrdiff_t;
        using value_type = char8_t;
        using pointer = value_type const*;
        using reference = value_type;

    private:
        static constexpr unit_count first_unit = 0; // magic values for length and remaining
        static constexpr unit_count last_unit = -1;
        Input pos = {};          // current read position
        mutable unit_count length = first_unit; // number of UTF-8 units for this pos (<=0 means unknown)
        mutable unit_count remaining = first_unit; // number of UTF-8 units left (0 means all, -1 means last)
        mutable value_type current = 0;   // result of * operator

        auto compare_key() const
        { return std::tuple{ pos, remaining == length ? 0 : remaining }; }

    public:
        to_utf8_iterator() = default;
        explicit to_utf8_iterator(Input pos)
            : pos{pos}
        {}

        bool operator==(const to_utf8_iterator& other) const
        {
            return compare_key() == other.compare_key();
        }

        reference operator*() const
        {
            if (current) {
                return current;
            }
            auto c = *pos;
            if (c <= 0x7F) {
                // ascii
                remaining = length = 1;
                return current = static_cast<value_type>(c);
            }
            if (0xD800 <= c && c <= 0xDFFF) {
                // reject UTF-16 surrogates
                c = replacement_char;
            }
            if (length <= 0) {
                auto const backwards = length == last_unit;
                // compute the length
                auto mask = ~0x3Fu;
                length = 1;
                for (auto ch = c;  ch & mask;  ch >>= 6, mask >>= 1) {
                    ++length;
                }
                remaining = backwards ? 1 : length;
            }
            auto const shift = remaining - 1;
            if (remaining == length) {
                return current = static_cast<value_type>(c >> (6 * shift) | ~(0x7Fu >> shift) & 0xFF);
            } else {
                // continuation byte
                return current = (c >> (6 * shift)) & 0x3F | 0x80;
            }
        }

        to_utf8_iterator& operator++()
        {
            if (length == 0) {
                // compute the length
                operator*();
            }

            if (--remaining == 0) {
                ++pos;
                length = 0;
            }
            current = 0;
            return *this;
        }

        to_utf8_iterator operator++(int)
        {
            if (!current) {
                // ensure current is valid in copy
                operator*();
            }
            auto it = *this;
            ++*this;
            return it;
        }

        to_utf8_iterator& operator--()
            requires std::is_base_of_v<std::bidirectional_iterator_tag, iterator_category>
        {
            if (length == last_unit) {
                // compute the length
                operator*();
            }
            if (length == first_unit || remaining++ == length) {
                --pos;
                remaining = length = last_unit;
            }
            current = 0;
            return *this;
        }

        to_utf8_iterator operator--(int)
            requires std::is_base_of_v<std::bidirectional_iterator_tag, iterator_category>
        {
            auto const it = *this;
            --*this;
            return it;
        }
    };

    template<std::input_iterator Input>
    requires std::is_same_v<typename std::iterator_traits<Input>::value_type, char16_t>
    class from_utf16_iterator
    {
    public:
        using iterator_category = typeutil::first_baseclass_t<typename std::iterator_traits<Input>::iterator_category,
                                                              std::bidirectional_iterator_tag,
                                                              std::forward_iterator_tag,
                                                              std::input_iterator_tag>;
        using difference_type = std::ptrdiff_t;
        using value_type = char32_t;
        using pointer = value_type const*;
        using reference = value_type&;

    private:
        static constexpr bool is_bidirectional = std::is_base_of_v<std::bidirectional_iterator_tag, iterator_category>;
        mutable Input pos = {};          // current read position
        mutable value_type current = 0;
        mutable enum class charpos { before, only, first, second, last, after} state = charpos::before;

    public:
        from_utf16_iterator() = default;
        explicit from_utf16_iterator(Input pos)
            : pos{pos}
        {}

        bool operator==(const from_utf16_iterator& other) const { return pos == other.pos; }

        reference operator*() const
        {
            switch (state) {
            case charpos::only:
            case charpos::first:
            case charpos::second:
            case charpos::after:
                // we've already read the codepoint here
                return current;

            case charpos::before:
                current = static_cast<char16_t>(*pos);
                if ((current & 0xF800) != 0xD800) {
                    state = charpos::only;
                    current = *pos;
                    break;
                }
                if ((current & 0xFC00) == 0xD800) {
                    // it's an initial surrogate
                    value_type const c = static_cast<char16_t>(*++pos);
                    if ((c & 0xFC00) != 0xDC00) {
                        // not a continuation surrogate
                        state = charpos::after;
                        return current = replacement_char;
                    }
                    state = charpos::second;
                    return current = 0x10000 + ((current & 0x3FF) << 10) + (c & 0x3FF);
                }
                // else, an unexpected continuation surrogate
                state = charpos::only;
                return current = replacement_char;

            case charpos::last:
                if constexpr (is_bidirectional) {
                    current = static_cast<char16_t>(*pos);
                    if ((current & 0xF800) != 0xD800) {
                        state = charpos::only;
                        current = *pos;
                        break;
                    }
                    if ((*pos & 0xFC00) == 0xDC00) {
                        // it's a continuation surrogate
                        value_type const c = static_cast<char16_t>(*--pos);
                        if ((c & 0xFC00) != 0xD800) {
                            // not an initial surrogate
                            ++pos;
                            state = charpos::only;
                            return current = replacement_char;
                        }
                        state = charpos::first;
                        return current = 0x10000 + ((c & 0x3FF) << 10) + (current & 0x3FF);
                    }
                    // else, an unexpected initial surrogate
                    state = charpos::only;
                    return current = replacement_char;
                }
                std::unreachable();
            }

            if ((current & 0xFFFE) == 0xFFFE) {
                // reject problematic non-characters
                current = replacement_char;
            }
            return current;
        }

        from_utf16_iterator& operator++()
        {
            switch (state) {
            case charpos::before:  operator*(); operator++(); break;
            case charpos::only:  ++pos; break;
            case charpos::first:  ++pos; ++pos; break;
            case charpos::second:  ++pos; break;
            case charpos::last:  ++pos; break;
            case charpos::after:  break;
            }
            state = charpos::before;
            current = 0;
            return *this;
        }

        from_utf16_iterator operator++(int)
        {
            if (!current) {
                // ensure current is valid in copy
                operator*();
            }
            auto it = *this;
            ++*this;
            return it;
        }

        from_utf16_iterator& operator--()
            requires is_bidirectional
        {
            switch (state) {
            case charpos::before:  --pos; break;
            case charpos::only:  --pos; break;
            case charpos::first:  --pos; break;
            case charpos::second:  --pos; --pos; break;
            case charpos::last:  operator*(); operator--(); break;
            case charpos::after:  --pos; break;
            }
            state = charpos::last;
            current = 0;
            return *this;
        }

        from_utf16_iterator operator--(int)
            requires is_bidirectional
        {
            auto const it = *this;
            --*this;
            return it;
        }
    };


    template<std::input_iterator Input>
    requires std::is_same_v<typename std::iterator_traits<Input>::value_type, char8_t>
    class from_utf8_iterator
    {
    public:
        using iterator_category = typeutil::first_baseclass_t<typename std::iterator_traits<Input>::iterator_category,
                                                              std::bidirectional_iterator_tag,
                                                              std::forward_iterator_tag,
                                                              std::input_iterator_tag>;
        using difference_type = std::ptrdiff_t;
        using value_type = char32_t;
        using pointer = value_type const*;
        using reference = value_type;

    private:
        static constexpr bool is_bidirectional = std::is_base_of_v<std::bidirectional_iterator_tag, iterator_category>;
        Input pos = {};          // current read position
        mutable value_type current = 0;

    public:
        from_utf8_iterator() = default;
        explicit from_utf8_iterator(Input pos)
            : pos{pos}
        {}

        bool operator==(const from_utf8_iterator&) const = default;

        reference operator*() const
        {
            if (current) {
                return current;
            }
            auto p = pos;
            char32_t c = static_cast<char8_t>(*p);
            auto count = 0u;
            for (auto mask = 0x80u;  mask & c;  mask >>= 1) {
                c &= ~mask;
                ++count;
            }
            if (count == 0) {
                // ASCII
                return current = c;
            }
            if (count == 1 || count > 6) {
                return current = replacement_char;
            }
            // minimum code-point value for given length
            constexpr char32_t minima[7]{ 0, 0, 0x80, 0x800, 0x1000, 0x20000, 0x4000000 };
            char32_t const minval = minima[count];
            while (--count > 0 && (*++p & 0xC0u) == 0x80u) {
                c <<= 6;
                c |= static_cast<char8_t>(*p) & 0x3F;
            }
            if (count > 0) {
                // missing continuation
                return current = replacement_char;
            }
            if (c < minval) {
                // reject overlong encodings
                return current = replacement_char;
            }
            if (0xD800 <= c && c <= 0xDFFF) {
                // reject UTF-16 surrogates
                return current = replacement_char;
            }
            if ((c & 0xFFFE) == 0xFFFE) {
                // reject problematic non-characters
                return current = replacement_char;
            }
            return current = c;
        }

        from_utf8_iterator& operator++()
        {
            auto count = 0;
            for (auto mask = 0x80u;  mask & *pos;  mask >>= 1) {
                ++count;
            }
            if (count == 0 || count > 6) {
                // ASCII or invalid - just advance by a single byte
                count = 1;
            }
            while (count-- > 0 && (*++pos & 0xC0) == 0x80) {
                ;               // skip continuation bytes (only)
            }
            current = 0;
            return *this;
        }

        from_utf8_iterator operator++(int)
        {
            if (!current) {
                // ensure current is valid in copy
                operator*();
            }
            auto it = *this;
            ++*this;
            return it;
        }

        from_utf8_iterator& operator--()
            requires is_bidirectional
        {
            while ((*--pos & 0xC0) == 0x80) {
                // just decrement some more
            }
            current = 0;
            return *this;
        }

        from_utf8_iterator operator--(int)
            requires is_bidirectional
        {
            if (!current) {
                // ensure current is valid in copy
                operator*();
            }
            auto const it = *this;
            --*this;
            return it;
        }
    };

    // Views

    template<template<typename> class T>
    struct wrapped_iterator_view_factory
        : std::ranges::range_adaptor_closure<wrapped_iterator_view_factory<T>>
    {
        template<std::ranges::input_range Range>
        auto operator()(Range const &range) const
        {
            //using iterator = T<std::ranges::iterator_t<Range const>>;
            return std::ranges::subrange{T{std::ranges::begin(range)},
                                         T{std::ranges::end(range)}};
        }
    };

    constexpr wrapped_iterator_view_factory<to_utf16_iterator> as_utf16;
    constexpr wrapped_iterator_view_factory<to_utf8_iterator> as_utf8;

    constexpr wrapped_iterator_view_factory<from_utf8_iterator> from_utf8;

    // Functions

    template<std::ranges::input_range R>
    requires std::convertible_to<typename std::ranges::range_value_t<R>, char32_t>
    and (!std::is_array_v<std::remove_reference_t<R>>)
    auto to_u16string(R&& range) {
        return range | as_utf16 | std::ranges::to<std::u16string>();
    }
    // For string literals, we don't want to include the trailing \0
    template<std::size_t N>
    auto to_u16string(const char32_t(&s)[N]) {
        return to_u16string(std::u32string_view(s, N-1));
    }

    template<std::ranges::input_range R>
    requires std::convertible_to<typename std::ranges::range_value_t<R>, char32_t>
    and (!std::is_array_v<std::remove_reference_t<R>>)
    auto to_u8string(R&& range) {
        return range | as_utf8 | std::ranges::to<std::u8string>();
    }
    // For string literals, we don't want to include the trailing \0
    template<std::size_t N>
    auto to_u8string(const char32_t(&s)[N]) {
        return to_u8string(std::u32string_view(s, N-1));
    }

    template<std::ranges::input_range R>
    requires std::is_same_v<std::decay_t<typename std::ranges::range_value_t<R>>, char8_t>
    and (!std::is_array_v<std::remove_reference_t<R>>)
    auto to_u32string(R&& range) {
        auto s = std::ranges::subrange(from_utf8_iterator{std::ranges::begin(range)},
                                       from_utf8_iterator{std::ranges::end(range)});
        return s | std::ranges::to<std::u32string>();
    }
    // For string literals, we don't want to include the trailing \0
    template<std::size_t N>
    auto to_u32string(const char8_t(&s)[N]) {
        return to_u32string(std::u8string_view(s, N-1));
    }

    template<std::ranges::input_range R>
    requires std::is_same_v<std::decay_t<typename std::ranges::range_value_t<R>>, char16_t>
    and (not std::is_array_v<std::remove_reference_t<R>>)
    auto to_u32string(R&& range) {
        auto s = std::ranges::subrange(from_utf16_iterator{std::ranges::begin(range)},
                                       from_utf16_iterator{std::ranges::end(range)});
        return s | std::ranges::to<std::u32string>();
    }
    // For string literals, we don't want to include the trailing \0
    template<std::size_t N>
    auto to_u32string(const char16_t(&s)[N]) {
        return to_u32string(std::u16string_view(s, N-1));
    }
}

#ifdef USING_GTEST

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>


// To UTF-16

TEST(to_utf16_iterator, bmp)
{
    auto *s = U"abà£";
    codecvt::to_utf16_iterator i{s};
    EXPECT_EQ(*i, u'a');
    EXPECT_EQ(*i++, u'a');
    EXPECT_EQ(*i++, u'b');
    EXPECT_EQ(*i++, u'à');
    EXPECT_EQ(*i++, u'£');
    EXPECT_EQ(*i, u'\0');
}

TEST(to_utf16_iterator, supplementary)
{
    auto *s = U"\U00024B62";
    codecvt::to_utf16_iterator i{s};
    EXPECT_EQ(*i, 0xD852);
    EXPECT_EQ(*i++, 0xD852);
    EXPECT_EQ(*i++, 0xDF62);
    EXPECT_EQ(*i, u'\0');

    // skip first surrogate
    i = codecvt::to_utf16_iterator{s};
    EXPECT_EQ(*++i, 0xDF62);
    EXPECT_EQ(*++i, u'\0');
}

TEST(to_utf16_iterator, too_big)
{
    char32_t s[]{ 0x110000, 0 };
    codecvt::to_utf16_iterator i{s};
    EXPECT_EQ(*i, 0xFFFD);
    EXPECT_EQ(*i++, 0xFFFD);
    EXPECT_EQ(*i, u'\0');
}

TEST(to_utf16_iterator, surrogates)
{
    char32_t s[]{ 0xD800, 0xDFFF, 0 };
    codecvt::to_utf16_iterator i{s};
    EXPECT_EQ(*i++, 0xFFFD);
    EXPECT_EQ(*i++, 0xFFFD);
    EXPECT_EQ(*i, u'\0');
}

TEST(to_utf16_iterator, equality)
{
    // iterator to the same UTF-16 unit should be equal whether or not
    // it's dereferenced.
    auto *s = U"a\U00024B62";
    codecvt::to_utf16_iterator i0{s}; // advance only
    auto i1 = i0;                     // advance and dereference

#define EXPECT_NEXT(ch) do {                    \
        EXPECT_TRUE(i0 == i1);                  \
        EXPECT_EQ(*i1, ch);                     \
        EXPECT_TRUE(i0 == i1);                  \
        ++i0, ++i1;                             \
    } while (0)

    EXPECT_NEXT(u'a');
    EXPECT_NEXT(0xD852);
    EXPECT_NEXT(0xDF62);
    EXPECT_NEXT(0);
}

TEST(to_utf16_iterator, decrement)
{
    auto const& s = U"\U00024B62" "abà£";
    {
        auto i = codecvt::to_utf16_iterator{std::ranges::cend(s)};
        EXPECT_EQ(*--i, u'\0');
        EXPECT_EQ(*--i, u'£');
        EXPECT_EQ(*--i, u'à');
        EXPECT_EQ(*--i, u'b');
        EXPECT_EQ(*--i, u'a');
        EXPECT_EQ(*--i, 0xDF62);
        EXPECT_EQ(*--i, 0xD852);
    }
}

TEST(to_utf16_iterator, input_stream_preincrement)
{
    auto s = std::basic_istringstream<char32_t>{U"\U00024B62" "abà£"};
    {
        auto i = codecvt::to_utf16_iterator{std::istreambuf_iterator{s}};
        EXPECT_EQ(*i, 0xD852);
        EXPECT_EQ(*++i, 0xDF62);
        EXPECT_EQ(*++i, u'a');
        EXPECT_EQ(*++i, u'b');
        EXPECT_EQ(*++i, u'à');
        EXPECT_EQ(*++i, u'£');
    }
}

TEST(to_utf16_iterator, input_stream_postincrement)
{
    auto s = std::basic_istringstream<char32_t>{U"\U00024B62" "abà£"};
    {
        auto i = codecvt::to_utf16_iterator{std::istreambuf_iterator{s}};
        auto c = *i++;
        EXPECT_EQ(c, 0xD852);
        c = *i++;
        EXPECT_EQ(c, 0xDF62);
        EXPECT_EQ(*i++, u'a');
        EXPECT_EQ(*i++, u'b');
        EXPECT_EQ(*i++, u'à');
        EXPECT_EQ(*i++, u'£');
    }
}

// To UTF-8

TEST(to_utf8_iterator, ascii)
{
    auto *s = U"ab";
    codecvt::to_utf8_iterator i{s};
    EXPECT_EQ(*i, u8'a');
    EXPECT_EQ(*i, u8'a');
    EXPECT_EQ(*++i, u8'b');
    EXPECT_EQ(*++i, u8'\0');
}

TEST(to_utf8_iterator, latin1)
{
    auto *s = U"©";
    codecvt::to_utf8_iterator i{s};
    EXPECT_EQ(*i, 0xC2);
    EXPECT_EQ(*i, 0xC2);
    ++i;
    EXPECT_EQ(*i, 0xA9);
    EXPECT_EQ(*i, 0xA9);
    ++i;
    EXPECT_EQ(*i, 0x00);

    // prove it works even when we don't dereference
    codecvt::to_utf8_iterator j{s};
    ++j;
    EXPECT_EQ(*j, 0xA9);

    EXPECT_EQ(codecvt::to_u8string(U"à£"), u8"à£");
}

TEST(to_utf8_iterator, general)
{
    EXPECT_EQ(codecvt::to_u8string(U"你好 👋 ᜃᜓᜋᜓᜐ᜔ᜆ"), u8"你好 👋 ᜃᜓᜋᜓᜐ᜔ᜆ");
}

TEST(to_utf8_iterator, surrogates)
{
    char32_t const s[]{ 0xD800, 0xDFFF, 0 };
    EXPECT_EQ(codecvt::to_u8string(s), u8"\uFFFD\uFFFD");
}

TEST(to_utf8_iterator, equality)
{
    auto *const s = U"a©👋";
    codecvt::to_utf8_iterator i0{s}; // advance only
    auto i1 = i0;                     // advance and dereference

#define EXPECT_NEXT(ch) do {                    \
        EXPECT_TRUE(i0 == i1);                  \
        EXPECT_EQ(*i1, ch);                     \
        EXPECT_TRUE(i0 == i1);                  \
        ++i0, ++i1;                             \
    } while (0)

    EXPECT_NEXT(0x61);

    EXPECT_NEXT(0xC2);
    EXPECT_NEXT(0xA9);

    EXPECT_NEXT(0xF0);
    EXPECT_NEXT(0x9F);
    EXPECT_NEXT(0x91);
    EXPECT_NEXT(0x8B);
    EXPECT_NEXT(0);

#undef EXPECT_NEXT
}

TEST(to_utf8_iterator, decrement)
{
    auto const& s = U"a©👋";
    {
        auto i = codecvt::to_utf8_iterator{std::ranges::cend(s)};
        EXPECT_EQ(*--i, u'\0');

        EXPECT_EQ(*--i, 0x8B);
        EXPECT_EQ(*--i, 0x91);
        EXPECT_EQ(*--i, 0x9F);
        EXPECT_EQ(*--i, 0xF0);

        EXPECT_EQ(*--i, 0xA9);
        EXPECT_EQ(*--i, 0xC2);

        EXPECT_EQ(*--i, 0x61);
    }
}

TEST(to_utf8_iterator, input_stream)
{
    auto s = std::basic_istringstream<char32_t>{U"a©👋b"};
    {
        auto i = codecvt::to_utf8_iterator{std::istreambuf_iterator{s}};
        EXPECT_EQ(*i++, u8'a');

        EXPECT_EQ(*i++, 0xC2);
        EXPECT_EQ(*i++, 0xA9);

        EXPECT_EQ(*i++, 0xF0);
        EXPECT_EQ(*i++, 0x9F);
        EXPECT_EQ(*i++, 0x91);
        EXPECT_EQ(*i++, 0x8B);

        EXPECT_EQ(*i++, u8'b');
    }
}

// From UTF-16

// helper for making invalid UTF-16 sequences from hex codes
static std::u16string u16string_from_hex(std::string in)
{
    std::istringstream is{in};
    std::u16string us;
    us.reserve(in.length() / 3 + 1);
    for (unsigned c;  is >> std::hex >> c; ) {
        us.push_back(static_cast<char16_t>(c));
    }
    return us;
}

TEST(from_utf16_iterator, ascii)
{
    auto *s = u"ab";
    codecvt::from_utf16_iterator i{s};
    EXPECT_EQ(*i, U'a');
    EXPECT_EQ(*i, U'a');
    EXPECT_EQ(*++i, U'b');
    EXPECT_EQ(*++i, U'\0');
}

TEST(from_utf16_iterator, latin1)
{
    auto *s = u"à£";
    codecvt::from_utf16_iterator i{s};
    EXPECT_EQ(*i, U'à');
    EXPECT_EQ(*i, U'à');
    EXPECT_EQ(*++i, U'£');
    EXPECT_EQ(*++i, U'\0');

    // prove it works even when we don't dereference
    i = codecvt::from_utf16_iterator{s};
    ++i;
    EXPECT_EQ(*i, U'£');
}

TEST(from_utf16_iterator, greek)
{
    auto *s = u"κόσμε";
    codecvt::from_utf16_iterator i{s};
    EXPECT_EQ(*i++, U'κ');
    EXPECT_EQ(*i++, U'ό');
    EXPECT_EQ(*i++, U'σ');
    EXPECT_EQ(*i++, U'μ');
    EXPECT_EQ(*i++, U'ε');
    EXPECT_EQ(*i, U'\0');
}

TEST(from_utf16_iterator, emoji)
{
    auto *s = u"👋";
    codecvt::from_utf16_iterator i{s};
    EXPECT_EQ(*i, U'👋');
    EXPECT_EQ(*++i, U'\0');
}

TEST(from_utf16_iterator, general)
{
    EXPECT_EQ(codecvt::to_u32string(u"你好 👋 ᜃᜓᜋᜓᜐ᜔ᜆ"), U"你好 👋 ᜃᜓᜋᜓᜐ᜔ᜆ");
}

TEST(from_utf16_iterator, decrement)
{
    auto const& s = u"a©👋";
    {
        auto i = codecvt::from_utf16_iterator{std::ranges::cend(s)};
        EXPECT_EQ(*--i, U'\0');
        EXPECT_EQ(*--i, U'👋');
        EXPECT_EQ(*--i, U'©');
        EXPECT_EQ(*--i, U'a');
    }
}

TEST(from_utf16_iterator, first_per_len)
{
    // We don't use 0xffff because that should be rejected
    char32_t s[] = { 0, 0x1000, 0 };
    EXPECT_EQ(codecvt::to_u32string(codecvt::to_u16string(s)), std::u32string_view(s, std::size(s) - 1));
}

TEST(from_utf16_iterator, last_per_len)
{
    // We don't use 0xffff because that should be rejected
    char32_t s[] = { 0xfffc, 0x10FFFF, 0 };
    EXPECT_EQ(codecvt::to_u32string(codecvt::to_u16string(s)), s);
}

TEST(from_utf16_iterator, unexpected_continuation)
{
    EXPECT_EQ(codecvt::to_u32string(u16string_from_hex("dc00 dfff")), U"\uFFFD\uFFFD");
}

TEST(from_utf16_iterator, missing_continuation)
{
    EXPECT_EQ(codecvt::to_u32string(u16string_from_hex("d800 20")), U"\uFFFD ");
    EXPECT_EQ(codecvt::to_u32string(u16string_from_hex("dbff 20")), U"\uFFFD ");
}

TEST(from_utf16_iterator, input_stream)
{
    auto s = std::basic_istringstream<char16_t>{u"aà£👋b"};
    {
        auto i = codecvt::from_utf16_iterator{std::istreambuf_iterator{s}};
        EXPECT_EQ(*i++, U'a');
        EXPECT_EQ(*i++, U'à');
        EXPECT_EQ(*i++, U'£');
        EXPECT_EQ(*i++, U'👋');
        EXPECT_EQ(*i++, U'b');
    }
}

// From UTF-8

// helper for making invalid UTF-8 sequences from hex codes
static std::u8string u8string_from_hex(std::string in)
{
    std::istringstream is{in};
    std::u8string us;
    us.reserve(in.length() / 3 + 1);
    for (unsigned c;  is >> std::hex >> c; ) {
        us.push_back(static_cast<char8_t>(c));
    }
    return us;
}

TEST(from_utf8_iterator, ascii)
{
    auto *s = u8"ab";
    codecvt::from_utf8_iterator i{s};
    EXPECT_EQ(*i, U'a');
    EXPECT_EQ(*i, U'a');
    EXPECT_EQ(*++i, U'b');
    EXPECT_EQ(*++i, U'\0');
}

TEST(from_utf8_iterator, latin1)
{
    auto *s = u8"à£";
    codecvt::from_utf8_iterator i{s};
    EXPECT_EQ(*i, U'à');
    EXPECT_EQ(*i, U'à');
    EXPECT_EQ(*++i, U'£');
    EXPECT_EQ(*++i, U'\0');

    // prove it works even when we don't dereference
    i = codecvt::from_utf8_iterator{s};
    ++i;
    EXPECT_EQ(*i, U'£');
}

TEST(from_utf8_iterator, greek)
{
    auto *s = u8"κόσμε";
    codecvt::from_utf8_iterator i{s};
    EXPECT_EQ(*i++, U'κ');
    EXPECT_EQ(*i++, U'ό');
    EXPECT_EQ(*i++, U'σ');
    EXPECT_EQ(*i++, U'μ');
    EXPECT_EQ(*i++, U'ε');
    EXPECT_EQ(*i, U'\0');
}

TEST(from_utf8_iterator, emoji)
{
    auto *s = u8"👋";
    codecvt::from_utf8_iterator i{s};
    EXPECT_EQ(*i, U'👋');
    EXPECT_EQ(*++i, U'\0');
}

TEST(from_utf8_iterator, general)
{
    EXPECT_EQ(codecvt::to_u32string(u8"你好 👋 ᜃᜓᜋᜓᜐ᜔ᜆ"), U"你好 👋 ᜃᜓᜋᜓᜐ᜔ᜆ");
}

TEST(from_utf8_iterator, decrement)
{
    auto const& s = u8"a©👋";
    {
        auto i = codecvt::from_utf8_iterator{std::ranges::cend(s)};
        EXPECT_EQ(*--i, U'\0');
        EXPECT_EQ(*--i, U'👋');
        EXPECT_EQ(*--i, U'©');
        EXPECT_EQ(*--i, U'a');
    }
}

TEST(from_utf8_iterator, first_per_len)
{
    // We don't use 0xffff because that should be rejected
    char32_t s[] = { 0, 0x80, 0x800, 0x1000, 0x20000, 0x4000000, 0 };
    EXPECT_EQ(codecvt::to_u32string(codecvt::to_u8string(s)), std::u32string_view(s, std::size(s) - 1));
}

TEST(from_utf8_iterator, last_per_len)
{
    // We don't use 0xffff because that should be rejected
    char32_t s[] = { 0x7f, 0x7ff, 0xfffc, 0x1FFFFD, 0x3FFFFFD, 0x7FFFFFFD, 0 };
    EXPECT_EQ(codecvt::to_u32string(codecvt::to_u8string(s)), s);
}

TEST(from_utf8_iterator, unexpected_continuation)
{
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("80 bf")), U"\uFFFD\uFFFD");
}

TEST(from_utf8_iterator, missing_continuation)
{
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("c0 20")), U"\uFFFD ");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("df 20")), U"\uFFFD ");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("fd 20")), U"\uFFFD ");
}

TEST(from_utf8_iterator, missing_last_continuation)
{
    // '/' should be representable only as 0x2f
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("c0")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("e0 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f0 80 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f8 80 80 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("fc 80 80 80 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("c1")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("e0 9f")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f0 8f bf")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f8 87 bf bf")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("fc 83 bf bf bf")), U"\uFFFD");
}

TEST(from_utf8_iterator, invalid_bytes)
{
    // fe and ff do not introduce very long sequences
    {
        auto const s = u8string_from_hex("fe 81 80 80 80 80 80");
        codecvt::from_utf8_iterator i{s.begin()};
        EXPECT_EQ(*i, U'\uFFFD');
    }
    {
        auto const s = u8string_from_hex("ff 81 80 80 80 80 80 80");
        codecvt::from_utf8_iterator i{s.begin()};
        EXPECT_EQ(*i, U'\uFFFD');
    }
}

TEST(from_utf8_iterator, overlong_ascii)
{
    // '/' should be representable only as 0x2f
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("c0 af")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("e0 80 af")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f0 80 80 af")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f8 80 80 80 af")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("fc 80 80 80 80 af")), U"\uFFFD");
}

TEST(from_utf8_iterator, overlong_max)
{
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("c1 bf")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("e0 9f bf")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f0 8f bf bf")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f8 87 bf bf bf")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("fc 83 bf bf bf bf")), U"\uFFFD");
}

TEST(from_utf8_iterator, overlong_nul)
{
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("c0 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("e0 80 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f0 80 80 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("f8 80 80 80 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("fc 80 80 80 80 80")), U"\uFFFD");
}

TEST(from_utf8_iterator, surrogates)
{
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("ed a0 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("ed ad bf")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("ed b0 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("ed be 80")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("ed bf bf")), U"\uFFFD");
}

TEST(from_utf8_iterator, noncharacters)
{
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("ef bf be")), U"\uFFFD");
    EXPECT_EQ(codecvt::to_u32string(u8string_from_hex("ef bf bf")), U"\uFFFD");
}

TEST(from_utf8_iterator, input_stream)
{
    auto s = std::basic_istringstream<char8_t>{u8"aà£👋b"};
    {
        auto i = codecvt::from_utf8_iterator{std::istreambuf_iterator{s}};
        EXPECT_EQ(*i++, U'a');
        EXPECT_EQ(*i++, U'à');
        EXPECT_EQ(*i++, U'£');
        EXPECT_EQ(*i++, U'👋');
        EXPECT_EQ(*i++, U'b');
    }
}

#else

#include <algorithm>
#include <iostream>
#include <iterator>
#include <ranges>

template<typename T>
constexpr auto cast_view =
    std::views::transform([](auto x) -> T { return static_cast<T>(x); });

#include <sstream>
int main()
{
    for (std::string line;  std::getline(std::cin, line);  ) {
        std::ranges::copy(line | cast_view<char8_t> | codecvt::from_utf8
                          | codecvt::as_utf8 | cast_view<char>,
                          std::ostreambuf_iterator<char>(std::cout));
        std::cout << '\n';
    }
}

#endif
