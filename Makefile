export CC := zig cc
export AR := zig ar
export PATH := $(CURDIR)/scripts:$(PATH)
export ROOTDIR := $(CURDIR)
export CFLAGS += -std=c11 -Wall -Werror -g -DDEBUG

# src/
HDRS := $(wildcard src/*.h)
SRCS := $(wildcard src/*.c)
OBJS := $(addprefix build/, $(notdir $(SRCS:.c=.o)))

BUILD_DEPS = $(HDRS) Makefile build/.mk

# compile the main executable
build/main: $(OBJS) $(BUILD_DEPS)
	$(CC) -o $@ $(CFLAGS) $(OBJS) $(DEPS_LDFLAGS) -lc

# compile a src file
build/%.o: src/%.c $(BUILD_DEPS)
	$(CC) -c $(CFLAGS) -o $@ $<

# create the build directory
build/.mk:
	mkdir -p build
	touch build/.mk

.PHONY: clean
clean:
	rm -rf build
