# 官方包计步示例的本板适配对照

2026-10-19，用户授权刷入官方示例并休息后继续核验。

来源为本地Waveshare `ESP32-S3-Touch-AMOLED-2.06` 仓库commit `b099739`内SensorLib，而非已证明可计步的出厂镜像。原`.ino`及SensorLib相关头文件保留副本和SHA256；vendor文件未修改，保留原MIT许可。

适配仅将原示例运行在现有ESP-IDF5.4.2/板级显示电源外壳：I2C使用BSP已建立的GPIO14/15总线，INT1映射GPIO21 CHANGE中断，调用未修改SensorLib的原始begin（包含软复位、复位结果等待、COPY_USID）、±2g/62.5Hz、启用加速度、原八项计步参数、INT1使能及回调、500ms循环。原生ESP-IDF后端替代Arduino Wire/HAL。不能称为完整原版Arduino固件或出厂固件。

`WATCH_OFFICIAL_PEDOMETER=ON`在编译时排除生产watch_steps.c，替换为本目录适配器，不会并行驱动同一IMU。默认OFF，正常固件仍用生产实现；独立build-official目录和PROJECT_VER=1.1.5-official，不改正常version.txt。

额外诊断：原回调日志保留；启动前10分钟每2秒记录原始步数、时间戳、g单位加速度、GPIO中断数和状态，之后只按需STEPS_STATUS读取。产品显示外壳保留，屏幕步数为测试会话raw，不写pet_steps或日历史；时钟/电量及Wi-Fi省电策略沿用。仅实际初始化/读取/update持统一AWAKE需求，500ms等待期间释放。该诊断循环不是功耗优化交付，不给续航承诺。

生产参数重配/格式实验命令在该构建中拒绝，防止混用。重启重跑原setup。SensorLib配置函数没有完整传播全部内部命令错误，因此函数返回和使能成功不构成算法生效证明；日志和真实行走验收分开记录。原生I2C后端有无限等待调用，未擅改官方库，属于该对照构建保留的限制。

素材仍使用PET_PACK_RESOURCES=ON。正式分区及NVS不变，只刷应用0x10000；回退使用已核验的1.1.4或稳定1.0.0，并记录新NVS备份。用户休息期间只检查启动/静置/息屏读取，不宣称行走已通过。

构建：加载本机IDF5.4环境，执行`idf.py -C firmware -B firmware/build-official -D PET_PACK_RESOURCES=ON -D WATCH_OFFICIAL_PEDOMETER=ON -D PROJECT_VER=1.1.5-official reconfigure build`。

## 2026-10-19真机执行结果

已只刷应用0x10000，压缩传输2,957,303字节，哈希通过。95秒完整受控重启记录仅一个ROM启动头，版本1.1.5-official、显示/8MB PSRAM正常；begin读到修订7c、内部固件01031e，原库配置/使能完成，GPIO21中断注册成功。读回CTRL1/2/5/7/8=68/07/01/01/d0，与生产配置不同，不据此判定哪项是根因。

约62秒面板DISPOFF/SLPIN，之后至94秒仍有静置raw=0、加速度约1g、采样时间戳更新；无已发现panic/反复重启。静置IRQ边沿为0，不证明中断物理触发或行走算法有效。用户休息，本次不做实际行走验收；USB日志不等同电池轻睡眠/功耗验收。正常OFF分支构建和驱动排除检查通过。

私有证据：firmware/backup/v1.1.5_official_flash_20261019.log、v1.1.5_official_reboot_20261019.log、v1.1.5_official_verification_20261019.json及official_recovery_20261019.json。手表保留此对照版待之后行走，日历史备份和1.1.4/稳定1.0.0回退保留。外置刷机清单已同步版本与说明。
