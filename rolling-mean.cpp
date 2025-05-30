#include <cmath>
#include <concepts>
#include <deque>
#include <stdexcept>
#include <optional>
#include <utility>

bool is_valid(auto const&) { return true; }
bool is_valid(std::floating_point auto const& v) { return std::isfinite(v); }
// Customisation point: user can add overloads of is_valid()
template<typename T>
bool is_valid(std::optional<T> const& v) { return v and is_valid(*v); }


template<typename T>
class rolling_mean
{
    using V = std::optional<T>;

    std::deque<V> content;
    std::size_t threshold;

    T mean = 0;
    std::size_t valid_count = 0;

public:
    // threshold==0 means use a sensible default (half of size)
    explicit rolling_mean(std::size_t size, std::size_t threshold = 0)
        : content(size),
          threshold{threshold ? threshold : (size + 1) / 2}
    {
        if (!size) {
            throw std::invalid_argument{"Size must be at least 1"};
        }
        if (threshold > size) {
            throw std::invalid_argument{"Threshold can never be reached"};
        }
    }

    V value() const
    {
        if (valid_count < threshold) {
            return {};
        }
        return mean;
    }

    void push_back(V value)
    {
        // For the arithmetic of updating incremental mean, see
        // https://fanf2.user.srcf.net/hermes/doc/antiforgery/stats.pdf
        const auto& old = content.front();
        if (is_valid(old) && --valid_count) { // N.B. only decrement if old was valid
            mean -= (*old - mean) / valid_count;
        }
        content.pop_front();
        content.emplace_back(std::move(value));
        if (is_valid(value)) {
            mean += (*value - mean) / ++valid_count;
        }
    }
};


#ifdef USING_GTEST

#include <gtest/gtest.h>

#include <format>

TEST(empty, no_result)
{
    auto m = rolling_mean<double>{5};
    EXPECT_EQ(m.value(), std::nullopt);
}

TEST(five_values, expect_mean)
{
    auto m = rolling_mean<double>{5};
    for (auto i: {1, 2, 3, 4, 5}) {
        m.push_back(i);
    }
    EXPECT_DOUBLE_EQ(m.value().value(), 3.0);
}

TEST(six_values, expect_mean)
{
    // This test ensures that value 1 is removed correctly.
    auto m = rolling_mean<double>{5};
    for (auto i: {1, 2, 3, 4, 5, 6}) {
        m.push_back(i);
    }
    EXPECT_DOUBLE_EQ(m.value().value(), 4.0);
}

TEST(many_values, expect_mean)
{
    // Test for numerical stability
    auto m = rolling_mean<double>{5};
        for (auto i: {1, 2, 3, 4, 5}) {
            m.push_back(i);
        }
    for (int j = 0;  j < 100'000;  ++j) {
        for (auto i: {1, 2, 3, 4, 5}) {
            SCOPED_TRACE(std::format("iteration {}, value {}", j, i));
            m.push_back(i);
            ASSERT_NE(m.value(), std::nullopt);
            ASSERT_DOUBLE_EQ(m.value().value(), 3.0);
        }
    }
}

TEST(two_bad_values, expect_mean)
{
    // Ensure missing values are not included
    auto m = rolling_mean<double>{5};
    m.push_back({});
    m.push_back(3);
    m.push_back(4);             // start of window
    m.push_back(5);
    m.push_back({});
    m.push_back({});
    m.push_back(6);
    EXPECT_DOUBLE_EQ(m.value().value(), 5.0);
}

TEST(three_bad_values, no_result)
{
    // Too many missing values
    auto m = rolling_mean<double>{5};
    m.push_back({});
    m.push_back(4);
    m.push_back(5);             // start of window
    m.push_back({});
    m.push_back({});
    m.push_back(6);
    m.push_back({});
    EXPECT_EQ(m.value(), std::nullopt);
}

#endif
