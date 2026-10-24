# AGENTS.md

XiangShan 昆明湖 V2 RTL 与 XS-GEM5 的 correlation 工作区。版本、目录结构和常用命令见 `README.md`。

## 脚本位置

自己写的脚本（编译、运行、生成检查点、环境设置等）一律放在 `scripts/`，不要放进 GEM5-XS、XiangShan 等组件目录。脚本用自身位置定位仓库根目录，例如 `$(dirname "$0")/..`，不要写绝对路径。

## 不要暴露服务器信息

这是公开仓库。提交的任何文件，包括文档、脚本、日志、元数据和配置，都不能出现服务器的绝对路径、用户名和主机名。

- 仓库内的路径写成相对仓库根目录的路径，或用 `$CMODEL_ROOT`。
- 用户目录下的依赖写成 `$HOME/...` 或 `~/...`。
- 用户名和主机名写成 `user@host` 这类占位符。
- 仿真器、构建工具生成的日志和元数据会自动写入绝对路径，Linux 启动日志里还有编译内核的 `用户名@主机名`。提交前先替换掉。
- 提交前在暂存的改动里（`git diff --cached`）搜一遍 home 目录前缀、用户名和主机名，确认没有命中。新加的文件也要查到。
- 改日志时按字节替换，不要改动原有的换行符（日志里常有 `\r`）。

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

## 学习文档（Docs/learning）规则

`Docs/learning/README.md` 是学习计划和索引。笔记按类别编号，每类一篇总览加若干分篇：

| 编号 | 类别 | 总览 | 状态 |
|---|---|---|---|
| 00 | 两种写法：gem5 的建模手法（TimeBuffer、tick 与事件、squash、补空泡、配置生效链）与对应的 RTL 写法 | `00-0-modeling-overview.md` | 框架 |
| 01 | 流水线（学习计划第一步） | `01-0-pipeline-overview.md` | 进行中 |
| 02 | 存储层次：LSU、DCache、L2、预取、TLB/PTW（第二步） | `02-0-memory-overview.md` | 框架 |
| 03 | Top-down 计数器口径（第三步，有了计算和存储之后） | `03-0-topdown-overview.md` | 框架 |

- 总览命名 `NN-0-<主题>-overview.md`，只放地图（架构图、对照图、待核实清单）和分篇表，不写细节。
- 分篇命名 `NN-M-<主题>.md`，每一项精读一篇，例如 `01-2-bpu-override.md`。内容紧密相关的可以合进同一篇（如 `01-1` 同时讲 FTQ 与 FTB）。改编号或合并时，同步改总览、README 和其他分篇里的链接。
- 新开一类时先搭框架：总览只写标题和"（待写）"，并在 README 的索引表和本表登记。

每一项都先读 RTL，再对照 gem5；笔记里也按这个顺序写：先讲 RTL 怎么实现，再讲 gem5 怎么建模，最后是两边对照和结论。

写精读笔记时仿照已有分篇（如 `01-2-bpu-override.md`）：
- 开头两段引用块：第一段说明是仓库内学习文档、图在 `figs/`、`文件:行号` 可点击；第二段写"属于 [NN 总览](NN-0-<主题>-overview.md)"，并说明 ✅（从代码或配置读出）、❓（待核实）、🆕（课本没有）三个标记。
- 引用代码一律写成相对本文件的链接并带行号，例如 [`IFU.scala:245`](../../XiangShan/src/main/scala/xiangshan/frontend/IFU.scala#L245)。
- RTL 和 gem5 两边对照着写，结论落到"第几拍、哪根信号、哪个 delay 参数"；精读结论单独成节，标题写成"精读结论（日期）：一句话结论"。
- **每篇至少一张辅助理解的图**，二选一：
  - 官方或互联网上现成的架构图：下载到 `figs/official/`，在 `figs/README.md` 写明来源链接。
  - 没有现成的图时，用本地的 verilog-diagram skill（`arch2drawio.py`）从代码画：`.yaml`、`.drawio`、`.png` 三件放 `figs/drawn/`，yaml 头部注释写清依据的代码行；渲染后用 drawio-skill 的 `validate.py` 检查，error 必须为 0，并目视检查 PNG。结构图画不出来的（位域、指针位置这类）可以手写 SVG，在 `figs/README.md` 注明。
- 精读完成后：`Docs/learning/README.md` 里对应的待办打勾，括号里写日期和笔记链接；总览的分篇表和待核实清单同步更新（❓ 改 ✅ 并写一行结论）。
