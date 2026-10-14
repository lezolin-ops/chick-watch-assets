# 官方原始资料资产

2026-10-14保存。来源、大小、SHA256及取得方式见[manifest.json](manifest.json)。保留原版权/许可；这些是参考文件，不自动替换固件驱动。

- 原理图 `schematic-v1.0.pdf`：官方GitHub，已检查第1页相关电路。
- 手册 `axp2101-datasheet-v1.0.pdf`：官方示例包附件，已检查电源/睡眠/ADC章节，未通读全部64页。
- `port_axp2101.cpp`：本型号官方PMU示例，已核对配置和遗留注释。
- XPowersLib.h及Interface.cpp/.hpp：官方示例依赖参考，保留MIT许可；不是板级供电正确性的独立证明。

综合结论与待核验项见[电源管理说明](../../../firmware/POWER_MANAGEMENT.md)。main分支会变化，使用本地摘要识别确切资产版本。

- `co5300-datasheet-v0.01.pdf`：2026-10-14从微雪文件站保存，检查睡眠进出pp143～146和显示开关pp155～158，未全文通读，SHA见manifest。
