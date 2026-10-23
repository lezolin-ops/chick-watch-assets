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

## 用户实际试走反馈与中断自检

用户随后报告几十步仍显示0。之后按需快照raw=0、irq=0、加速度/时间戳正常，并非行走全过程日志。1.1.5不能作为计步已可用交付。

1.1.6-irq仅添加显式STEPS_IRQ_TEST：唯一worker确认会话raw=0，暂清CTRL8.bit7，通过原库0x0F命令完成/ACK驱动INT1，再恢复并读回原CTRL8，记录GPIO21边沿差。依据QST Rev A第5.10.1节。raw非0跳过，恢复失败停止worker并释放AWAKE，需重启。原库文件未改；测试仅清本来为0的会话计数，不写日历史，不调整计步参数。该测试证明命令中断链路时不能替代实际计步事件验证；没有边沿也需结合状态/引脚检查，不能直接判硬件损坏。

### 中断输入配置遗漏定位

1.1.6命令中断自检返回command=0、edges=0、restored=1。进一步检查原理图第1页QMI区，INT1连GPIO21，INT2为TP15；不是把INT2误作INT1。核对原库`setPins(int _irq)`发现函数体`_irq = _irq`只赋值参数本身，成员仍为-1，因此HAL未初始化该引脚。适配器之前只调用gpio_set_intr_type/handler_add，未显式启用输入；Arduino attachInterrupt会处理输入，而这条原生IDF路线没有处理。这是适配器的明确遗漏，不能据此声称原始计数全0也都由它导致。

1.1.7在适配器显式gpio_config GPIO21为INPUT、ANYEDGE、无上下拉，再安装ISR；vendor保持原字节不改。继续同一命令自检对照；先前IRQ边沿0不再作为芯片/连线不工作证据。日历史和原计步参数保持。

### 修复后的同条件结果

1.1.7命令自检`command=0 write_ok=1 edges=2 pin=0 restored=1 ctrl8=d0`；1.1.6在相同命令下edges=0。证明显式输入配置修复后，芯片命令INT1至GPIO21/ISR可收到两个转换边沿，配置已恢复。两个边沿是测试命令产生，不能当作两步或计步事件已工作。用户休息，1.1.7实际行走未复测，静置raw0不构成计步失败或成功判据。
