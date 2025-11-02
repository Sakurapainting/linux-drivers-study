# buildroot构建根文件系统

```bash
tar -vxjf buildroot-2019.02.6.tar.bz2
```

buildroot 和 uboot、Linux kernel 一样也支持图形化配置

```bash
make menuconfig
```

## 配置 Target options

(“=”号后面是配置项要选择的内容！)

```
Target options
-> Target Architecture
= ARM (little endian)
-> Target Binary Format
= ELF
-> Target Architecture Variant
= cortex-A7
-> Target ABI
= EABIhf
-> Floating point strategy
= NEON/VFPv4
-> ARM instruction set
= ARM
```

现在大多数嵌入式设备的ARM都是小端序

Cortex-A7 内置了 NEON/VFPv4 硬件浮点单元，选择使用 EABIhf ABI（Application Binary Interface）

## 配置 Toolchain

用于配置交叉编译工具链，也就是交叉编译器，这里设置为我们自己所使用的交叉编译器即可。buildroot 其实是可以自动下载交叉编译器的，但是都是从国外服务器下载的

```
Toolchain
-> Toolchain type = External toolchain
-> Toolchain
= Custom toolchain  //用户自己的交叉编译器
-> Toolchain origin = Pre-installed toolchain   //预装的编译器
-> Toolchain path =/usr/local/arm/gcc-linaro-4.9.4-2017.01-x86_64_arm-linux-gnueabihf
-> Toolchain prefix
= $(ARCH)-linux-gnueabihf //前缀
-> External toolchain gcc version
= 4.9.x
-> External toolchain kernel headers series = 4.1.x
-> External toolchain C library
= glibc/eglibc
-> [*] Toolchain has SSP support? (NEW) //选中
-> [*] Toolchain has RPC support? (NEW) //选中
-> [*] Toolchain has C++ support?
//选中
-> [*] Enable MMU support (NEW)
//选中
```

## 配置 System configuration

此选项用于设置一些系统配置，比如开发板名字、欢迎语、用户名、密码等。需要配置的项目和其对应的内容如下：

```
System configuration
-> System hostname
= you_imx6ull //平台名字，自行设置
-> System banner
= Welcome to you's alpha i.mx6ull //欢迎语
-> Init system
= BusyBox   //使用 busybox
-> /dev management
= Dynamic using devtmpfs + mdev //使用 mdev
-> [*] Enable root login with password (NEW)    //使能登录密码
-> Root password
= 123456    //登录密码为 123456
```

- devtmpfs 是 Linux 内核提供的设备文件系统，在内核态快速创建基本设备节点，启动速度最快，但功能简单。

- mdev 是 BusyBox 提供的轻量级设备管理器，用于动态创建和删除 /dev 目录下的设备节点。非常适合嵌入式 Linux 系统（体积小、功能够用）；支持热插拔（U盘、SD卡等即插即用）。

## 配置 Filesystem images

此选项配置我们最终制作的根文件系统为什么格式的

```
-> Filesystem images
-> [*] ext2/3/4 root filesystem //如果是 EMMC 或 SD 卡的话就用 ext3/ext4
-> ext2/3/4 variant
= ext4  //选择 ext4 格式
-> [*] ubi image containing an ubifs root filesystem //如果使用 NAND 的话就用 ubifs
```

对于 I.MX6U 来说此选项不用配置，因为我们是通过 Mfgtool 工具将根文件系统烧写到开发板上的 EMMC/SD 卡中，烧写的时候需要自己对根文件系统进行打包。

## 禁止编译 Linux 内核和 uboot

buildroot 不仅仅能构建根文件系统，也可以编译 linux 内核和 uboot。当配置 buildroot，使能 linux 内核和 uboot 以后 buildroot 就会自动下载最新的 linux 内核和 uboot 源码并编译。但是我们一般都不会使用 buildroot 下载的 linux 内核和 uboot，因为 buildroot 下载的 linux 和 uboot官方源码，里面会缺少很多驱动文件，而且最新的 linux 内核和 uboot 会对编译器版本号有要求，可能导致编译失败。因此我们需要配置 buildroot，关闭 linux 内核和 uboot 的编译，只使用buildroot 来构建根文件系统

```
-> Kernel
-> [ ] Linux Kernel //不要选择编译 Linux Kernel 选项！
```

```
-> Bootloaders
-> [ ] U-Boot //不要选择编译 U-Boot 选项！
```

## 配置 Target packages

此选项用于配置要选择的第三方库或软件、比如 alsa-utils、ffmpeg、iperf 等工具，但是现在我们先不选择第三方库，防止编译不下去！先编译一下最基本的根文件系统，如果没有问题的话再重新配置选择第三方库和软件。

## 编译 buildroot

注意，一定要加 sudo，而且不能通过-jx 来指定多核编译！！！

```bash
sudo make
```

buildroot 编译过程会很耗时，可能需要几个小时

去 https://cmake.org/files/v3.8/cmake-3.8.2.tar.gz  下载源码到 buildroot 源码目录下的dl文件夹中，dl文件夹专门用于存放下载下来的源码

等待编译完成，编译完成以后就会在 buildroot-2019.02.6/output/images 下生成根文件系统

编译出来了多种格式的 rootfs，比如 ext2、ext4、ubi 等。其中rootfs.tar 就是打包好的根文件系统，我们就使用 rootfs.tar 进行测试。

## 挂载测试

这里不用nfs，不好用。

挂载到sd卡里

```bash
sudo mkfs.ext4 /dev/sdb1
sudo mount /dev/sdb1 /mnt
sudo tar xpf rootfs.tar -C /mnt
sudo umount /mnt
```

然后imxdownload u-boot.bin进sd卡

设置uboot：

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

**注意，usb转网口插进去之后要确保虚拟机的网络适配器桥接模式是打开的，不然就重启虚拟机试试，否则ping不通**

打开开发板网卡，这样虚拟机就能ping通开发板了，然后设置一下root密码，就可以ssh连接ftp了。

```bash
ifconfig eth0 192.168.10.50 netmask 255.255.255.0 up

passwd root
```

## buildroot 第三方软件和库的配置 - 使能 alsa-lib, alsa-utils, busybox

```
Target packages
-> Libraries
-> Audio/Sound
-> -*- alsa-lib ---> 此配置项下的文件全部选中
```

```
Target packages
-> Audio and video applications
-> alsa-utils
此目录下的软件全部选中
```

还要修改busybox对中文字符的支持，详情见网上教程，主要是禁止字符大于 0X7F 的时候设置为‘?’，还需要配置 busybox 来使能 unicode 码

```
Location:
-> Settings
-> Support Unicode  //选中
-> Check $LC_ALL, $LC_CTYPE and $LANG environment variables //选中
```

配置好以后就可以重新编译 buildroot 下的 busybox，进入到 buildroot 源码目录下，输入如下命令查看当前 buildroot 所有配置了的目标软件包，也就是 packages：

```bash
sudo make show-targets
```

列出了当前 buildroot 中所有使能了的 packages 包，其中就包括 busybox，如果我们想单独编译并安装 busybox 的话

```bash
sudo make busybox
```

然后 sudo make 编译，重复前面步骤。
