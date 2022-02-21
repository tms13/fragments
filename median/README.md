# Flexible median evaluator

This code can compute the median of almost any sequence.

All the options made for some combinatorial explosion of arguments,
making it hard to default some subset of them, so it can be created
using a Builder pattern.  That allows simple use such as

> ```
> // values is any sequence that satisfies Forward Range
> auto m = stats::median(values);
> ```

and more advanced use by calling factories, possibly chained:

> ```
> auto m = stats::median.using_compare(<std::greater>)
>                       .using_arithmetic_midpoint()
>                       (values);
> ```

To minimise copying, I wanted to sort in-place when we're passed
ownership:

> ```
> auto m = stats::median(std::move(values));
> ```

If we want to permit mutating of all writable ranges, then we can
specifically ask for the in-place policy:

> ```
> auto calc_median = stats::median.using_inplace_strategy();
> auto m = calc_median(values);
> ```

The other primitive policies are
1. `copy_strategy`, which always makes a copy of the (projected)
   values, and
1. `external_strategy`, which makes pointers to the elements (without
   projection, since that need not be transparent).
   
Note that these three strategies have different requirements on the
range - copy accepts an input range; external needs a forward range;
inplace is most restrictive, needing a random-access range.

From these, we have two composite strategies:
1. The default strategy sorts in-place if possible, otherwise by
   copying, falling back to the external strategy as a last resort.
1. The minimum-space (`frugal`) strategy also prefers to sort in-place
   if possible, but prefers the external strategy over copying when
   pointers to elements are smaller than projected values.

I've tried to use consistent template parameter names (i.e. `Comp` for
a comparator and `Proj` for projection).  I looked to the standard for
guidance, but it's inconsistent in this respect - sometimes even within
one section (e.g. the description of `nth_element()` and its associated
concepts).
