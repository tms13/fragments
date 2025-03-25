#include <map>
#include <ranges>
#include <stdexcept>
#include <string_view>
#include <utility>

// Returns a function object that converts any character present in
// "from" to the corresponding character in "to".
auto make_substitutor(std::wstring_view from, std::wstring_view to)
{
    if (from.size() != to.size()) {
        throw std::invalid_argument("Replacement length mismatch");
    }
    auto map = std::views::zip(from, to) | std::ranges::to<std::map>();
    return [map=std::move(map)](wchar_t c) {
        auto it = map.find(c);
        return it == map.end() ? c : it->second;
    };
}

#ifdef USING_GTEST

#include <gtest/gtest.h>

#include <algorithm>
#include <string>

auto tr_string(auto const& tr, std::wstring s)
{
    std::ranges::transform(s, s.begin(), tr);
    return s;
}

TEST(tr, bad_args)
{
    EXPECT_THROW(make_substitutor(L"hello", L"hello"), std::invalid_argument);
}

TEST(tr, noop)
{
    auto const tr = make_substitutor(L"", L"");
    EXPECT_EQ(tr_string(tr, L""), L"");
    EXPECT_EQ(tr_string(tr, L"hello"), L"hello");
}

TEST(tr, english)
{
    auto const tr = make_substitutor(L"ehlo", L"ipza");
    EXPECT_EQ(tr_string(tr, L""), L"");
    EXPECT_EQ(tr_string(tr, L"hello"), L"pizza");
}

TEST(tr, greek)
{
    auto const tr = make_substitutor(L"αβγδεζηθικλμνξοπρσςτυφχψω",
                                     L"ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΣΤΥΦΧΨΩ");
    EXPECT_EQ(tr_string(tr, L"Γεια σας"), L"ΓΕΙΑ ΣΑΣ");
}


#else

#include <algorithm>
#include <iostream>
#include <iterator>
#include <locale>
#include <cstdlib>
#include <cstring>
#include <print>

// Convert from platform string to wide string
static std::wstring arg_to_wstring(const char* arg)
{
    std::wstring s(std::strlen(arg), '\0'); // likely over-allocation
    auto len = std::mbstowcs(s.data(), arg, s.size());
    if (len == -1uz) {
        throw std::invalid_argument("Malformed multibyte string");
    }
    s.resize(len);
    return s;
}

int main(int argc, char **argv)
{
    if (!std::setlocale(LC_ALL, "")) {
        println(std::cerr, "Invalid locale settings");
        return EXIT_FAILURE;
    }
    if (argc != 3) {
        println(std::cerr, "Usage: {} from_chars to_chars", *argv ? *argv : "tr");
        return EXIT_FAILURE;
    }

    try {

        std::wstring const from = arg_to_wstring(argv[1]);
        std::wstring const to = arg_to_wstring(argv[2]);

        std::wcin.exceptions(std::ios::badbit | std::ios::failbit);
        std::wcout.exceptions(std::ios::badbit | std::ios::failbit);

        std::transform(std::istreambuf_iterator<wchar_t>(std::wcin),
                       std::istreambuf_iterator<wchar_t>(),
                       std::ostreambuf_iterator<wchar_t>(std::wcout),
                       make_substitutor(from, to));
    } catch (std::exception& e) {
        println(std::cerr, "{}", e.what());
        return EXIT_FAILURE;
    }
}

#endif
