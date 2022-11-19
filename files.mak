
median: median.o gtest_main.o gtest-all.o
median: LINK.o = $(CXX)
median: LDLIBS += -pthread
median: CXXFLAGS += -fconcepts

median-flexible view: CXXFLAGS += -ftemplate-backtrace-limit=0 -fconcepts-diagnostics-depth=10

median-flexible: median-flexible.hh
