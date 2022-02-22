#include <algorithm>
#include <array>
#include <cctype>
#include <climits>
#include <cstdlib>
#include <cstring>
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
    static constexpr int upper_int(char c)
    {
        return std::toupper(static_cast<unsigned char>(c));
    }
    static constexpr char upper_char(char c)
    {
        return static_cast<char>(upper_int(c));
    }

    static char_table create_table(int rotation)
    {
        constexpr auto* letters = "abcdefghijklmnopqrstuvwxyz";
        constexpr int len = std::strlen(letters);

        // normalise to the smallest positive equivalent
        rotation = (rotation % len + len) % len;

        char_table table;
        // begin with a identity mapping
        std::iota(table.begin(), table.end(), 0);
        // change the mapping of letters
        for (auto i = 0, target = rotation;  i < len;  ++i, ++target) {
            if (target == len) {
                target = 0;
            }
            table[letters[i]] = letters[target];
            table[upper_int(letters[i])] = upper_char(letters[target]);
        }
        return table;
    }
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
