# 图源说明

- `official/`：香山官方文档站的图（https://docs.xiangshan.cc/projects/design/zh-cn/nanhu/frontend/ ，南湖版前端文档；昆明湖 V2 的前端结构与之一致，参数以本仓库 `XiangShan/src/main/scala/xiangshan/Parameters.scala` 为准）。昆明湖整体架构图直接引用仓库自带的 `XiangShan/images/xs-arch-kunminghu.svg`。
  - `frontend.png` 前端总览，`bpu.svg` 三级预测流水（BP1/BP2/BP3），`ftq.svg` FTQ 与各模块的接口，`IFU.png` IFU 流水，`tage.png`、`ITTAGE.png` 预测器原理。
- `drawn/`：官方没有的图，用 verilog-diagram skill 的 `arch2drawio.py` 从源码读出后画的。每张图三个文件：`.yaml` 是图描述（改图改它再重新渲染），`.drawio` 可在 draw.io 里编辑，`.png` 用于嵌入文档。

重新渲染：

```bash
python3 ~/.claude/skills/verilog-diagram/scripts/arch2drawio.py render drawn/bpu_override_rtl.yaml -o drawn/bpu_override_rtl.drawio --png
```
