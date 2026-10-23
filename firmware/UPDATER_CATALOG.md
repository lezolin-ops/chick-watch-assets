# 刷机软件固件版本与更新说明

2026-10-18。当前安装并核对的是 `/Applications/宠物手表 2.app`，软件版本0.5.1。手表固件候选版本为1.1.1，两者版本号独立。

## 不重新编译即可维护的目录

`~/Library/Application Support/ChickDeviceCenter/Firmware/`

- `catalog.json`：版本名称、日期、摘要、注意事项及sections更新详情。
- 每个版本子目录：`manifest.json`和`.bin`。例如`v1.1.1/manifest.json`、`v1.1.1/firmware.bin`。
- 清单条目用相对路径关联manifest；manifest包含版本、chip、file、offset、sha256、recommended、placeholder等。
- 应用镜像offset为65536（0x10000）；合并镜像为0。软件界面和刷写引擎都只允许这两个地址，并在选择/刷写时验证文件及SHA256。
- 维护完文件，关闭并重新打开“更换版本”即可读取；不必重新编译、签名或重启软件。
- 外置同版本优先；未重复的应用内置版本仍可选。旧已验收睡眠版仍推荐，1.1.1明确标记待真机验收。

项目维护副本在`tools/chick-device-center/firmware/`，软件源码补丁在`tools/chick-device-center/`。此tools目录沿用项目本地工具忽略规则；维护副本存在本机，未宣称已提交版本控制。已保留原安装软件备份`/private/tmp/mini-app-original/宠物手表 2.app`（临时目录不是长期备份）。另一个同名软件`/Applications/宠物手表.app`本轮未替换，应使用“宠物手表 2.app”。

## 本轮验证

53项Python测试（含0x10000写入参数、不全片擦除、非法地址在串口前拒绝）通过，17项Swift清单/镜像校验测试通过。新引擎自检esptool5.1.0/pyserial3.5通过，应用签名完整性验证通过。桌面UI确认版本列表同时显示1.1.1与旧睡眠版，并能打开新版本完整更新说明。仅维护新固件时不需要编译；修改软件功能/协议才需要重新构建。
