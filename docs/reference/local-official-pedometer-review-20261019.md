# 本地官方包计步实现复核

日期：2026-10-19。只读检查本机 `/Users/Zhuanz1/esp/waveshare-2.06`，Git HEAD `b099739`，本次未更新远端或刷写官方镜像。

- `examples/arduino/libraries/SensorLib/examples/QMI8658_PedometerExample/QMI8658_PedometerExample.ino` 是随官方包携带的Lewis He通用示例，参数50/200/100/200/20/10/0/4、±2g/62.5Hz，与当前项目参数一致。通过INT1回调读24位硬件计数。默认SPI，CS34/IRQ33，I2C分支默认17/18，不是本板14/15和INT21的直接可刷工程。
- `examples/arduino/libraries/SensorLib/src/SensorQMI8658.hpp` 1006行起有两组CAL参数、0x0D命令，1056行读取0x5A三字节，1071行启用CTRL8 bit4。项目此前研究和移植的就是这条硬件计数路线，并非已实测软件计步算法。
- `examples/arduino/04_LVGL_QMI8658_ui/04_LVGL_QMI8658_ui.ino` 配置加速度±4g/1000Hz，读取加速度/陀螺仪并画曲线，未调用configPedometer/enablePedometer。不能将姿态图表能运行当计步已验证。
- 本地ESP-IDF六个顶层示例源码检索未发现pedometer/计步/step_count实现；03_esp-brookesia的main/components也未命中QMI8658或计步关键词。此为该本地版本源码检索结论，不概括所有官方版本。
- `FirmWare/` 当前仅有小智`ESP32-S3-Touch-AMOLED-2.06-xiaozhi-251104.bin`及README；不能通过文件名认定镜像内无计步。未反汇编或刷机验证其运行功能。

结论：本地有通用硬件计步示例且此前已研究配置/握手/读取；尚无“本型号官方固件在这块手表实测能计步”的证据，也没有完整对照运行过原版计步示例。若做官方示例对照，应先适配本板I2C/中断/电源，保留原版算法、单独构建并按发布流程保护现有应用/NVS；不能直接刷通用默认引脚示例。

## 官方示例适配执行

用户随后明确要求刷入。已使用同一官方包未修改SensorLib，按原计步示例顺序适配I2C14/15与INT1 GPIO21、原生ESP-IDF后端，构建1.1.5-official。保留产品显示/电源外壳，排除生产计步worker；不称为完整原版出厂固件。详见[适配说明](../../experiments/qmi-official/README.md)及PU-089。已保护1.1.4回退和当前NVS，实际刷写/启动/行走状态随该记录维护。

补充：用户提供重力球应用截图后，检查相关04_Immersive_block/main/main.c的水平校准为应用RAM内X/Y均值扣除，未写传感器偏移寄存器；尚未验证截图中完整Gravitysphere应用的源码对应关系，不能把这个校准当成内置计步的必要前置。用户本地2021 Rev0.6与之前2022 RevA差异见[qmi8658c-user-manual-rev06-review-20261019.md](qmi8658c-user-manual-rev06-review-20261019.md)。
