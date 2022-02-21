#ifndef MEDIAN_H
#define MEDIAN_H

#include <algorithm>
#include <cmath>
#include <concepts>
#include <functional>
#include <iterator>
#include <memory>
#include <numeric>
#include <ranges>
#include <utility>

/*
  A flexible but user-friendly way to evaluate the median of almost any collection.

  Easy interface:
  * stats::median(values)               // values is unchanged

  * stats::median(std::move(values))    // may re-order the container

  * values | stats::median              // works like a view

  More advanced:
  * auto small_median = stats::median.using_frugal_strategy();
    small_median(values)                // tries harder not to minimize memory use

  * Other strategies are provided.  The "inplace" strategy is useful to end users, as it treats all
    inputs as rvalues (modifying through references) even without `std::move()`.  The "copy" and
    "external" strategies are mostly useful to the implementation of the default and frugal ones.

  * We can use any comparator or projection function, and any any function to calculate the mean of
    the mid elements (this function will be passed duplicate arguments if the input size is odd).

    An "arithmetic" midpoint function is provided; this can be useful for getting fractional medians
    from integer inputs.  For example:

        stats::median.using_arithmetic_midpoint()(std::array{ 0, 1, 2, 3})  ⟶  1.5
 */

namespace stats
{
    // Type traits
    template<std::ranges::forward_range Range, typename Proj>
    using projected_t =
        std::projected<std::ranges::iterator_t<Range>, Proj>::value_type;

    template<std::ranges::forward_range Range, typename Proj, typename Midpoint>
    using median_result_t =
        std::invoke_result_t<Midpoint, projected_t<Range, Proj>, projected_t<Range, Proj>>;

    // Concepts
    template<typename Range, typename Comp, typename Proj>
    concept sortable_range =
        std::sortable<std::ranges::iterator_t<Range>, Comp, Proj>;

    template<typename C, typename Range, typename Proj>
    concept projected_strict_weak_order =
        std::indirect_strict_weak_order<C, std::projected<std::ranges::iterator_t<Range>, Proj>>;

    template<typename M, typename Range, typename Proj>
    concept midpoint_function =
        std::invocable<M, projected_t<Range, Proj>, projected_t<Range, Proj>>;

    template<typename S>
    concept median_strategy =
        std::is_same_v<std::invoke_result_t<S, std::vector<int>&&, std::less<>, std::identity, std::less<>>, bool>;

    // Midpoint policies
    struct default_midpoint
    {
        template<typename T>
        constexpr auto operator()(T const& a, T const& b) const
        {
            using std::midpoint;
            return midpoint(a, b);
        }
    };

    template<typename T>
    struct arithmetic_midpoint
    {
        constexpr auto operator()(T const& a, T const& b) const
        {
            return default_midpoint{}.operator()<T>(a, b);
        }
    };

    // Median policies
    struct presorted_strategy
    {
        // This one only works if the input is already sorted.
        // Normally only called after an `is_sorted()` test.
        template<std::ranges::forward_range Range,
                 std::invocable<std::ranges::range_value_t<Range>> Proj,
                 projected_strict_weak_order<Range, Proj> Comp,
                 midpoint_function<Range, Proj> Midpoint>
        auto operator()(Range&& values, Comp, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
        {
            // Already ordered; just access the middle elements.
            auto const size = std::ranges::distance(values);
            auto const lower = std::next(std::ranges::begin(values), (size - 1) / 2);
            auto const upper = size % 2 ? lower : std::next(lower);
            return midpoint(proj(*lower), proj(*upper));
        }
    };

    struct inplace_strategy
    {
        template<std::ranges::random_access_range Range,
                 std::invocable<std::ranges::range_value_t<Range>> Proj,
                 projected_strict_weak_order<Range, Proj> Comp,
                 midpoint_function<Range, Proj> Midpoint>
        auto operator()(Range&& values, Comp compare, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
            requires sortable_range<Range, Comp, Proj>
        {
            auto const size = std::ranges::distance(values);

            auto upper = std::ranges::begin(values) + size / 2;
            std::ranges::nth_element(values, upper, compare, proj);
            auto lower = size % 2 ? upper
                : std::ranges::max_element(std::ranges::begin(values), upper, compare, proj);
            return midpoint(std::invoke(proj, *lower), std::invoke(proj, *upper));
        }
    };

    struct inplace_strategy_rvalues_only
    {
        // Exists mainly to implement the default and frugal strategies
        // But could be useful if you need to disallow copy and external.
        template<std::ranges::random_access_range Range,
                 std::invocable<std::ranges::range_value_t<Range>> Proj,
                 projected_strict_weak_order<Range, Proj> Comp,
                 midpoint_function<Range, Proj> Midpoint>
        auto operator()(Range&& values, Comp compare, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
            requires sortable_range<Range, Comp, Proj> && (!std::is_lvalue_reference_v<Range>)
        {
            return inplace_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
        }
    };

    struct copy_strategy
    {
        template<std::ranges::input_range Range,
                 std::invocable<std::ranges::range_value_t<Range>> Proj,
                 projected_strict_weak_order<Range, Proj> Comp,
                 midpoint_function<Range, Proj> Midpoint>
        auto operator()(Range&& values, Comp compare, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
            requires std::copyable<std::remove_reference_t<projected_t<Range, Proj>>>
        {
            auto projected = values | std::views::transform(proj);
            auto v = std::vector(std::ranges::begin(projected), std::ranges::end(projected));
            return inplace_strategy{}(v, compare, std::identity{}, midpoint);
         }
    };

    struct external_strategy
    {
        template<std::ranges::forward_range Range,
                 std::invocable<std::ranges::range_value_t<Range>> Proj,
                 projected_strict_weak_order<Range, Proj> Comp,
                 midpoint_function<Range, Proj> Midpoint>
        auto operator()(Range&& values, Comp compare, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
        {
            using pointer_type = std::add_pointer_t<const std::ranges::range_value_t<Range>>;
            using pointer_traits = std::pointer_traits<pointer_type>;
            auto indirect_project = [proj](auto const* a)->decltype(auto) { return std::invoke(proj, *a); };

            auto pointers = values | std::views::transform(pointer_traits::pointer_to);
            auto v = std::vector(std::ranges::begin(pointers), std::ranges::end(pointers));
            return inplace_strategy{}(v, compare, indirect_project, midpoint);
         }
    };

    // Policy adaptor
    template<typename Policy>
    struct shortcircuit_sorted
    {
        const Policy inner;

        template<std::ranges::forward_range Range,
                 typename Comp,
                 typename Proj,
                 typename Midpoint>
        auto operator()(Range&& values, Comp compare, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
            requires std::invocable<Policy, Range, Comp, Proj, Midpoint>
        {
            // If already ordered, just access the middle elements
            if (std::ranges::is_sorted(values, compare, proj)) {
                return presorted_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
            }
            return inner(std::forward<Range>(values), compare, proj, midpoint);
        }
    };

    // Composite policies
    struct default_strategy
    {
        template<std::ranges::forward_range Range,
                 std::invocable<std::ranges::range_value_t<Range>> Proj,
                 projected_strict_weak_order<Range, Proj> Comp,
                 midpoint_function<Range, Proj> Midpoint>
        auto operator()(Range&& values, Comp compare, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
            requires std::invocable<inplace_strategy_rvalues_only, Range, Comp, Proj, Midpoint>
                  || std::invocable<copy_strategy, Range, Comp, Proj, Midpoint>
                  || std::invocable<external_strategy, Range, Comp, Proj, Midpoint>
        {
            if constexpr (std::invocable<inplace_strategy_rvalues_only, Range, Comp, Proj, Midpoint>) {
                return inplace_strategy_rvalues_only{}(std::forward<Range>(values), compare, proj, midpoint);
            }
            if (std::ranges::is_sorted(values, compare, proj)) {
                return presorted_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
            }
            if constexpr (std::invocable<copy_strategy, Range, Comp, Proj, Midpoint>) {
                try {
                    return copy_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
                } catch (std::bad_alloc&) {
                    if constexpr (!std::invocable<external_strategy, Range, Comp, Proj, Midpoint>) {
                        throw;
                    }
                    if constexpr (sizeof (projected_t<Range, Proj>*) >= sizeof (projected_t<Range, Proj>)) {
                        // external strategy won't help
                        throw;
                    }
                    // Else, we can try using external strategy more cheaply,
                    // so fallthrough to try that.
                }
            }
            if constexpr (std::invocable<external_strategy, Range, Comp, Proj, Midpoint>) {
                return external_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
            }
        }
    };

    struct frugal_strategy
    {
        template<std::ranges::forward_range Range,
                 std::invocable<std::ranges::range_value_t<Range>> Proj,
                 projected_strict_weak_order<Range, Proj> Comp,
                 midpoint_function<Range, Proj> Midpoint>
        auto operator()(Range&& values, Comp compare, Proj proj, Midpoint midpoint) const
            -> median_result_t<Range, Proj, Midpoint>
            requires std::invocable<inplace_strategy_rvalues_only, Range, Comp, Proj, Midpoint>
                  || std::invocable<copy_strategy, Range, Comp, Proj, Midpoint>
                  || std::invocable<external_strategy, Range, Comp, Proj, Midpoint>
        {
            constexpr auto can_inplace = std::invocable<inplace_strategy_rvalues_only, Range, Comp, Proj, Midpoint>;
            constexpr auto can_external = std::invocable<external_strategy, Range, Comp, Proj, Midpoint>;
            constexpr auto can_copy = std::invocable<copy_strategy, Range, Comp, Proj, Midpoint>;

            if constexpr (can_inplace) {
                return inplace_strategy_rvalues_only{}(std::forward<Range>(values), compare, proj, midpoint);
            }
            if (std::ranges::is_sorted(values, compare, proj)) {
                return presorted_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
            }
            if constexpr (can_copy && sizeof (projected_t<Range, Proj>*) < sizeof (projected_t<Range, Proj>)) {
                // Copies are smaller than pointers
                return copy_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
            }
            if constexpr (can_external) {
                return external_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
            }
            if constexpr (can_copy) {
                return copy_strategy{}(std::forward<Range>(values), compare, proj, midpoint);
            }
        }
    };

    // The median calculator type
    template<typename Proj, typename Comp, typename Midpoint, typename Strategy>
    class median_engine
    {
        const Strategy strategy;
        const Comp compare;
        const Proj projection;
        const Midpoint midpoint;

    public:
        // For simple construction, start with stats::median and use
        // the builder interface to customise it.
        constexpr median_engine(Proj projection, Comp comparer,
                                Midpoint midpoint, Strategy strategy) noexcept
            : strategy{std::move(strategy)},
              compare{std::move(comparer)},
              projection{std::move(projection)},
              midpoint{std::move(midpoint)}
        {}

        // Builder interface - projection, comparator and midpoint functions
        template<typename P>
        [[nodiscard]]
        constexpr auto using_projection(P projection) const {
            return median_engine<P, Comp, Midpoint, Strategy>
                (std::move(projection), compare, midpoint, strategy);
        }

        template<typename C>
        [[nodiscard]]
        constexpr auto using_compare(C compare) const {
            return median_engine<Proj, C, Midpoint, Strategy>
                (projection, std::move(compare), midpoint, strategy);
        }

        template<typename M>
        [[nodiscard]]
        constexpr auto using_midpoint(M midpoint) const {
            return median_engine<Proj, Comp, M, Strategy>
                (projection, compare, std::move(midpoint), strategy);
        }
        template<typename T = double>
        [[nodiscard]]
        constexpr auto using_arithmetic_midpoint() const {
            return using_midpoint(arithmetic_midpoint<T>{});
        }

        // Builder interface - median strategy
        template<median_strategy S>
        [[nodiscard]]
        constexpr auto using_strategy(S strategy) const
        {
            return median_engine<Proj, Comp, Midpoint, S>
                (projection, compare, midpoint, std::move(strategy));
        }

        [[nodiscard]] constexpr auto using_inplace_strategy() const
        { return using_strategy(inplace_strategy{}); }

        [[nodiscard]] constexpr auto using_external_strategy() const
        { return using_strategy(shortcircuit_sorted{external_strategy{}}); }

        [[nodiscard]] constexpr auto using_copy_strategy() const
        { return using_strategy(shortcircuit_sorted{copy_strategy{}}); }

        [[nodiscard]] constexpr auto using_default_strategy() const
        { return using_strategy(default_strategy{}); }

        [[nodiscard]] constexpr auto using_frugal_strategy() const
        { return using_strategy(frugal_strategy{}); }

        // Main function interface:
        // Compute the median of a range of values
        template<std::ranges::forward_range Range>
        auto operator()(Range&& values) const
            requires std::invocable<Strategy, Range, Comp, Proj, Midpoint>
        {
            return calculate_median(std::forward<Range>(values));
        }

        // Overload for const filter views which are not standard ranges.
        // Some standard views (chunk_by_view, drop_while_view, filter_view, split_view) are not
        // const-iterable, due to time complexity requirements on begin() requiring it to remember
        // its result.  See https://stackoverflow.com/q/67667318
        template<typename View>
        auto operator()(View&& values) const
            requires (!std::ranges::range<View>)
            && std::ranges::range<std::decay_t<View>>
        {
            // Make a copy - which is a range
            auto values_copy = values;
            // but pass it as an lvalue ref so we don't order in-place by default
            return calculate_median(values_copy);
        }

    private:
        template<std::ranges::forward_range Range>
        auto calculate_median(Range&& values) const
            requires std::invocable<Strategy, Range, Comp, Proj, Midpoint>
        {
            auto const begin = std::ranges::begin(values);
            auto const size = std::ranges::distance(values);

            switch (size) {
            case 0:
                throw std::invalid_argument("Attempting median of empty range");
            case 1:
                {
                    auto const& a = project(begin);
                    return midpoint(a, a);
                }
            case 2:
                {
                    auto const& a = project(begin);
                    auto const& b = project(std::next(begin));
                    if (compare(a, b)) {
                        // Yes, the order matters!
                        // e.g. std::midpoint rounds towards its first argument.
                        return midpoint(a, b);
                    } else {
                        return midpoint(b, a);
                    }
                }
            }

            // If the range contains NaN values, there is no meaningful median.
            // We still need to launder through mipoint() for correct return type.
            using value_type = std::ranges::range_value_t<Range>;
            if constexpr (std::is_floating_point_v<std::remove_reference_t<value_type>>) {
                auto isnan = [](value_type d){ return std::isnan(d); };
                if (auto it = std::ranges::find_if(values, isnan, projection); it != std::ranges::end(values)) {
                    return midpoint(project(it), project(it));
                }
            }

            // else use selected strategy
            return strategy(std::forward<Range>(values), compare, projection, midpoint);
        }

        auto project(std::indirectly_readable auto p) const -> decltype(auto)
        {
            return std::invoke(projection, *p);
        }
    };

    // We can put a median engine at the end of an adaptor chain
    // e.g.   auto midval = view | filter | median;
    template<typename InputRange, typename... MedianArgs>
    auto operator|(InputRange&& range, median_engine<MedianArgs...> engine)
    {
        return std::forward<median_engine<MedianArgs...>>(engine)(std::forward<InputRange>(range));
    }

    // Default engine, from which we can obtain customised ones using the builder interface.
    static constexpr auto median = median_engine
        {
            std::identity{},
            std::less<>{},
            default_midpoint{},
            default_strategy{}
        };
}

#endif
