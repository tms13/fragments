SHELL = bash

CC := gcc-14
CXX := g++-14

COMPILE.s = $(AS) $(ASFLAGS) $(TARGET_MACH)
AS := nasm

CXXVER := c++26
CVER := c23
WARNINGS = -Wall -Wextra -Wwrite-strings
WARNINGS += -Wno-parentheses -Wdangling-else
WARNINGS += -Wpedantic -Warray-bounds
#WARNINGS += -Wmissing-braces   # re-enable after https://gcc.gnu.org/bugzilla/show_bug.cgi?id=95330
WARNINGS += -Wconversion
#ANALYZER = -fanalyzer
CXX_WARNINGS += -Wuseless-cast $(if $(PKGS),,-Weffc++)
CC_WARNINGS += -Wstrict-prototypes
DEBUG_OPTIONS += -fPIC -gdwarf-4 -g
ARCH = native

CXXFLAGS += -std=$(CXXVER) $(DEBUG_OPTIONS) $(WARNINGS) $(CXX_WARNINGS) $(ANALYZER) $(INCLUDES)
CFLAGS += -std=$(CVER) $(DEBUG_OPTIONS) $(WARNINGS) $(CC_WARNINGS) $(ANALYZER) $(INCLUDES)
LDLIBS += $(LIBS)

# These EXTRA_FOO varibles allow users to add to FOO (as alternative
# to completely overriding them) with Make command-line arguments.
CXXFLAGS += $(EXTRA_CXXFLAGS)
CFLAGS += $(EXTRA_CFLAGS)
WARNINGS += $(EXTRA_WARNINGS)

CXXFLAGS += $(patsubst -I%,-isystem %,$(if $(PKGS),$(shell pkg-config --cflags $(PKGS))))
CFLAGS += $(patsubst -I%,-isystem %,$(if $(PKGS),$(shell pkg-config --cflags $(PKGS))))
LDLIBS += $(if $(PKGS),$(shell pkg-config --libs $(PKGS)))

PYTHON=/usr/bin/python3

VALGRIND_ARGS := --leak-check=full
#VALGRIND_ARGS += -q

# Prevent test programs from competing too hard for resources (particularly memory, which can lead
# to thrashing swap).
MAX_MEM_KB=1048576
MAX_CPU_SECS=60
LIMITS = ulimit -S -v $(MAX_MEM_KB) -t $(MAX_CPU_SECS)

PROGNAME = ./$(patsubst %.exe,%,$<)
# User can supply command-line arguments in RUNARGS (which will be
# shell-processed) and standard input in INPUT.
export INPUT
print_cmd = printf '%s ' $(TOOL)  $(PROGNAME); $(if $(RUNARGS),/usr/bin/printf '%q ' $(RUNARGS);)

RUN = $(PROGNAME) $(RUNARGS)
RUN += $(if $(INPROC),< <($(INPROC)),$(if $(INFILE),<"$(INFILE)",<<<"$$INPUT"))
print_cmd += /usr/bin/printf $(if $(INPROC),'< <(%s)\n' "$(INPROC)",$(if $(INFILE),'<%q\n' "$(INFILE)",$(if $(subst environment,,$(origin INPUT)),'<<<%q\n' "$$INPUT",'')));
RUN += $(POSTPROC)
print_cmd += echo;

#print_cmd += echo $(origin INFILE) $(INFILE);

.PHONY: %.run %.time %.valgrind %.shellcheck
.DELETE_ON_ERROR:

%.exe: %.cc
	@$(MAKE) $*

%.exe: %.cpp
	@$(MAKE) $*

%.exe: %.c
	@$(MAKE) $*

%.run: %.exe
	@$(print_cmd)
	@$(LIMITS); exec $(TOOL) $(RUN)

%.run: %.py
	@$(print_cmd)
	@$(LIMITS); exec $(TOOL) $(PYTHON) $(RUN)

%.run: %.sh
	@$(print_cmd)
	@$(LIMITS); if test -x $<; then exec $(TOOL) $(RUN); else exec $(TOOL) $(SHELL) $(RUN); fi

%.run: %.dc
	@$(print_cmd)
	@$(LIMITS); exec $(TOOL) $(RUN)

%.run: %.sed
	@$(print_cmd)
	@$(LIMITS); exec $(TOOL) sed $(SEDFLAGS) -f $(RUN)

c%.run: c%.class
	@$(print_cmd)
	@$(LIMITS); exec $(TOOL) java c$* $(wordlist 2,9999,$(RUN))

%.time:
	$(MAKE) TOOL=time $*.run

%.valgrind:
	$(MAKE) TOOL='valgrind $(VALGRIND_ARGS)' $*.run

%.shellcheck: %.sh
	shellcheck -f gcc $(SHELLCHECK_ARGS) $<

%.shellcheck: %
	shellcheck -f gcc $(SHELLCHECK_ARGS) $<


%_moc.cpp: %.h
	moc -o $@ $<

%_moc.cpp: %.cpp
	moc -o $@ $<

%_ui.h: %.ui
	uic -p -o $@ $<

%.s: %.c
	$(CC) $(OUTPUT_OPTION) $< -S $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH)

%.s: %.cpp
	$(CXX) $(OUTPUT_OPTION) $< -S $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH)

%.s: CPPFLAGS += -DASM_OUTPUT

%.s: CFLAGS += -fverbose-asm
%.s: CXXFLAGS += -fverbose-asm


c%.class: c%.java
	javac $^

%.o: %.s
	as -g --32 -o $@ $<

%.s: %.S
	intel2gas -o $@ $<


# Google Test objects

GTEST_DIR = /usr/src/googletest/googletest
GMOCK_DIR = /usr/src/googletest/googlemock

# All Google Test headers.  Usually you shouldn't change this
# definition.
GTEST_HEADERS = $(wildcard /usr/include/gtest/*.h \
                           /usr/include/gtest/internal/*.h)

# Usually you shouldn't tweak such internal variables, indicated by a
# trailing _.
GTEST_SRCS_ = $(wildcard $(GTEST_DIR)/src/*.cc $(GTEST_DIR)/src/*.h) $(GTEST_HEADERS)

# For simplicity and to avoid depending on Google Test's
# implementation details, the dependencies specified below are
# conservative and not optimized.  This is fine as Google Test
# compiles fast and for ordinary users its source rarely changes.
VPATH += $(GTEST_DIR)/src
VPATH += $(GMOCK_DIR)/src
gtest%.o: WARNINGS =
gtest%.o: CXXFLAGS += -isystem $(GTEST_DIR) -isystem $(GTEST_DIR)/include
gtest%.o: gtest%.cc $(GTEST_SRCS_)
	$(COMPILE.cpp) $(OUTPUT_OPTION) $<

gmock%.o: override INCLUDES += -I$(GMOCK_DIR) -isystem $(GMOCK_DIR)

OPTIMIZED += gtest%.o gmock%.o


include files.mak


### Keep these after files.mak

$(OPTIMIZED) $(patsubst %,%.s,$(OPTIMIZED)): CFLAGS += -O3 -march=$(ARCH)
$(OPTIMIZED) $(patsubst %,%.s,$(OPTIMIZED)): CXXFLAGS += -O3 -march=$(ARCH)

$(USING_GTEST): gtest_main.o gtest-all.o
$(USING_GTEST): CXXFLAGS += -DUSING_GTEST
$(USING_GTEST): INCLUDES += -isystem $(GTEST_DIR)/include
$(USING_GTEST): LDLIBS += -pthread
$(USING_GTEST): LINK.o = $(LINK.cc)

ELF_SIGNATURE = 7f454c46  # ASCII: ^?, E, L, F

clean::
	$(RM) *~ *.o *_moc.cpp *_ui.h
	find . -type f -executable \
	       -exec sh -c "hexdump -n 4 -e '4/1 \"%02x\"' \$$1 | grep -qx $(ELF_SIGNATURE)" is-elf {} \; \
	       -delete
