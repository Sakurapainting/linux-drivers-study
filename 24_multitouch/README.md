# IMX6ULL LinuxDriver

## 多点电容触摸屏 - 驱动框架

- 电容触摸屏上报多点触摸信息，通过触摸芯片（比如FT5426），这是一个i2c设备。多点电容触摸屏本质是i2c驱动
- 触摸IC一般都是有INT（中断引脚），当监测到触摸信息以后就会触发中断，那么就要在中断处理函数里面读取触摸点信息
- 得到触摸点信息。linux下有触摸屏上报的流程规章制度，涉及到input子系统下触摸信息的上报。文档参考 `${sdk}/Documentation/input/multi-touch-protocol.txt` ，老版本可能没有MT（multi-touch）协议。协议有两种类型：
- type A: 适用于触摸点不能被区分，此类型设备上报原始数据（这类在实际使用中用的少）。没有轨迹追踪的信息，有的只是点坐标等信息。判断当前的多个点各属于哪条线，依靠软件计算，用硬件计算的是type B。
- type B: 适用于有硬件追踪并能区分触摸点的触摸设备，此类型设备通过slot更新某一个触摸点的信息，FT5426就属于此类型，一般的多点电容触摸屏IC都有此能力

### 两种类型的上报方式

**type A**:

```c
ABS_MT_POSITION_X x[0]      // 第一个点x轴坐标
ABS_MT_POSITION_Y y[0]      // 第一个点y轴坐标
SYN_MT_REPORT               // 点与点之间用这个隔离，使用input_mt_sync
ABS_MT_POSITION_X x[1]      // 第二个点x轴坐标
ABS_MT_POSITION_Y y[1]      // 第二个点y轴坐标
SYN_MT_REPORT               
SYN_REPORT                  // 所有点发送完成后，input_sync
```

**type B**:

type B 使用slot来区分触摸点，slot使用 ABS_MT_TRACKING_ID 来增加、删除、替换一个触摸点信息

```c
ABS_MT_SLOT 0               // 表示要上报第一个触摸点信息
ABS_MT_TRACKING_ID 45       // 通过调用input_mt_report_slot_state
ABS_MT_POSITION_X x[0]      
ABS_MT_POSITION_Y y[0]
ABS_MT_SLOT 1
ABS_MT_TRACKING_ID 46       // 第二个触摸点，使用函数input_mt_slot
ABS_MT_POSITION_X x[1]      
ABS_MT_POSITION_Y y[1]
SYN_REPORT                  // 所有点发送完成后，input_sync
```

## 多点电容触摸屏 - 驱动

- 驱动主框架是i2c设备，会用到中断，在中断处理函数中上报触摸点信息，要用到input子系统框架
- 设备树IO修改，i2c节点添加：
INT -> GPIO1_IO09
RST -> SNVS_TAMPER9
I2C_SDA -> UART5_RXD
I2C_SCL -> UART5_RXD

- MX6ULL_PAD_SNVS_TAMPER 的 都要放到设备树的 `&iomuxc_snvs` 下
- 编写设备树找地址宏定义时，先去6ull找，没有再去6ul找

```c
&iomuxc {
        /*you tsc INT*/
		pinctrl_tsc: tscgrp {
			fsl,pins = <
				MX6UL_PAD_GPIO1_IO09__GPIO1_IO09	0xf080
			>;
		};    
};
```

- 除了要检查 `GPIO1_IO09` 在其他地方是否有占用，还要检查是否有 `&gpio1 9` 这样的使用方式，下面其他引脚配置同理

```c
&iomuxc_snvs {
        /*you MT RST*/
		pinctrl_tsc_reset: tsc_reset {
			fsl,pins = <
				MX6ULL_PAD_SNVS_TAMPER9__GPIO5_IO09		0x10b0
			>;
		};    
};

```

```c
&iomuxc {
        pinctrl_i2c2: i2c2grp {
			fsl,pins = <
				MX6UL_PAD_UART5_TX_DATA__I2C2_SCL 0x4001b8b0
				MX6UL_PAD_UART5_RX_DATA__I2C2_SDA 0x4001b8b0
			>;
		};    
};
```

- 在i2c节点下：

```c
&i2c2 {
    /*you 4.3寸 FT5426*/
	ft5426: ft5426@38 {
		compatible = "edt,edt-ft5426";
		pinctrl-names = "default";
		pinctrl-0 = <&pinctrl_tsc
					&pinctrl_tsc_reset>;
		interrupt-parent = <&gpio1>;
		interrupts = <9 IRQ_TYPE_NONE>;
		reset-gpios = <&gpio5 9 GPIO_ACTIVE_LOW>;
		interrupt-gpios = <&gpio1 9 GPIO_ACTIVE_LOW>;
		status = "okay";
	};    
};
```

- interrupts 常见定义：

```c
#define IRQ_TYPE_NONE		0x00000000
#define IRQ_TYPE_EDGE_RISING	0x00000001
#define IRQ_TYPE_EDGE_FALLING	0x00000002
#define IRQ_TYPE_EDGE_BOTH	(IRQ_TYPE_EDGE_FALLING | IRQ_TYPE_EDGE_RISING)
#define IRQ_TYPE_LEVEL_HIGH	0x00000004
#define IRQ_TYPE_LEVEL_LOW	0x00000008
```

- 查看设备树中i2c2在哪个节点下，发现在 `imx6ull.dtsi` 下的 `aips2: aips-bus@02100000` 下。
- 在开发板中，查看i2c确实在这个地址下：

```bash
/ # cd /proc/device-tree
/sys/firmware/devicetree/base # ls
#address-cells                 gpioled
#size-cells                    interrupt-controller@00a01000
aliases                        key
alphaled                       memory
backlight                      model
beep                           name
chosen                         pxp_v4l2
clocks                         regulators
compatible                     reserved-memory
cpus                           soc
dtsleds                        sound
gpio_keys                      spi4
/sys/firmware/devicetree/base # cd soc
/sys/firmware/devicetree/base/soc # ls
#address-cells      compatible          ranges
#size-cells         dma-apbh@01804000   sram@00900000
aips-bus@02000000   gpmi-nand@01806000  sram@00904000
aips-bus@02100000   interrupt-parent    sram@00905000
aips-bus@02200000   name
busfreq             pmu
/sys/firmware/devicetree/base/soc # cd aips-bus@02100000/
/sys/firmware/devicetree/base/soc/aips-bus@02100000 # ls
#address-cells       lcdif@021c8000       serial@021f0000
#size-cells          mmdc@021b0000        serial@021f4000
adc@02198000         name                 serial@021fc000
compatible           ocotp-ctrl@021bc000  usb@02184000
csi@021c4000         pxp@021cc000         usb@02184200
csu@021c0000         qspi@021e0000        usbmisc@02184800
ethernet@02188000    ranges               usdhc@02190000
i2c@021a0000         reg                  usdhc@02194000
i2c@021a4000         romcp@021ac000       weim@021b8000
i2c@021a8000         serial@021e8000
i2c@021f8000         serial@021ec000
```

- i2c地址是021a4000，可见其下有 `ft5426@38` ：

```bash
/sys/firmware/devicetree/base/soc/aips-bus@02100000 # cd i2c@021a4000/
/sys/firmware/devicetree/base/soc/aips-bus@02100000/i2c@021a4000 # ls
#address-cells   compatible       ov5640@3c        status
#size-cells      ft5426@38        pinctrl-0        wm8960@1a
clock_frequency  interrupts       pinctrl-names
clocks           name             reg
```

- 准备i2c设备框架，但是不需要fops，因为这也是输入设备。
- 复位引脚和中断引脚，包括中断
- input子系统框架
- 初始化FT5426
- 在中断服务函数里面读取触摸坐标值，然后上报给系统

---

- devm函数申请后会自动释放

```c
// 申请复位IO 并且默认输出低电平 devm函数申请后会自动释放
        ret = devm_gpio_request_one(&client->dev, dev->reset_pin, GPIOF_OUT_INIT_LOW, "edt-ft5426 reset");
```

---

- 写到这里，发现与手中开发板触摸屏芯片不同，我手上的开发板是gt9147的，直接使用正点原子左忠凯的源代码，其中有一些地方不同，但大体相似。

## 多点电容触摸屏 - 添加到内核中

- 在 `${sdk}/drivers/input/touchscreen` 中，添加 `gt9147.c` 文件，并修改Makefile文件，添加：

```makefile
obj-y += gt9147.o
```

- 编译内核