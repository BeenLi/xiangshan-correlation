# 编译并运行全部参考答案：make；只编译某一题：make build/04_pipeline
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror
SRCS     := $(sort $(wildcard solutions/*.cpp))
BINS     := $(patsubst solutions/%.cpp,build/%,$(SRCS))

.PHONY: all run clean
all: run

build/%: solutions/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(BINS)
	@for b in $(BINS); do ./$$b > build/$$(basename $$b).log 2>&1 && tail -n 1 build/$$(basename $$b).log || { echo "FAILED: $$b"; cat build/$$(basename $$b).log; exit 1; }; done

clean:
	rm -rf build
