# Linux 驱动开发学习项目

这是一个基于IMX6ULL ARM开发板的Linux驱动开发学习项目。

### 我的硬件平台（测试可用）
- **开发板**: IMX6ULL ARM Cortex-A7 正点原子阿尔法
- **内核版本**: Linux 4.1.15
- **交叉编译器**: arm-linux-gnueabihf-gcc

## 编译环境配置

### 编译方法
每个驱动目录都包含Makefile，进入对应目录后执行：
```bash
make
```

清理编译文件：
```bash
make clean
```

## 文件结构

```
Linux_Drivers/
├── imx6ull-alientek-emmc.dts    # 完整设备树文件（最新版本）
├── imx6ull-alientek-emmc.dtb    # 编译后的设备树
├── zImage                       # Linux内核镜像
├── sync_kernel_files.sh         # 内核文件同步脚本
├── README.md                    # 项目说明文档
├── LICENSE                      # 许可证文件
├── xx_xxxdevice_driver/                # 某章节内容
│   ├── xxx.c             # 驱动源码
│   ├── xx_xxxdevice_driver.dts         # 设备树片段（示例）
│   ├── xxxAPP.c              # 测试应用
│   ├── README.md             # 章节笔记、说明
│   └── Makefile              # 编译脚本
```

### 说明
- **根目录文件**: 包含完整的设备树文件和内核镜像，适用于最新的驱动章节
- **各章节目录**: 每个目录对应一个驱动学习主题，包含驱动源码、测试应用和编译脚本
- **设备树片段**: 各章节下的`.dts`文件仅为示例片段，需要手动添加到完整设备树中
- **编译产物**: `.ko`、`.o`、`.mod.c`等文件为编译生成，可通过`make clean`清理

## 使用方法

###

### 1. 设备树使用

仓库根目录下的 `.dts` 和 `.dtb` 是适用于最新章节(chapter)的完整设备树文件

设备树中的其他地方如果有冲突寄存器，把其他地方的注释掉

各章节下的 `.dts` 是该章节的dts文件片段，不能直接使用，仅为示例。可以参考自行添加到用户自己的完整设备树中

参考u-boot示例

```shell
env default -a;saveenv 
setenv ipaddr 192.168.10.50 
setenv ethaddr 00:04:9f:04:d2:35 
setenv gatewayip 192.168.10.1 
setenv netmask 255.255.255.0 
setenv serverip 192.168.10.100 
saveenv

setenv bootcmd 'tftp 80800000 zImage;tftp 83000000 imx6ull-alientek-emmc.dtb;bootz 80800000 - 83000000' 
saveenv 

setenv bootargs 'console=ttymxc0,115200 root=/dev/mmcblk0p1 rootwait rw'
saveenv

boot
```

### 2. 加载驱动模块
```bash
depmod
modprobe driver_name.ko
```

### 3. 查看设备信息
```bash
cat /proc/devices
ls /dev/
```

### 4. 测试驱动
每个驱动都提供了对应的应用程序（如chrdevbaseAPP、ledAPP等），获取权限后可使用：
```bash
sudo chmod +x driverAPP
```

如果没有，可以自行编译：

```bash
arm-linux-gnueabihf-gcc driverAPP.c -o driverAPP
```

### 5. 卸载驱动模块
```bash
rmmod driver_name
```

## 贡献指南

欢迎提交Issue和Pull Request来改进这个项目：
- Bug修复
- 代码优化
- 文档改进
- 新功能添加

### 代码格式

本项目使用clang-format来保持 C/C++ 语言代码风格一致。


## 许可证

本项目采用MIT许可证，详见[LICENSE](LICENSE)文件。

## 联系方式

如有问题请提交Issue或发送邮件讨论。

---

**注意**: 此项目仅用于学习目的，在生产环境中使用前请进行充分测试。
