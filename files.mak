
codecvt: CXXFLAGS += -fconcepts-diagnostics-depth=3
codecvt.run: INFILE = codecvt.in

median: median.o gtest_main.o gtest-all.o
median: LINK.o = $(CXX)
median: LDLIBS += -pthread
median: CXXFLAGS += -fconcepts

median-flexible view: CXXFLAGS += -ftemplate-backtrace-limit=0 -fconcepts-diagnostics-depth=10

median-flexible: median-flexible.hh

wchar-tr.run: RUNARGS = αβγδεζηθικλμνξοπρσςτυφχψω ΑΒΓΔΕΖΗΘΙΚΛΜΝΞΟΠΡΣΣΤΥΦΧΨΩ
wchar-tr.run: INPUT = Γεια σας

USING_GTEST += codecvt
USING_GTEST += endian
USING_GTEST += wchar-tr

OPTIMIZED += amicable-numbers
