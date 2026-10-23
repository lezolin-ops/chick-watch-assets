# 固件台账 MANIFEST

> 本表为本项目固件唯一权威清单（2026-10-16 整理）。文件名遵循命名规范：`<设备>_<类型>_<版本>_<YYYYMMDD>_<标识>.bin`；NVS/系统分区快照不带版本号。版本号由本次整理按时间线指派，后续正式发版按语义化规则递增（见 FIRMWARE.md「固件管理规范」）。旧文件名与新文件名一一对应，SHA256 与内容强校验，改名不改变任何字节。

## 命名规范

| 字段 | 取值 | 示例 |
|---|---|---|
| 设备 | `pet_watch` | pet_watch |
| 类型 | `full` 全量16M · `app` 应用镜像 · `nvs` NVS分区 · `sys` 系统分区 | pet_watch_app |
| 版本 | 语义化 `vX.Y.Z`（分区快照省略） | v1.0.0 |
| 日期 | 备份/构建日 YYYYMMDD | 20261015 |
| 标识 | 关键差异或 `pre-<变更>` | sleep-v1 / pre-qspi-fix |

## 完整镜像（full / app）

| 新文件名 | 旧文件名 | 类型 | 版本 | 大小 | SHA256 | 变更说明 | 验收状态 |
|---|---|---|---|---|---|---|---|
| pet_watch_full_v0.0.0_20261011_factory.bin | `factory_flash_16MB.bin` | full | v0.0.0 | 16.00M | `d3068fc8a14f511299b36dc1059d52974f869acbb8e247dd1317650665c3e64d` | 出厂原样全量 16MB，恢复出厂用写回 0x0 | 出厂备份（原样） |
| pet_watch_full_v0.1.3_20261011_pre-wifi-flash.bin | `pre_wifi_flash_20261011.bin` | full | v0.1.3 | 16.00M | `bdedebbf068fe579444931f412a777f9d2b2e9c3507796fc8c4d9fc6fb9179df` | Wi-Fi 刷写前全量快照（含 QSPI 修复+资源压缩） | 历史快照 |
| pet_watch_app_v0.1.1_20261011_pre-qspi-fix.bin | `pet_watch_before_qspi_fix_20261011.bin` | app | v0.1.1 | 8.66M | `d9eb752c2d7ab4e4419feb0c90318db11c70560e5676b801000c99d2a80561b1` | QSPI 灰线修复前旧项目固件（FIRMWARE §3.13 回退用） | 已验收后被替代（回退用） |
| pet_watch_app_v0.1.2_20261011_raw-resource-reference.bin | `pet_watch_raw_resource_reference_20261011.bin` | app | v0.1.2 | 8.81M | `971fa1d30317e7b82cfa5f439531b878d9497afabd8cb1e0d8ef9278b69850cf` | 资源无损压缩对照构建（PET_PACK_RESOURCES=OFF） | 对照用，未宣称与设备烧录版本一致 |
| pet_watch_app_v0.2.0_20261012_wifi-white-password.bin | `pet_watch_wifi_white_password_20261012.bin` | app | v0.2.0 | 3.79M | `c30517888bc1a0b92371d5a9b950629f78428a83ef00cfbd0ae5926aa7a205ec` | Wi-Fi 密码输入/保存与重启自动重连（增量） | 已刷入校验，联网待验收 |
| pet_watch_app_v0.2.1_20261012_wifi-brightness100.bin | `pet_watch_wifi_brightness100_20261012.bin` | app | v0.2.1 | 3.79M | `cfe820b49dfcd2054ab96a43e3110179abe0a936399d901fce8cd7552a16cde8` | 亮度开放 100%（默认 50%，PU-009） | 已刷入校验 |
| pet_watch_app_v0.2.2_20261012_safe-area.bin | `pet_watch_safe_area_20261012.bin` | app | v0.2.2 | 3.79M | `f31ad10ba07e7f98d883a842239f2e6dcb4ff23519e3c4f13ed2a6c65ebc85d4` | 圆角屏幕安全区布局 | 已刷入校验 |
| pet_watch_app_v0.2.3_20261012_wifi-time-sync.bin | `pet_watch_wifi_time_sync_20261012.bin` | app | v0.2.3 | 3.80M | `c800ed952e026bbb0fdf88a6e8db82ad68f4931a5a80107d3535569de5af6198` | 联网校时 + RTC 写回 + Wi-Fi 开关（FIRMWARE 首页） | 已刷入校验，55 秒日志通过 |
| pet_watch_app_v0.2.4_20261012_home-clock-v5.bin | `pet_watch_home_clock_v5_20261012.bin` | app | v0.2.4 | 3.82M | `dd24c37d970b51117ec525226e13324d8dc3811fb368eab98af746edaf87987d` | 首页时钟 v5（历史构建，当前build已更新） | 已刷入校验，交互待验收 |
| pet_watch_app_v0.3.0_20261014_power-stage1.bin | `pet_watch_power_stage1_20261014.bin` | app | v0.3.0 | 3.84M | `768b6c45b63e71414e29ffa2e4d703ffbb93c048496e0c00d82b1ddd70cb13de` | 电源优化阶段 1（POWER_MANAGEMENT.md） | 已刷入校验 |
| pet_watch_app_v0.3.1_20261014_power-configurable.bin | `pet_watch_power_configurable_20261014.bin` | app | v0.3.1 | 3.84M | `c1ad97876df656843ddb04ae81874d013be4ff0e2ed14048b61937c6709ab262` | 电源可配置版：CPU 需求接口 + 80–240MHz + 息屏休眠开关 | 已刷入校验（PU 电源记录） |
| pet_watch_app_v0.3.2_20261014_power-baseline.bin | `pet_watch_power_baseline_20261014.bin` | app | v0.3.2 | 3.84M | `9ef9bfc145499a57da63ca95fc9e510d46a56c1ac6fdc3a3a2022c8f3843da96` | 电源基线（= build-power-baseline/pet_watch.bin） | 基线对照 |
| pet_watch_app_v0.4.0_20261014_battery-log.bin | `pet_watch_battery_log_20261014.bin` | app | v0.4.0 | 3.84M | `f58b5967ef0a33b3a1196a97505a29a70ab748e03f77c5023eb345434faebc7f` | 后台电池记录（BATTERY_LOG.md） | 已刷入，哈希与启动检查通过 |
| pet_watch_app_v0.4.1_20261014_offline-policy.bin | `pet_watch_offline_policy_20261014.bin` | app | v0.4.1 | 3.85M | `29d9256ae1a8d549cfb677457d9bfcb17cbacb5d05b1ebe24865d6e0fceadfb6` | 离线 15 分钟 Wi-Fi 策略 | 已刷入，边界模拟通过 |
| pet_watch_app_v0.4.2_20261014_network-switches.bin | `pet_watch_network_switches_20261014.bin` | app | v0.4.2 | 3.85M | `272d2f7ac791652e01c29f086338ad9eff8c96e3cbdb36117569ac85281dc09b` | 网络开关（联网需求保护） | 已刷入，启动日志通过 |
| pet_watch_app_v0.4.3_20261014_wifi-backoff.bin | `pet_watch_wifi_backoff_20261014.bin` | app | v0.4.3 | 3.85M | `120bacb6a8e09ebe258bdd1b3c19e5b04313b225de3036eac639c5a6390abc76` | Wi-Fi 退避（30/60/120 秒阶梯） | 已刷入，长阶梯待验收 |
| pet_watch_app_v0.4.4_20261014_panel-sleep.bin | `pet_watch_panel_sleep_20261014.bin` | app | v0.4.4 | 3.85M | `e1379ee7ea7edf2beaf6432807309bb6c7a217e12f99e0263f52d7331ca273e3` | 屏幕休眠（AMOLED 亮度归零/恢复） | 已刷入，启动日志通过 |
| pet_watch_app_v0.4.5_20261014_idle-diagnostics-v1.bin | `pet_watch_idle_diagnostics_v1_20261014.bin` | app | v0.4.5 | 3.85M | `9d42ad36e172b74458c901596aac37c84b3d8101fbb86b5c1a41dbfd6de75ee4` | 空闲外设诊断 v1 | 已刷入，哈希校验通过 |
| pet_watch_app_v0.4.6_20261014_idle-diagnostics.bin | `pet_watch_idle_diagnostics_20261014.bin` | app | v0.4.6 | 3.85M | `007b4787e7040cb1a8f724725edceefeae3cb8990e1fb12cdec707c4425b1517` | 空闲诊断修订版（当日更新） | 已刷入，哈希校验通过 |
| pet_watch_app_v1.0.0_20261015_sleep-v1.bin | `pet_watch_sleep_v1_20261014.bin` | app | v1.0.0 | 3.86M | `5bea131432af81690e89498ba7448eb801f6a6bcadbd10a5a4788b8ebe78afd0` | 睡眠 v1（三阶段）+ 已有优化合并（PU-044，最新验收版） | 已刷入验收（PU-044） |

## 待验收候选固件

软件计步1.2.0：[候选说明](candidates/v1.2.0/README.md)，4105568字节，SHA256 `6dd40667862a9246e315c826915b557e36ef5d4a1805a6d1880970f7e4de3528`。已构建/离线回归通过，应用刷写哈希及92秒启动/息屏后读取通过；真机计步/功耗待验收，外部驾驶样本存在误计，不能作为已验收版推荐。

| 文件 | 版本 | 大小（字节） | SHA256 | 状态 |
|---|---|---:|---|---|
| [pet_watch_app_v1.1.2_20261018_steps-resume.bin](candidates/v1.1.2/pet_watch_app_v1.1.2_20261018_steps-resume.bin) | v1.1.2 | 4104368 | `07dfb16908f7314e1dd69dfed28b35c3887a5bab204cd60a495f2ef243cb35f1` | 已刷应用/哈希通过，90秒启动及息屏后IMU读取通过；行走计数/准确度待复测，地址0x10000 |
| [pet_watch_app_v1.1.1_20261018_steps-toggle.bin](candidates/v1.1.1/pet_watch_app_v1.1.1_20261018_steps-toggle.bin) | v1.1.1 | 4103536 | `d64c306c89b225f68658e6025838e00e097b63b7b6b744e4bd0af71a0677eb18` | 当前推荐候选版；主屏时钟开关联动步数，电量保留；已构建/主机测试通过，未刷写，地址0x10000 |
| [pet_watch_app_v1.1.0_20261018_steps-status.bin](candidates/v1.1.0/pet_watch_app_v1.1.0_20261018_steps-status.bin) | v1.1.0 | 4103552 | `9391680bf60fb9518c9d0bebfc478f61f2a1ceedb1e1c401424a4f7ca69e6270` | 已构建/主机测试通过，未刷写/启动检查，真机验收待完成；应用地址0x10000 |

此候选版按用户要求交由自制刷机软件刷写，附[说明](candidates/v1.1.0/README.md)、SHA256SUMS与manifest.json；未假定软件兼容JSON。最新验收版仍为v1.0.0。

## 分区快照（nvs / sys）

| 新文件名 | 旧文件名 | 类型 | 大小 | SHA256 | 变更说明 |
|---|---|---|---|---|---|
| pet_watch_nvs_20261014_pre-battery-log.bin | `pre_battery_log_20261014_nvs.bin` | nvs | 24K | `a1749a435e52e276f15299428d8878886aa76c15a82d28e2e36a25dbc5888e16` | 变更前 NVS 快照（电池记录） |
| pet_watch_nvs_20261014_pre-idle-diagnostics.bin | `pre_idle_diag_20261014_nvs.bin` | nvs | 24K | `3d9f6961c7b593bb8fbdbc13cb7955ae1b00272839670e7c1e18e8dd6ef60079` | 变更前 NVS 快照（空闲诊断） |
| pet_watch_nvs_20261014_pre-network-switch.bin | `pre_network_switch_20261014_nvs.bin` | nvs | 24K | `8456829fdb31d16a864e81709a75c7aea44ab8144f533a53960a19c410013557` | 变更前 NVS 快照（网络开关） |
| pet_watch_nvs_20261014_pre-offline-policy.bin | `pre_offline_policy_20261014_nvs.bin` | nvs | 24K | `d9268c57c1d1a548379515b4ca1331a01bdf3aa6eb7b5404a6ee8691b7c93f9a` | 变更前 NVS 快照（离线策略） |
| pet_watch_nvs_20261014_pre-panel-sleep.bin | `pre_panel_sleep_20261014_nvs.bin` | nvs | 24K | `723a44ad1408ee2d3dede052c0707fe99b7e0e59174c12c66b559b5974d75e47` | 变更前 NVS 快照（屏幕休眠） |
| pet_watch_nvs_20261014_pre-sleep-v1.bin | `pre_sleep_v1_20261014_nvs.bin` | nvs | 24K | `34e46416ce3feb4600695b4d256411321b5dabbfbd1e39215207105e1786e2d1` | 变更前 NVS 快照（睡眠 v1） |
| pet_watch_nvs_20261014_pre-wifi-backoff.bin | `pre_backoff_20261014_nvs.bin` | nvs | 24K | `6b1f8f149a7aa32b5d4243b8106c89df5b6dc1997e2e3901b665f64b49661220` | 变更前 NVS 快照（Wi-Fi 退避） |
| pet_watch_sys_20261013_pre-power.bin | `pre_power_20261013_system64k.bin` | sys | 64K | `6826c0d4d96b82b9553ea453be28731ed5dccb446b6b73482e07797d8e379b65` | 变更前 64KB 系统分区快照（电源阶段） |

## 关联说明

- 最新验收版：`pet_watch_app_v1.0.0_20261015_sleep-v1.bin`（SHA `5bea131432af81690e89498ba7448eb801f6a6bcadbd10a5a4788b8ebe78afd0`），对应 release 目录副本。
- 当前构建目录与备份对应关系：`firmware/build/pet_watch.bin` = v1.1.2候选版（steps-resume，2026-10-18更新）；`build-power/pet_watch.bin` = v1.0.0（sleep-v1）；`build-power-baseline/pet_watch.bin` = v0.3.2（power-baseline）。
- 本目录内 `*.json` / `*.log` / `*.csv` 为各版本刷写与验证证据，不参与命名规范。
- 刷机/改版前备份命名带 `pre-<变更>`；刷前用本表 SHA256 核对。


2026-10-19：v1.1.3计步诊断候选版已应用刷写/哈希通过，参数重新下发及读回/握手核对通过；实际行走待验收。见[candidate](candidates/v1.1.3/README.md)，不替换正式稳定版。

2026-10-19：新增v1.1.4参数格式对照诊断版，4,106,192字节，SHA256 `67a9cf78e450a0d57d4ebdf09c7e9797482f6f616259e53bdc648b6663cf67f9`。应用刷写哈希、完整启动与三格式读回/握手通过；运动对照结果见[方案及记录](../docs/reference/steps-format-comparison-20261019.md)，不替换已验收稳定版本。

2026-10-19：官方包计步示例适配版1.1.5-official已应用刷写/哈希及95秒启动、面板休眠后静置读取检查通过；实际行走未测。测试计数仅RAM，日历史保留；来源/适配边界见experiments/qmi-official/README.md（本项目根目录）。不替换已验收稳定版。

2026-10-19：用户试走1.1.5仍0。1.1.6命令IRQ自检0边沿后定位IDF适配漏配GPIO21 INPUT（原库setPins无效）；1.1.7-irqfix显式输入修复已刷入，80秒启动/面板休眠后读取通过，同一命令自检2边沿、原CTRL8恢复成功。原库不改，日历史保留；修复后实际行走尚未复测，不当作计步已修复。详见根目录experiments/qmi-official/README.md和PU-090。
