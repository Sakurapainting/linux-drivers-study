# Linux 驱动开发学习项目

这是一个基于IMX6ULL ARM开发板（正点原子alpha）的Linux内核驱动开发学习项目。

### 硬件平台
- **开发板**: IMX6ULL ARM Cortex-A7
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

## 使用方法

### 1. 加载驱动模块
```bash
depmod
modprobe driver_name.ko
```

### 2. 查看设备信息
```bash
cat /proc/devices
ls /dev/
```

### 3. 测试驱动
每个驱动都提供了对应的应用程序（如chrdevbaseAPP、ledAPP等）：
```bash
./driverAPP
```

### 4. 卸载驱动模块
```bash
rmmod driver_name
```

## 贡献指南

欢迎提交Issue和Pull Request来改进这个项目：
- Bug修复
- 代码优化
- 文档改进
- 新功能添加

## 许可证

本项目采用MIT许可证，详见[LICENSE](LICENSE)文件。

## 联系方式

如有问题请提交Issue或发送邮件讨论。

---

**注意**: 此项目仅用于学习目的，在生产环境中使用前请进行充分测试。
