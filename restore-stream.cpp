#include <ios>
#include <iomanip>

// Members are all public and mutable, so if we really don't want
// to restore any particular part of the state, we can override.
template<class CharT, class Traits = typename std::char_traits<CharT>>
struct save_stream_state
{
    std::basic_ios<CharT,Traits>& stream;
    std::ios_base::fmtflags flags;
    std::locale locale;
    std::streamsize precision;
    std::streamsize width;
    CharT fill;

    save_stream_state(std::basic_ios<CharT,Traits>& stream)
        : stream{stream},
          flags{stream.flags()},
          locale{stream.getloc()},
          precision{stream.precision()},
          width{stream.width()},
          fill{stream.fill()}
    {}

    // deleting copy construction also prevents move
    save_stream_state(const save_stream_state&) = delete;
    void operator=(const save_stream_state&) = delete;

    ~save_stream_state()
    {
        stream.flags(flags);
        stream.imbue(locale);
        stream.precision(precision);
        stream.width(width);
        stream.fill(fill);
    }
};


#include <iostream>
int main()
{
    auto test = []() {
        std::cout << std::setw(15) << "Foo" << ' '
        << true << ' ' << 123456 << '\n';
    };
    {
        test();
        const save_stream_state guard{std::cout};
        std::cout << std::setfill('_') << std::left << std::uppercase
                  << std::boolalpha << std::hex << std::showbase;
        test();
    } // stream restored here
    test();


    std::cout << std::endl;


    // Now with wide-character stream:
    auto wtest = []() {
        std::wclog << std::setw(15) << L"Foo" << L' '
        << true << L' ' << 123456 << L'\n';
    };
    {
        wtest();
        // AAA style initialization - and guard multiple streams
        auto const guard = { save_stream_state{std::wclog},
                             save_stream_state{std::wcin} };
        std::wclog << std::setfill(L'_') << std::left << std::uppercase
                   << std::boolalpha << std::hex << std::showbase;
        wtest();
    } // stream restored here
    wtest();
}
