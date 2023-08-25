#include <algorithm>
#include <cmath>
#include <concepts>
#include <cstdlib>
#include <functional>
#include <limits>
#include <numeric>
#include <ranges>
#include <utility>
#include <vector>

template<std::unsigned_integral Number>
class aliquot_sums
{
    std::vector<Number> sums;
    std::vector<Number> prime_numbers;

    static constexpr inline Number ipow(Number base, unsigned exp)
    {
        // Binary exponentiation
        Number result = 1;
        for (auto m = base;  exp;  m *= m, exp /= 2) {
            if (exp % 2) { result *= m; }
        }
        return result;
    }

    static constexpr inline Number sum_powers(Number p, unsigned i)
    {
        // Each prime factor p with cardinality i contributes
        //    1 + p + p² + ... + pⁱ to the product
        // That simplifies to
        //    (pⁱ⁺¹ - 1) / (p - 1)
        return (ipow(p, i+1) - 1) / (p - 1);
    };

    static constexpr inline bool greater_than_sqrt(Number a, Number b)
    {
        // Test a > √b efficiently without overflow
        static constexpr const Number max_root = std::sqrt(std::numeric_limits<Number>::max());
        if (a <= max_root) [[likely]] {
            return a * a > b;
        } else {
            return a > b / a;
        }
    }


public:
    constexpr aliquot_sums(Number maxval)
        : sums{ 0 },
          prime_numbers{2}
    {
        sums.reserve(maxval);
        // Estimate number of primes using Gauss/Legendre approximation: π(x) ≅ x / ln(x)
        prime_numbers.reserve(static_cast<std::size_t>(std::ceil(static_cast<double>(maxval) / std::log(maxval))));

        for (auto const number: std::views::iota(Number{1}, maxval)) {
            // From the prime factorisation n = 2ᵃ·3ᵇ·5ᶜ·…·pˣ·…,
            // we can reconstruct the factors as all 2ⁱ·3ʲ·5ᵏ·…·pʸ·…
            // where 0 ≤ i ≤ a, 0 ≤ j ≤ b, etc.
            // The sum of these is thus
            // (2⁰+2¹+2²+…+2ᵃ)·(3⁰+3¹+3²+…+3ᵇ)·…·(p⁰+p¹+p²+…+pˣ)·…
            Number aliquot = 1;
            auto n = number;
            for (auto const p: prime_numbers) {
                unsigned count = 0; // cardinality of this prime
                while (n % p == 0) {
                    ++count;
                    n /= p;
                }
                if (count > 0) {
                    aliquot *= sum_powers(p, count);
                };
                if (greater_than_sqrt(p, n)) {
                    break;          // p > √n  ⇒  n is 1 or prime
                }
            }
            if (n > 1) {
                // We ended on a prime; its count is 1
                aliquot *= n + 1;
                if (n > prime_numbers.back()) {
                    // not already seen
                    prime_numbers.push_back(n);
                }
            }
            // We have summed _all_ the factors, so need to subtract
            // the number itself to get the _proper_ factors.
            sums.push_back(aliquot - number);
        }
    }

    constexpr auto const& primes() const
    {
        return prime_numbers;
    }

    constexpr auto perfect_numbers() const
    {
        std::vector<Number> result;
        for (auto const i: std::views::iota(1u, sums.size())) {
            if (sums[i] == i) {
                result.push_back(i);
            }
        }
        return result;
    }

    constexpr auto amicable_pairs() const
    {
        std::vector<std::pair<Number,Number>> pairs;
        for (auto const a: std::views::iota(1uz, sums.size())) {
            auto const b = sums[a];
            if (a < b && b < sums.size() && sums[b] == a) {
                pairs.emplace_back(a, b);
            }
        }
        return pairs;
    }
};


#include <cstdint>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>

int main(int argc, char **argv)
{
    using Number = std::uint_fast32_t;

    Number maxval = 1'000'000;     // default
    switch (argc) {
    case 0:
    case 1:
        break;
    case 2:
        try {
            std::size_t endpos;
            auto n = std::stoul(argv[1], &endpos, 0);
            if (n > std::numeric_limits<Number>::max()) { throw std::out_of_range{argv[1]}; }
            if (argv[1][endpos]) { throw std::invalid_argument{argv[1]}; }
            maxval = static_cast<Number>(n);
        } catch (std::exception& e) {
            std::cerr << "Invalid argument: " << e.what() << '\n';
            return EXIT_FAILURE;
        }
        break;
    default:
        std::cerr << "Too many arguments" << '\n';
        return EXIT_FAILURE;
    }

    auto const sums = aliquot_sums{maxval};

    std::cout << "Perfect Numbers:\n";
    for (auto n: sums.perfect_numbers()) {
        std::cout << n << '\n';
    }

    std::cout << "\nAmicable Pairs:\n";
    for (auto [a, b]: sums.amicable_pairs()) {
        std::cout << a << ',' << b << '\n';
    }
}
