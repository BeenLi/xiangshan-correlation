# Difftest 与 NEMU 参考模型

本文说明本工作区里 difftest 的工作方式：参考模型导出了哪些函数、被测模型怎样调用它们，以及为什么一个"标准答案"模型还需要被测模型告诉它一些事情。文中的源码位置都对应 `README.md` 里锁定的版本。

## 1. 三个角色

| 角色 | 在本工作区里是谁 | 职责 |
|---|---|---|
| 被测模型，DUT | XiangShan RTL 编出的 EMU，或者 XS-GEM5 | 真正要验证的处理器模型 |
| 参考模型，REF | NEMU 编成的 `riscv64-nemu-interpreter-so` | 只按 ISA 语义执行指令，不模拟时序，作为标准答案 |
| 测试框架 | RTL 侧是 XiangShan 的 `difftest` 子模块；GEM5 侧是 `GEM5-XS/src/cpu/difftest.cc` 和 `base.cc` | 在运行时用 `dlopen` 加载 REF，每当 DUT 提交指令，就驱动 REF 执行并比对状态 |

difftest 不是一个程序，而是 DUT 和 REF 之间的一份调用约定：REF 导出一组 C 函数，测试框架按约定的时机调用它们。

## 2. 调用约定：REF 导出的函数

两个 so 文件导出的函数名几乎相同，用 `nm -D --defined-only <so>` 可以列出。RTL 用的 xs-ref 只多一个 `difftest_raise_critical_error`。实现在 `NEMU/src/cpu/difftest/ref.c`。

### 2.1 按用途分类

| 类别 | 函数 | 作用 |
|---|---|---|
| 初始化 | `difftest_init`、`difftest_init_v2`、`difftest_set_ramsize`、`difftest_load_flash` | 建立 REF 的内存和 ISA 状态。`init_v2` 会检查 DUT 传入的寄存器结构体大小，与 REF 不一致就直接终止 |
| 状态搬运 | `difftest_memcpy`、`difftest_memcpy_init`、`difftest_get_backed_memory`、`difftest_regcpy`、`difftest_csrcpy`、`difftest_uarchstatus_cpy` | 在 DUT 和 REF 之间拷贝内存、寄存器和 CSR。方向参数取 `DIFFTEST_TO_REF` 或反方向。`get_backed_memory` 让 REF 直接共用 DUT 的内存，省去拷贝 |
| 推进执行 | `difftest_exec`、`difftest_guided_exec`、`difftest_skip_one` | `exec(n)` 让 REF 执行 n 条指令。`guided_exec` 让 REF 按 DUT 给出的结果执行一条指令。`skip_one` 让 REF 不执行这条指令，直接采用 DUT 的写回值 |
| 注入不确定事件 | `difftest_raise_intr`、`difftest_raise_nmi_intr`、`difftest_interrupt_delegate`、`difftest_virtual_interrupt_is_hvictl_inject`、`difftest_non_reg_interrupt_pending`、`difftest_sync_aia`、`difftest_raise_mhpmevent_overflow`、`difftest_uarchstatus_sync` | 把只有 DUT 才知道的事件告诉 REF，见第 4 节 |
| 额外检查 | `difftest_store_commit`、`difftest_get_store_event_other_info`、`difftest_query_ref`、`difftest_raise_critical_error` | `store_commit` 按提交顺序比对每次写内存的地址、数据和掩码 |
| 回滚 | `difftest_store_log_reset`、`difftest_store_log_restore`、`difftest_runahead_init` | 配合 DUT 侧的快照与回滚功能使用 |
| 调试 | `difftest_display`、`isa_reg_display`、`difftest_enable_debug` | 出错时打印 REF 的完整状态 |

### 2.2 同名不等于兼容

函数名只是约定的一部分，下面三点也必须一致：
- **参数个数和含义。** 同一个函数的签名会随编译配置变化。例如开启 `CONFIG_LIGHTQS` 时，`difftest_regcpy` 从 2 个参数变成 4 个。
- **结构体布局。** `regcpy` 拷贝的是一个寄存器结构体，每加入一个新 CSR 或新扩展，字段位置就可能变化。
- **REF 实现的功能集。** REF 由 defconfig 决定实现哪些扩展。REF 和 DUT 的功能集不一致时，同一条指令两边结果不同，会被误报为错误。

所以每个 DUT 都要搭配和自己的测试框架同期、同配置的 REF。本工作区的配对是：

| DUT | 测试框架代码 | REF | NEMU 提交 | defconfig |
|---|---|---|---|---|
| XS-GEM5，KMH-V2 标签 | GEM5 源码自带 | `NEMU/build/riscv64-nemu-interpreter-so` | `d6fb8ed4` | `riscv64-gem5-ref_defconfig` |
| XiangShan V2 RTL | `XiangShan/difftest` 子模块 | `XiangShan/ready-to-run/riscv64-nemu-interpreter-so` | `b9801f05` | `riscv64-xs-ref_defconfig` |

两份 defconfig 的主要区别：

| 配置项 | gem5-ref | xs-ref |
|---|---|---|
| 物理地址位数 | 36 | 48 |
| PMP 表项 | 16 | 64 |
| PMA 表项 | 16 | 32 |
| Smstateen、AIA 与 IMSIC、CBO、Sdtrig、Smrnmi、Smdbltrp | 不开 | 开 |
| `CONFIG_DIFFTEST_REF_FOR_GEM5` | 开 | 不开 |

xs-ref 对齐的是 V2 RTL 的功能集，gem5-ref 对齐的是 GEM5 的功能集。gem5-ref 不开 Smstateen，说明 GEM5 没有实现 stateen 寄存器。这正是检查点恢复程序需要为 GEM5 单独编译一份的原因，详见 `README.md` 的"恢复程序"一节。

## 3. 一步比对的流程

DUT 每提交一条指令，测试框架做以下几件事：

1. 如果这条指令是需要跳过的指令，就调用 `skip_one`，或者把 DUT 的结果通过 `regcpy` 写进 REF，然后直接进入下一条。
2. 如果这条指令触发了中断，就调用 `raise_intr`；如果触发了特定例外，就调用 `guided_exec`。
3. 否则调用 `exec(1)`，让 REF 正常执行一条。
4. 调用 `regcpy` 把 REF 的状态取回来，与 DUT 比较 PC、通用寄存器、浮点寄存器和 CSR。
5. 有任何不同，就调用 `display` 打印两边状态，然后中止仿真。

源码位置：

| 步骤 | RTL 侧 | GEM5 侧 |
|---|---|---|
| 普通执行与跳过 | `difftest/src/test/csrc/difftest/checkers/instructions.cpp` | `GEM5-XS/src/cpu/base.cc` 的 `diffWithNEMU` |
| 中断与例外 | `checkers/traps.cpp` 的 `do_interrupt` 和 `do_exception` | `base.cc` 的 `difftestRaiseIntr` 和调用 `guided_exec` 的函数 |
| LR/SC 与中断待决状态 | `checkers/sync_states.cpp` | `diffWithNEMU` 开头对 SC 指令的处理 |
| 写内存比对 | `checkers/store.cpp` | `base.cc` |
| 跳过哪些 CSR | 见第 4 节 | `GEM5-XS/src/cpu/difftest.cc` 里的 `skipCSRs` |

RTL 侧比对的内容比 GEM5 多。除了寄存器和写内存事件，RTL 还在设计里插入了原子操作、LR/SC、L1 TLB、L2 TLB 等探针，每个探针都有对应的检查器，见 `difftest/src/test/csrc/difftest/difftest.cpp` 的 `init_checkers`。

## 4. 为什么参考模型需要被"告知"

NEMU 是一个功能模型：给定同样的初始状态和同样的指令，它总能算出同样的结果。但 RISC-V 规范有意把一些结果留给具体实现或运行时机来决定，这些结果 NEMU 靠自己是算不出来的。对这类情况，difftest 的原则是：**REF 不去猜，由 DUT 告诉 REF 发生了什么，REF 照做，然后继续比对后面的指令。**

| 不确定事件 | 为什么 REF 算不出来 | 处理方式 |
|---|---|---|
| 外设和非缓存访问 | 串口状态、定时器等外设的状态在 DUT 的平台里，REF 没有对应的设备状态 | RTL 把这类指令标记为跳过，条件是 `isMMIO`、`isNCIO`。GEM5 对严格有序的访问调用 `regcpy`，把 DUT 的结果写进 REF |
| 性能计数器和周期计数 | `mcycle`、`minstret`、`hpmcounter` 的值取决于微架构时序，功能模型没有周期的概念 | RTL 的跳过条件包括 `isPerfCnt`。GEM5 在 `skipCSRs` 里列出这些 CSR，比对时用 DUT 的值覆盖 REF |
| 异步中断 | 中断在哪条指令之后被响应，取决于 DUT 的流水线状态和外部事件到达的时刻 | DUT 在响应中断的那一刻调用 `raise_intr`。委托关系、NMI、虚拟中断注入分别用 `interrupt_delegate`、`raise_nmi_intr`、`virtual_interrupt_is_hvictl_inject` 同步 |
| 来自平台的中断待决位 | PLIC、AIA 等中断控制器的状态不在 CPU 寄存器里 | `non_reg_interrupt_pending` 和 `sync_aia` |
| 计数器溢出中断 | 溢出时刻取决于性能计数器，而计数器取决于时序 | `raise_mhpmevent_overflow` |
| 页错误等部分例外 | 规范允许 DUT 缓存地址翻译。页表被修改而尚未执行 `sfence.vma` 时，DUT 可能用旧的 TLB 表项，NEMU 没有 TLB，两边可能一边出错一边不出错。硬件错误例外也只有 DUT 知道 | RTL 对指令页错误、加载页错误、存储页错误、客户机页错误和硬件错误调用 `guided_exec`，把例外号和 `mtval`、`stval` 等值一起传给 REF，强制 REF 产生同样的例外 |
| SC 指令成功与否 | 规范允许 SC 因实现原因失败，比如对应的缓存行被替换或保留超时 | 每次 SC 之后，DUT 调用 `uarchstatus_sync` 传入 `sc_fail` |
| 多核下的内存顺序 | 多个核写同一地址的先后顺序取决于缓存一致性协议的时序 | 使用全局的参考内存和 `store_commit` 按提交顺序比对 |

这种做法的代价是：被"告知"的那一步本身没有被验证。比如 DUT 说某次外设读返回了某个值，REF 只能照单全收。所以 difftest 能保证的是，在这些事件之外，DUT 的执行与 ISA 语义一致。

## 5. 与 correlation 的关系

difftest 只检查功能正确性，不参与任何性能统计。它的作用是保证 GEM5 和 RTL 执行的是同一条指令流，这样两边的性能差异才有比较意义。IPC 和各项性能计数器的对比，要看 GEM5 的 `stats.txt` 和 RTL 的 `simulator_err.txt`。
