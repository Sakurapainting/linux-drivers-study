# USB驱动

## USB驱动 - USB简介

**USB1.0**：USB 规范于 1995 年第一次发布，由 Inter、IBM、Microsoft 等公司组成的 USB-IF(USB Implement Forum)组织提出。USB-IF 于 1996 年正式发布 USB1.0，理论速度为 1.5Mbps。1998 年 USBIF 在 USB1.0 的基础上提出了 USB1.1 规范。
**USB2.0**：USB2.0 依旧由 Inter、IBM、Microsoft 等公司提出并发布，USB2.0 分为两个版本:Full-Speed 和 High-Speed，也就是全速(FS)和高速(HS)。USB2.0 FS 的速度为 12Mbps，USB2.0HS 速度为 480Mbps。目前大多数单片机以及低端 Cortex-A 芯片配置的都是 USB2.0 接口，比如 STM32 和 ALPHA 开发板所使用的 I.MX6ULL。USB2.0 全面兼容 USB1.0 标准。
**USB3.0**：USB3.0 同样有 Inter 等公司发起的，USB3.0 最大理论传输速度为 5.0Gbps，USB3.0引入了全双工数据传输，USB2.0 的 480Mbps 为半双工。USB3.0 中两根线用于发送数据，另外两根用于接收数据。在 USB3.0 的基础上又提出了 USB3.1、USB3.2 等规范，USB3.1 理论传输速度提升到了 10Gbps，USB3.2 理论传输速度为 20Gbps。为了规范 USB3.0 标准的命名，USB-IF 公布了最新的 USB 命名规范，原来的 USB3.0 和 USB3.1 命名将不会采用，所有的 3.0 版本的 USB 都命名为 USB3.2，以前的 USB3.0、USB3.1 和 USB3.2 分别叫做 USB3.2 Gen1、USB3.2Gen2、USB3.2 Gen 2X2

## USB驱动 - USB电气特性

- 由于 正点原子 I.MX6U-ALPHA 开发板使用的 Mini USB 接口，因此我们就以 Mini USB 为例。Mini USB 线一般都是一头为 USB A 插头，一头为 Mini USB插头。一共有四个触点，也就是 4 根线.
- USB A 插头从左到右线序依次为 1,2,3,4，第 1 根线为 VBUS，电压为5V，第 2 根线为 D-，第 3 根线为 D+，第 4 根线为 GND。USB 采用差分信号来传输数据，因此有 D-和 D+两根差分信号线。USB A 插头的 1 和 4 这两个触点比较长，2 和 3 这两个触点比较短。1 和 4 分别为 VBUS 和 GND，也就是供电引脚，当插入 USB 的时候会先供电，然后再接通数据线。拔出的时候先断开数据线，然后再断开电源线。
- Mini USB 插头有 5 个触点，也就是 5 根线，线序从左往右依次是 1~5。第 1 根线为 VCC(5V)，第 2 根线为 D-，第 3 根线为 D+，第 4 根线为 ID，第 5 根线为 GND。可以看出 Mini USB 插头相比 USB A 插头多了一个 ID 线，这个 ID 线用于实现 OTG 功能，通过 ID 线来判断当前连接的是主设备(HOST)还是从设备(SLAVE)。
- USB 是一种支持热插拔的总线接口，使用差分线(D-和 D+)来传输数据，USB 支持两种供电模式：总线供电和自供电，总线供电就是由 USB 接口为外部设备供电，在 USB2.0 下，总线供电最大可以提供 500mA 的电流。

## USB驱动 - USB HUB 拓扑

- 虽然我们可以对原生的 USB 口数量进行扩展，但是我们不能对原生 USB 口的带宽进行扩展。
- USB 只能主机与设备之间进行数据通信，USB 主机与主机、设备与设备之间是不能通信的。因此两个正常通信的 USB 接口之间必定有一个主机，一个设备。为此使用了不同的插头和插座来区分主机与设备，比如主机提供 USB A 插座，从机提供 Mini USB、Micro USB 等插座。
- 在一个 USB 系统中，仅有一个 USB 主机，但是可以有多个 USB 设备，包括 USB 功能设备和 USBHUB，最多支持 127 个设备。一个 USB 主控制器支持 128 个地址，地址 0 是默认地址，只有在设备枚举的时候才会使用，地址 0 不会分配给任何一个设备。所以一个 USB 主控制器最多可以分配 127 个地址。整个 USB 的拓扑结构就是一个分层的金字塔形。（参考自USB2.0 协议）
- 从 Root Hub 开始，一共有 7 层，金字塔顶部是 Root Hub，这个是USB 控制器内部的。图中的 Hub 就是连接的 USB 集线器，Func 就是具体的 USB 设备。
- USB 主机和从机之间的通信通过管道(Pipe)来完成，管道是一个逻辑概念，任何一个 USB设备一旦上电就会存在一个管道，也就是默认管道，USB 主机通过管道来获取从机的描述符、配置等信息。在主机端管道其实就是一组缓冲区，用来存放主机数据，在设备端管道对应一个特定的端点。

## USB驱动 - OTG

OTG 全称是 USB On-The-Go，是USB 2.0规范的一个补充协议。

### OTG 的核心功能

OTG 允许一个USB设备**既可以作为主机(Host)，也可以作为从机(Device)**，实现设备间的直接通信，而不需要通过PC作为中介。

### 工作原理

OTG 通过 **ID 引脚**来判断当前设备的角色：

| ID引脚状态 | 设备角色 | 说明 |
|-----------|---------|------|
| **接地 (GND)** | 主机 (Host/A-Device) | Mini USB的ID线连接到GND，设备作为主机 |
| **悬空 (Float)** | 从机 (Device/B-Device) | ID线悬空，设备作为从机 |

ID=1：OTG 设备工作在从机模式。
ID=0：OTG 设备工作在主机模式。

### 实际应用场景

````
场景1：手机连接U盘
手机 (OTG Host) ←→ U盘 (Device)
手机的ID线接地，工作在Host模式，可以读取U盘数据

场景2：手机连接电脑
电脑 (Host) ←→ 手机 (OTG Device)  
手机的ID线悬空，工作在Device模式，电脑可以访问手机存储

场景3：手机之间传输数据
手机A (OTG Host) ←→ 手机B (OTG Device)
通过OTG线缆决定谁是主机
````

**ID线**就是实现OTG功能的关键：
- 普通USB A接口没有ID线，只能固定作为Host
- Mini/Micro USB有ID线，支持OTG动态切换角色

## USB驱动 - I.MX6ULL USB接口

I.MX6ULL 内部集成了两个独立的 USB 控制器，这两个 USB 控制器都支持 OTG 功能。
I.MX6ULL 内部 USB 控制器特性如下：
- ①、有两个 USB2.0 控制器内核分别为 Core0 和 Core1，这两个 Core 分别连接到 OTG1 和 OTG2。
- ②、两个 USB2.0 控制器都支持 HS、FS 和 LS 模式，不管是主机还是从机模式都支持HS/FS/LS，硬件支持 OTG 信号、会话请求协议和主机协商协议，支持 8 个双向端点。
- ③、支持低功耗模式，本地或远端可以唤醒。
- ④、每个控制器都有一个 DMA。
每个 USB 控制器都有两个模式：正常模式(normal mode)和低功耗模式(low power mode)。
- 每个 USB OTG 控制器都可以运行在高速模式(HS 480Mbps)、全速模式(LS 12Mbps)和低速模式(1.5Mbps)。正常模式下每个 OTG 控制器都可以工作在主机(HOST)或从机(DEVICE)模式下，每个 USB 控制器都有其对应的接口。低功耗模式顾名思义就是为了节省功耗，USB2.0 协议中要
求，设备在上行端口检测到空闲状态以后就可以进入挂起状态。在从机(DEVICE)模式下，端口停止活动 3ms 以后 OTG 控制器内核进入挂起状态。在主机(HOST)模式下，OTG 控制器内核不会自动进入挂起状态，但是可以通过软件设置。不管是本地还是远端的 USB 主从机都可以通过产生唤醒序列来重新开始 USB 通信。
- 两个 USB 控制器都兼容 EHCI，这里我们简单提一下 OHCI、UHCI、EHCI 和 xHCI，这三个是用来描述 USB 控制器规格的，区别如下：
- **OHCI**：全称为 Open Host Controller Interface，这是一种 USB 控制器标准，厂商在设计 USB控制器的时候需要遵循此标准，用于 USB1.1 标准。OHCI 不仅仅用于 USB，也支持一些其他的
接口，比如苹果的 Firewire 等，OHCI 由于硬件比较难，所以软件要求就降低了，软件相对来说
比较简单。OHCI 主要用于非 X86 的 USB，比如扩展卡、嵌入式 USB 控制器。
- **UHCI**：全称是 Universal Host Controller Interface，UHCI 是 Inter 主导的一个用于 USB1.0/1.1
的标准，与 OHCI 不兼容。与 OHCI 相比 UHCI 硬件要求低，但是软件要求相应就高了，因此
硬件成本上就比较低。
- **EHCI**：全称是 Enhanced Host Controller Interface，是 Inter 主导的一个用于 USB2.0 的 USB
控制器标准。I.MX6ULL 的两个 USB 控制器都是 2.0 的，因此兼容 EHCI 标准。EHCI 仅提供
USB2.0 的高速功能，至于全速和低速功能就由 OHCI 或 UHCI 来提供。
- **xHCI**：全称是 eXtensible Host Controller Interface，是目前最流行的 USB3.0 控制器标准，
在速度、能效和虚拟化等方面比前三个都有较大的提高。xHCI 支持所有速度种类的 USB 设备，
xHCI 出现的目的就是为了替换前面三个。
- 关于 I.MX6ULL 的 USB 控制器就简单的讲解到这里，至于更详细的内容请参考 I.MX6ULL
参考手册中的“Chapter 56 Universal Serial Bus Controller(USB)”章节。

### I.MX6ULL USB HUB

I.MX6ULL-ALPHA 使用 SL2.1A 这个 HUB 芯片将 I.MX6ULL 的 USBOTG2 扩展成了 4 路 HOST 接口，其中一路供 4G 模块使用，因此就剩下了三个通用的 USB A 插座

符合 USB2.0 标准的 USBHUB 芯片，支持一拖四扩展，可以将一路 USB 扩展为 4 路 USB HOST 接口。这里我们将I.MX6ULL 的 USB OTG2 扩展出了 4 路 USB HOST 接口，分别为 HUB_DP1/DM1、HUB_DP2/DM2、HUB_DP3/DM3 和 HUB_DP4/DM4。其中 HUB_DP4/DM4 用于 4G 模块，因此对外提供的只有三个 USB HOST 接口.

### I.MX6ULL USB OTG

I.MX6U-ALPHA 开发板上有一路 USB OTG 接口，使用 I.MX6ULL 的 USB OTG1 接口。此路 USB OTG 既可以作为主机(HOST)，也可以作为从机(DEVICE)，从而实现完整的 OTG 功能。

USB OTG 通过 ID 线来完成主从机切换，这里就涉及到硬件对 USB ID 线的处理

## USB驱动 - USB描述符

Device Descriptor
设备描述符

Configuration Descriptor
配置描述符

String Descriptor
字符串描述符

Interface Descriptor
接口字符串

Endpoint Descriptor
端点描述符

## USB驱动 - Linux内核自带HOST实验

### USB鼠标键盘测试

**NXP 官方的 Linux 内核默认已经使能了 USB 键盘鼠标驱动**

如何手动使能这些驱动？

menuconfig中，使能通用HID驱动：

- -> Device Drivers
    -> HID support
    -> HID bus support (HID [=y])
    -> <*> Generic HID driver

使能 USB 键盘和鼠标驱动：

- -> Device Drivers
    -> HID support
    -> USB HID support
    -> <*> USB HID transport layer 

此选项对应配置项就是 CONFIG_USB_HID，也就是 USB 接口的 HID 设备。如果要使用USB 接口的 keyboards(键盘)、mice(鼠标)、joysticks(摇杆)、graphic tablets(绘图板)等其他的 HID设备，那么就需要选中“USB HID Transport layer”。但是要注意一点，此驱动和 HIDBP(BootProtocol)键盘、鼠标的驱动不能一起使用。

**如果HID无法使用，则使用“USB HIDBP Keyboard (simple Boot) support”和“USB HIDBP Mouse(simple Boot) support”这两个配置项，也就是USB_KBD和 USB_MOUSE。HID是一个较低层的接口，上层还需要input的支持，或者直接打开USB KBD这样的，直接跳过支持。这对于不同键盘来说需要使用的驱动不一样**

```bash
usb 1-1.3: new full-speed USB device number 3 using ci_hdrc
input: Darmoshark M3 Mouse as /devices/platform/soc/2100000.aips-bus/2184200.usb/ci_hdrc.1/usb1/1-1/1-1.3/1-1.3:1.0/0003:248A:FF12.0001/input/input4
hid-generic 0003:248A:FF12.0001: input: USB HID v1.11 Mouse [Darmoshark M3 Mouse] on usb-ci_hdrc.1-1.3/input0
input: Darmoshark M3 Mouse as /devices/platform/soc/2100000.aips-bus/2184200.usb/ci_hdrc.1/usb1/1-1/1-1.3/1-1.3:1.1/0003:248A:FF12.0002/input/input5
hid-generic 0003:248A:FF12.0002: input: USB HID v1.11 Keyboard [Darmoshark M3 Mouse] on usb-ci_hdrc.1-1.3/input1
hid-generic 0003:248A:FF12.0003: device has no listeners, quitting
```

如果成功驱动的话就会在/dev/input 目录下生成一个名为 eventX(X=0,1,2,3…)的文件，鼠标和键盘都是作为输入子系统设备的。测试时这里对应的是/dev/input/event3 这个设备。查看鼠标原始输入值：

```bash
hexdump /dev/input/event3
```

- 打开开发板根文件系统中的/etc/inittab 文件，然后在里面加入下面这一行：

```conf
tty1::askfirst:-/bin/sh
```

### USB鼠标键盘测试

NXP 官方的 Linux 内核默认已经使能了 U 盘

U 盘使用 SCSI 协议，因此要先使能 Linux 内核中的 SCSI 协议

- -> Device Drivers
    -> SCSI device support
    -> <*> SCSI disk support

还需要使能 USB Mass Storage，也就是 USB 接口的大容量存储设备

- -> Device Drivers
    -> USB support (USB_SUPPORT [=y])
    -> Support for Host-side USB (USB [=y])
    -> <*> USB Mass Storage support

### U盘测试

要使用FAT32 格式的，NTFS 和 exFAT 由于版权问题所以在 Linux下支持的不完善，操作的话可能会有问题，比如只能读，不能写或者无法识别等。

我们一般使用 U 盘的时候都是只有一个分区。要想访问 U盘我们需要先对 U 盘进行挂载，理论上挂载到任意一个目录下都可以，这里我创建一个/mnt/usb_disk 目录，然后将 U 盘挂载到/mnt/usb_disk 目录下

```bash
mkdir /mnt/usb_disk -p
mount /dev/sda1 /mnt/usb_disk/ -t vfat -o iocharset=utf8
```

-t 指定挂载所使用的文件系统类型，这里设置为 vfat，也就是 FAT 文件系统，“-o iocharset” 设置硬盘编码格式为 utf8

至此 U 盘就能正常读写操作了，直接对/mnt/usb_disk 目录进行操作就行了。如果要拔出 U盘要执行一个 sync 命令进行同步，然后在使用 unmount 进行 U 盘卸载:

```bash
sync                        //同步
cd /                        //如果处于/mnt/usb_disk 目录的话先退出来，否则卸载的时候提示设备忙，导致卸载失败，切记！
umount /mnt/usb_disk        //卸载
```

## USB驱动 - Linux 内核自带 USB OTG 实验

### 修改设备树

查阅原理图可以知道，USB OTG1 的 ID 引脚连接到了 I.MX6ULL 的 GPIO1_IO00 这个引脚上，USB OTG 默认工作在主机(HOST)模式下，因此 ID 线应该是低电平。这里需要修改设备树中 GPIO1_IO00 这个引脚的电气属性，将其设置为默认下拉。打开设备树 imx6ull-alientek-emmc.dts，在 iomuxc 节点的 pinctrl_hog_1 子节点下添加 GPIO1_IO00 引脚信息：

```c
&iomuxc {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_hog_1>;
    imx6ul-evk {
        pinctrl_hog_1: hoggrp-1 {
            fsl,pins = <
                MX6UL_PAD_UART1_RTS_B__GPIO1_IO19	0x17059 /* SD1 CD */
                MX6UL_PAD_GPIO1_IO05__USDHC1_VSELECT	0x17059 /* SD1 VSELECT */
                // MX6UL_PAD_GPIO1_IO09__GPIO1_IO09        0x17059 /* SD1 RESET */
                MX6UL_PAD_GPIO1_IO00__ANATOP_OTG1_ID    0x13058 /*OTG1 ID */
            >;
        };
    };
};
```

### USB声卡实验

- ALPHA 开发板板载了音频解码芯片，因此可以将 ALPHA 开发板作为一个外置USB 声卡，配置 Linux 内核：

- -> Device Drivers
    -> USB support (USB_SUPPORT [=y])
    -> USB Gadget Support (USB_GADGET [=y]
    -> [M]USB Gadget Drivers
    ->[M] Audio Gadget
    ->UAC 1.0 (Legacy)

- 编译为驱动模块，配置完成以后重新编译内核，得到新的 zImage 和三个.ko驱动模块文件：

```
drivers/usb/gadget/libcomposite.ko
drivers/usb/gadget/function/usb_f_uac1.ko
drivers/usb/gadget/legacy/g_audio.ko
```

将上述三个.ko 模块拷贝到开发板根文件系统中

```bash
cd drivers/usb/gadget/
sudo cp libcomposite.ko rootfs/lib/modules/4.1.15/
sudo cp function/usb_f_uac1.ko rootfs/lib/modules/4.1.15/
sudo cp legacy/g_audio.ko rootfs/lib/modules/4.1.15/
```

拷贝完成以后使用新编译出来的 zImage 启动开发板，配
置 ALPHA 的声卡，保证声卡播放正常，将开发板与电脑连接起来，最后依次加载 libcomposite.ko、usb_f_uac1.ko 和 g_audio.ko 这三个驱动模块。

其中，libcomposite.ko 依赖于configfs，要检查内核编译时是否有选择编译，在这里我的menuconfig把他作为模块编译，所以还要先加载一下configfs.ko，他在 `fs/configfs/configfs.ko` 

```bash
/lib/modules/4.1.15 # depmod
/lib/modules/4.1.15 # modprobe configfs.ko 
/lib/modules/4.1.15 # modprobe libcomposite.ko 
/lib/modules/4.1.15 # modprobe usb_f_uac1.ko 
/lib/modules/4.1.15 # modprobe g_audio.ko 
g_audio gadget: Hardware params: access 3, format 2, channels 2, rate 48000
g_audio gadget: Linux USB Audio Gadget, version: Feb 2, 2012
g_audio gadget: g_audio ready
g_audio gadget: high-speed config #1: Linux USB Audio Gadget
```

加载完成以后稍等一会虚拟出一个 USB 声卡，打开电脑的设备管理器，选择“声音、视频和游戏控制器”，会发现有一个名为“AC Interface”设备，AC Interface”就是开发板模拟出来的 USB 声卡，设置 windows，选择音频输出使用“AC Interface”，一切设置好以后就可以从开发板上听到电脑输出的声音，此时开发板就完全是一个 USB 声
卡设备了。
