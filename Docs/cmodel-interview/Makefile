# 编译并运行全部参考答案：make；只编译某一题：make build/04_pipeline
# 自测自己的代码：把 mine/NN_xxx.cpp 写好后 make mine（全部）或 make mine N=01（只跑 01）
CXX      ?= g++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Werror
SRCS     := $(sort $(wildcard solutions/*.cpp))
BINS     := $(patsubst solutions/%.cpp,build/%,$(SRCS))

# 自己的代码：不开 -Werror 免得被告警卡住；开 ASan/UBSan 抓越界和未定义行为，-O0 -g 方便 gdb
MINE_CXXFLAGS ?= -std=c++17 -O0 -g -Wall -Wextra -fsanitize=address,undefined
N         ?=
MINE_SRCS := $(sort $(wildcard mine/$(N)*.cpp))
MINE_BINS := $(patsubst mine/%.cpp,build/mine/%,$(MINE_SRCS))

.PHONY: all run mine clean
all: run

build/%: solutions/%.cpp
	@mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(BINS)
	@for b in $(BINS); do ./$$b > build/$$(basename $$b).log 2>&1 && tail -n 1 build/$$(basename $$b).log || { echo "FAILED: $$b"; cat build/$$(basename $$b).log; exit 1; }; done

build/mine/%: mine/%.cpp
	@mkdir -p build/mine
	$(CXX) $(MINE_CXXFLAGS) -o $@ $<

# 自己的代码直接把输出打到终端，方便看 printf 调试信息
mine: $(MINE_BINS)
	@test -n "$(MINE_SRCS)" || { echo "mine/ 下没有匹配 '$(N)*.cpp' 的文件"; exit 1; }
	@for b in $(MINE_BINS); do echo "== $$b"; ./$$b || { echo "FAILED: $$b"; exit 1; }; done

clean:
	rm -rf build
