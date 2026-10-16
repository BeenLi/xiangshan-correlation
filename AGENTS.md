# AGENTS.md

XiangShan 昆明湖 V2 RTL 与 XS-GEM5 的 correlation 工作区。版本、目录结构和常用命令见 `README.md`。

## 检查点归档命名规则

生成 SimPoint 检查点时，`generate_checkpoint.py --archive-id` 的值必须按以下格式命名：

```
<负载>-<编译器><主版本号>-<优化级别>[-<指令集>]-<间隔>
```

| 字段 | 写法 | 例子 |
|---|---|---|
| 负载 | 负载集或程序名，全小写 | `spec06`、`spec17`、`coremark`、`gapbs` |
| 编译器与主版本号 | 编译器名加主版本号，不写次版本号 | `gcc15`、`clang19` |
| 优化级别 | 编译优化选项去掉横线 | `o2`、`o3`、`ofast` |
| 指令集 | 只在不是默认的 `rv64gcb` 时才写 | `rv64gcbv`、`rv64gcbh` |
| 间隔 | SimPoint 间隔，以百万条指令为单位，加 `m` | `20m`、`50m` |

规则：
- 字段之间用短横线连接，全部小写，不用下划线和空格。
- 编译器版本和优化级别必须写。它们直接决定指令流，不同版本的检查点不能混在一起比较。
- 编译器版本以实际编译出的程序为准，不要凭印象写。可以查 workload-builder 所用 Buildroot 工具链的版本，或查程序运行时打印的编译器信息。
- 同一个名字只对应一次生成。改了编译器、优化级别、指令集或间隔，就必须换一个新名字，不要覆盖旧归档。
- 只跑 SPEC 的一部分子项时，在负载字段后加上子项，例如 `spec06int-gcc15-o2-20m`。

例子：

| 场景 | 归档名 |
|---|---|
| SPEC CPU2006，GCC 15，-O2，20M 间隔 | `spec06-gcc15-o2-20m` |
| SPEC CPU2017，GCC 15，-O3，开启向量扩展，20M 间隔 | `spec17-gcc15-o3-rv64gcbv-20m` |
| workload-builder 的 Linux 版 CoreMark，GCC 15.1.0，-O2，20M 间隔 | `coremark-gcc15-o2-20m` |

检查点都由 workload-builder 打包成 Linux 镜像后生成，所以名字里不再写 `linux`。

已有的 `checkpoints/coremark-linux-20m` 是在这条规则确立之前生成的，属于例外。它的实际参数是 GCC 15.1.0、-O2、`rv64gcb`、20M 间隔，按新规则应叫 `coremark-gcc15-o2-20m`。
