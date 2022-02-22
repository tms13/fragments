#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <numeric>
#include <stdexcept>

class caesar_rotator {
    using char_table = std::array<char, UCHAR_MAX+1>;
    const char_table table;

public:
    caesar_rotator(int rotation) noexcept
        : table{create_table(rotation)}
    {}

    char operator()(char c) const noexcept
    {
        return table[static_cast<unsigned char>(c)];
    };

private:
#define LETTERS "abcdefghijklmnopqrstuvwxyz"
    static char_table create_table(int rotation)
    {
        constexpr int len = (sizeof LETTERS) - 1; // don't count the terminating null
        static const auto* alpha2 = reinterpret_cast<const unsigned char*>(LETTERS LETTERS);

        // normalise to the smallest positive equivalent
        rotation = (rotation % len + len) % len;

        char_table table;
        // begin with a identity mapping
        std::iota(table.begin(), table.end(), 0);
        // change the mapping of letters
        for (auto i = 0;  i < len;  ++i) {
            table[alpha2[i]] = alpha2[i+rotation];
            table[std::toupper(alpha2[i])] = static_cast<char>(std::toupper(alpha2[i+rotation]));
        }
        return table;
    }
#undef LETTERS
};


int main(int argc, char **argv)
{
    constexpr int default_rotation = 13;
    // Parse arguments
    int rotation;
    if (argc <= 1) {
        rotation = default_rotation;
    } else if (argc == 2) {
        try {
            std::size_t end;
            rotation = std::stoi(argv[1], &end);
            if (argv[1][end]) { throw std::invalid_argument(""); }
        } catch (...) {
            std::cerr << "Invalid Caesar shift value: " << argv[1] << " (integer required)\n";
            return EXIT_FAILURE;
        }
    } else {
        std::cerr << "Usage: " << argv[0]
                  << " [NUMBER]\nCaesar-shift letters in standard input by NUMBER places (default "
                  << default_rotation <<")\n";
        return EXIT_FAILURE;
    }

    // Now filter the input
    std::transform(std::istreambuf_iterator<char>{std::cin},
                   std::istreambuf_iterator<char>{},
                   std::ostreambuf_iterator<char>{std::cout},
                   caesar_rotator{rotation});
}
