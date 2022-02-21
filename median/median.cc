#include "median.hh"

#include <gtest/gtest.h>
#include <array>
#include <forward_list>
#include <stdexcept>
#include <vector>

namespace test
{
    struct moveonly_int
    {
        int value;

        moveonly_int(int i) : value{i} {}
        moveonly_int(const moveonly_int&) = delete;
        moveonly_int(moveonly_int&&) = default;
        void operator=(const moveonly_int&) = delete;
        moveonly_int& operator=(moveonly_int&&) = default;

        bool operator<(const moveonly_int& other) const
        { return value < other.value; }
    };

    // specific midpoint for this type, to be found by ADL
    double midpoint(const moveonly_int& a, const moveonly_int& b)
    {
        return b.value - a.value; // the name is a lie
    }

    struct nocopy_int
    {
        int value;

        nocopy_int(int i) : value{i} {}
        nocopy_int(const nocopy_int&) = delete;
        void operator=(const nocopy_int&) = delete;

        bool operator<(const nocopy_int& other) const
        { return value < other.value; }
    };

    // specific midpoint for this type, to be found by ADL
    double midpoint(const nocopy_int& a, const nocopy_int& b)
    {
        return a.value + b.value; // the name is a lie
    }

    template<typename T>
    struct expect_midpoint {
        const T expected_a;
        const T expected_b;
        void operator()(T const& actual_a, T const& actual_b) const
        {
            EXPECT_EQ(expected_a, actual_a);
            EXPECT_EQ(expected_b, actual_b);
        }
    };

    struct dummy_midpoint
    {
        auto operator()(auto&&, auto&&) const {}
    };

    struct invalid_strategy
    {
        template<std::ranges::forward_range Range,
                 typename Comp,
                 typename Proj,
                 typename Midpoint>
        auto operator()(Range&&, Comp, Proj, Midpoint) const
            -> stats::median_result_t<Range, Proj, Midpoint>
        {
            throw std::logic_error("should not be called");
        }
    };

}

// C++20 version of detection idiom
template<typename Func, typename... Args>
constexpr bool can_call(Func&&, Args&&...)
    requires std::invocable<Func, Args...>
{ return true; }

template<typename... Args>
constexpr bool can_call(Args&&...)
{ return false; }

template<typename Strategy, typename Range>
concept strategy_accepts_type =
    std::invocable<Strategy, Range, std::less<>, std::identity, test::dummy_midpoint>;

enum strategy_mask : unsigned {
    sm_inplace        = 0x01,
    sm_inplace_rvalue = 0x02,
    sm_copy           = 0x04,
    sm_external       = 0x08,
    sm_default        = 0x10,
    sm_frugal         = 0x20,

    sm_none =  0u,
    sm_all  = ~0u,
};

constexpr auto operator+(strategy_mask a, strategy_mask b)
{ return static_cast<strategy_mask>(static_cast<unsigned>(a) | static_cast<unsigned>(b)); }
constexpr auto operator-(strategy_mask a, strategy_mask b)
{ return static_cast<strategy_mask>(static_cast<unsigned>(a) & ~static_cast<unsigned>(b)); }
constexpr bool is_set(strategy_mask a, strategy_mask b)
{ return a & b; }

template<typename Range>
void expect_usable(strategy_mask m = 0)
{
    if (!is_set(m, sm_inplace)) {
        // if we can't inplace, then we certainly can't inplace-rvalue
        m = m - sm_inplace_rvalue;
    }
    EXPECT_EQ(is_set(m, sm_inplace), (strategy_accepts_type<stats::inplace_strategy, Range>));
    EXPECT_EQ(is_set(m, sm_inplace_rvalue), (strategy_accepts_type<stats::inplace_strategy_rvalues_only, Range>));
    EXPECT_EQ(is_set(m, sm_copy), (strategy_accepts_type<stats::copy_strategy, Range>));
    EXPECT_EQ(is_set(m, sm_external), (strategy_accepts_type<stats::external_strategy, Range>));
    EXPECT_EQ(is_set(m, sm_default), (strategy_accepts_type<stats::default_strategy, Range>));
    EXPECT_EQ(is_set(m, sm_frugal), (strategy_accepts_type<stats::frugal_strategy, Range>));
}

// Tests of callability
// (Could be static, but we get better diagnostics this way)

TEST(Strategies, Regular)
{
    using Range = std::vector<int>;
    {
        SCOPED_TRACE("pass by value\n");
        expect_usable<Range>(sm_all);
    }
    {
        SCOPED_TRACE("pass by ref\n");
        expect_usable<Range&>(sm_all - sm_inplace_rvalue);
    }
    {
        SCOPED_TRACE("pass by const ref\n");
        expect_usable<Range const&>(sm_all - sm_inplace);
    }
    {
        SCOPED_TRACE("pass by rvalue\n");
        expect_usable<Range&&>(sm_all);
    }
}

TEST(Strategies, MoveOnly)
{
    using Range = std::vector<test::moveonly_int>;
    // can't be copied
    {
        SCOPED_TRACE("pass by value\n");
        expect_usable<Range>(sm_all - sm_copy);
    }
    {
        SCOPED_TRACE("pass by ref\n");
        expect_usable<Range&>(sm_all - sm_copy - sm_inplace_rvalue);
    }
    {
        SCOPED_TRACE("pass by const ref\n");
        expect_usable<Range const&>(sm_external + sm_default + sm_frugal);
    }
    {
        SCOPED_TRACE("pass by rvalue\n");
        expect_usable<Range&&>(sm_all - sm_copy);
    }
}

TEST(Strategies, NoCopy)
{
    using Range = std::vector<test::nocopy_int>;
    {
        SCOPED_TRACE("pass by value\n");
        expect_usable<Range>(sm_all - sm_inplace - sm_copy);
    }
    {
        SCOPED_TRACE("pass by ref\n");
        expect_usable<Range&>(sm_all - sm_inplace - sm_copy);
    }
    {
        SCOPED_TRACE("pass by const ref\n");
        expect_usable<Range const&>(sm_all - sm_inplace - sm_copy);
    }
    {
        SCOPED_TRACE("pass by rvalue\n");
        expect_usable<Range&&>(sm_all - sm_inplace - sm_copy);
    }
}

TEST(Strategies, ForwardOnly)
{
    using Range = std::forward_list<int>;
    {
        SCOPED_TRACE("pass by value\n");
        expect_usable<Range>(sm_all - sm_inplace);
    }
    {
        SCOPED_TRACE("pass by ref\n");
        expect_usable<Range&>(sm_all - sm_inplace);
    }
    {
        SCOPED_TRACE("pass by const ref\n");
        expect_usable<Range const&>(sm_all - sm_inplace);
    }
    {
        SCOPED_TRACE("pass by rvalue\n");
        expect_usable<Range&&>(sm_all - sm_inplace);
    }
}

TEST(Strategies, FilteredView)
{
    using View = std::ranges::filter_view<std::ranges::ref_view<int[1]>, std::function<bool(int)>>;
    {
        SCOPED_TRACE("pass by value\n");
        expect_usable<View>(sm_all - sm_inplace);
    }
    {
        SCOPED_TRACE("pass by ref\n");
        expect_usable<View&>(sm_all - sm_inplace);
    }
    {
        SCOPED_TRACE("pass by const ref\n");
        // const view isn't a range - needs median_engine to copy it
        expect_usable<View const&>(sm_none);
    }
    {
        SCOPED_TRACE("pass by rvalue\n");
        expect_usable<View&&>(sm_all - sm_inplace);
    }
}


// Don't even try compiling the rest unless earlier tests succeed!
#ifndef TYPE_TESTS_FAILED

static void test_strategy(auto const& m, auto&& range, auto&& name = "")
{
    SCOPED_TRACE(name);
    m(std::forward<decltype(range)>(range));
}

// Use this one for tests where the engine should not call out to strategy.
// I.e. when input is ordered, or there's only 1 or 2 elements.
template<std::ranges::forward_range Container = std::vector<int>,
         typename Midpoint = test::expect_midpoint<std::ranges::range_value_t<Container>>,
         typename Comp = std::less<>, typename Proj = std::identity>
static void test_values_trivial(Container&& values, Midpoint expected,
                                Comp compare = {}, Proj projection = {})
{
    auto const m = stats::median
        .using_compare(compare)
        .using_projection(projection)
        .using_midpoint(expected)
        .using_strategy(stats::shortcircuit_sorted{test::invalid_strategy{}}); // will fail if called

    test_strategy(m, std::forward<Container>(values), "trivial");
}

template<std::ranges::forward_range Container = std::vector<int>,
         typename Midpoint = test::expect_midpoint<std::ranges::range_value_t<Container>>,
         typename Comp = std::less<>, typename Proj = std::identity>
static void test_values_const_input(const Container& values, Midpoint expected,
                                    Comp compare = {}, Proj projection = {})
{
    auto const m = stats::median
        .using_compare(compare)
        .using_projection(projection)
        .using_midpoint(expected);

    test_strategy(m, values, "default strategy");
    test_strategy(m.using_copy_strategy(), values, "copy strategy");
    test_strategy(m.using_external_strategy(), values, "external strategy");
}

template<std::ranges::forward_range Container = std::vector<int>,
         typename Midpoint = test::expect_midpoint<std::ranges::range_value_t<Container>>,
         typename Comp = std::less<>, typename Proj = std::identity>
static void test_values(Container&& values, Midpoint expected,
                        Comp compare = {}, Proj projection = {})
{
    test_values_const_input(values, expected, compare, projection);

    auto const m = stats::median
        .using_compare(compare)
        .using_projection(projection)
        .using_midpoint(expected);

    test_strategy(m.using_inplace_strategy(), values, "inplace strategy");
}


TEST(Median, Empty)
{
    EXPECT_THROW(stats::median(std::vector<int>{}), std::invalid_argument);
}

TEST(Median, OneElement)
{
    SCOPED_TRACE("from here\n");
    test_values_trivial({100}, {100, 100});
}

TEST(Median, TwoElements)
{
    SCOPED_TRACE("from here\n");
    test_values_trivial({100, 200}, {100, 200});
    SCOPED_TRACE("from here\n");
    test_values_trivial({200, 100}, {100, 200});
}

TEST(Median, ThreeSortedElements)
{
    SCOPED_TRACE("from here\n");
    test_values_trivial({1, 2, 3}, {2, 2});
}

TEST(Median, ThreeElements)
{
    SCOPED_TRACE("from here\n");
    test_values({1, 3, 2}, {2, 2});
}

TEST(Median, FourSortedElements)
{
    SCOPED_TRACE("from here\n");
    test_values_trivial({2, 4, 6, 8}, {4, 6});
    SCOPED_TRACE("from here\n");
    test_values_trivial({4, 4, 4, 6}, {4, 4});
}

TEST(Median, FourElements)
{
    SCOPED_TRACE("from here\n");
    test_values({8, 2, 6, 4}, {4, 6});
    SCOPED_TRACE("from here\n");
    test_values({4, 4, 6, 4}, {4, 4});
}

TEST(Median, FiveElements)
{
    SCOPED_TRACE("from here\n");
    test_values({8, 2, 6, 4, 0}, {4, 4});
}


TEST(Median, PlainArray)
{
    int values[] = { 2, 1, 3};
    test_values(values, {2, 2});
}

TEST(Median, ConstPlainArray)
{
    const int values[] = { 2, 1, 3};
    test_values_const_input(values, {2, 2});
    // Also exercise the range-adaptor style
    EXPECT_EQ(values | stats::median, 2);
}

TEST(Median, Strings)
{
    SCOPED_TRACE("from here\n");
    std::string_view values[] = { "one", "two", "three", "four", "five", "six" };
    // Alphabetical:  five four ONE SIX three two
    test_values(values, test::expect_midpoint<std::string_view>{"one", "six"});
}


TEST(Median, NaNsFirst)
{
    constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
    double values[] = { nan, nan, 1, 1, 100, 100, 10 };
    test_values_trivial(values, test::dummy_midpoint{});
    EXPECT_TRUE(std::isnan(stats::median(values)));
}

TEST(Median, NaNsLast)
{
    constexpr auto nan = std::numeric_limits<double>::quiet_NaN();
    double values[] = { 1, 1, 100, 100, 10, nan, nan };
    test_values_trivial(values, test::dummy_midpoint{});
    EXPECT_TRUE(std::isnan(stats::median(values)));
}

TEST(Median, Infinities)
{
    constexpr auto inf = std::numeric_limits<double>::infinity();
    std::vector<double> values;
    values = { -inf, inf, -inf };
    EXPECT_EQ(stats::median(values), -inf);
    values = { inf, -inf, inf };
    EXPECT_EQ(stats::median(values), inf);
    values = { inf, -inf, inf, -inf };
    EXPECT_TRUE(std::isnan(stats::median(values))); // midpoint of ±∞
}


TEST(Median, CustomOrder)
{
    auto values = std::array{3, 4, 5, 100, 101, 102};
    // order by last digit:  100, 101, 102, 3, 4, 5
    auto const compare = [](int a, int b){ return a % 10 < b % 10; };
    SCOPED_TRACE("from here\n");
    test_values(values, {102, 3}, compare);
}

TEST(Median, CustomProjection)
{
    auto values = std::array{3, 4, 5, 100, 101, 102};
    // project to last digit:  0, 1, 2, 3, 4, 5
    auto const projection = [](int a){ return a % 10; };
    SCOPED_TRACE("from here\n");
    test_values(values, {2, 3}, {}, projection);
}

TEST(Median, Value)
{
    auto const values = std::forward_list{0, 1, 2, 3};

    EXPECT_EQ(stats::median(values), 1); // rounded down
    EXPECT_EQ(stats::median.using_arithmetic_midpoint()(values), 1.5);

    // And with reverse order (causing integer std::midpoint() to round upwards)
    auto m = stats::median.using_compare(std::greater<int>{});
    EXPECT_EQ(m(values), 2);
    EXPECT_EQ(m.using_arithmetic_midpoint<long double>()(values), 1.5L);
}

TEST(Median, MoveOnly)
{
    // finds test::midpoint (which returns the difference!)
    std::array<test::moveonly_int, 4> values{0, 3, 5, 2};
    EXPECT_FALSE(can_call(stats::median.using_copy_strategy(), values));
    EXPECT_EQ(stats::median(values), 1); // 3 - 2
    EXPECT_EQ(stats::median.using_inplace_strategy()(values), 1);
}

TEST(Median, NoCopy)
{
    // finds test::midpoint (which returns the sum!)
    std::array<test::nocopy_int, 4> values{0, 1, 4, 2};
    EXPECT_FALSE(can_call(stats::median.using_inplace_strategy(), values));
    EXPECT_FALSE(can_call(stats::median.using_copy_strategy(), values));
    EXPECT_EQ(stats::median.using_external_strategy()(values), 3); // 1 + 2
    EXPECT_EQ(stats::median(std::move(values)), 3);
}

TEST(Median, FilteredRange)
{
    constexpr auto nan = std::numeric_limits<double>::quiet_NaN();

    double values[] = { nan, nan, 1, 100, 10 };
    auto view = values | std::views::filter([](double d){ return !std::isnan(d); });

    EXPECT_EQ(stats::median.using_copy_strategy()(view), 10);
    EXPECT_EQ(stats::median.using_external_strategy()(view), 10);
    EXPECT_EQ(stats::median(view), 10);
    EXPECT_EQ(values[2], 1);    // shouldn't have modified underlying range
}

TEST(Median, FilteredRangeConst)
{
    constexpr auto nan = std::numeric_limits<double>::quiet_NaN();

    double values[] = { nan, nan, 1, 100, 10 };
    auto const view = values | std::views::filter([](double d){ return !std::isnan(d); });

    EXPECT_EQ(stats::median.using_copy_strategy()(view), 10);
    EXPECT_EQ(stats::median.using_external_strategy()(view), 10);
    EXPECT_EQ(stats::median(view), 10);
    EXPECT_EQ(values[2], 1);    // shouldn't have modified underlying range
}

#endif
