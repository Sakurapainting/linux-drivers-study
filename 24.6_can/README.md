# CAN驱动

## CAN驱动 - CAN协议分析

### CAN电气属性
- 两根差分线：CAN_H和CAN_L
- 两个电平：显性电平和隐形电平：
  - 显性电平：表示逻辑0，此时CAN_H比CAN_L高，分别是3.5V和1.5V，电位差2V。
  - 隐形电平：表示逻辑1，此时CAN_H和CAN_L都是2.5V，电位差0V

## CAN驱动 - FlexCAN

- 6ull 使用 can 收发芯片
- CAN1_TX接UART3_CTS
- CAN1_RX接UART3_RTS
- 修改设备树：NXP原厂的设备树已经配置好了FlexCAN的节点信息 (FlexCAN1 和 FlexCAN2) ，但是还是要来看一下如何配置 I.MX6ULL 的 CAN1 节点。首先看一下 I.MX6ULL 的 FlexCAN设备树绑定文档，打开 `Documentation/devicetree/bindings/net/can/fsl-flexcan.txt`
- CAN1 引脚配置信息，打开 imx6ull-alientek-emmc.dts

```c
pinctrl_flexcan1: flexcan1grp{
    fsl,pins = <
        MX6UL_PAD_UART3_RTS_B__FLEXCAN1_RX 0x1b020
        MX6UL_PAD_UART3_CTS_B__FLEXCAN1_TX 0x1b020
    >;
};
```

- 打开 imx6ull.dtsi 文件，找到名为“flexcan1”的节点，FlexCAN1完整节点信息：

```c
flexcan1: can@02090000 {
    compatible = "fsl,imx6ul-flexcan", "fsl,imx6q-flexcan";
    reg = <0x02090000 0x4000>;
    interrupts = <GIC_SPI 110 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&clks IMX6UL_CLK_CAN1_IPG>,
        <&clks IMX6UL_CLK_CAN1_SERIAL>;
    clock-names = "ipg", "per";
    stop-mode = <&gpr 0x10 1 0x10 17>;
    status = "disabled";
};
```

- 根据第 2 行的 compatible 属性就可以找到 I.MX6ULL 的 FlexCAN 驱动源文件，驱动文名为 drivers/net/can/flexcan.c。第 9 行的 status 属性为 disabled，所以 FlexCAN1 默认关闭的。在 imx6ull-alientek-emmc.dts 中添加使能 FlexCAN1 的相关操作：

```c
&flexcan1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_flexcan1>;
    xceiver-supply = <&reg_can_3v3>;
    status = "okay";
};
```

- 关闭 FlexCAN2 相关节点：I.MX6ULL 带有两个 CAN 控制器：FlexCAN1 和 FlexCAN2，NXP 官方的 EVK 开发板这两个 CAN 接口都用到了，因此 NXP 官方的设备树将这两个 CAN 接口都使能了。但是，正点原子的 I.MX6U-ALPHA 开发板将 FlexCAN2 的 IO 分配给了 ECSPI3，所以正点原子的 I.MX6U-ALPHA 开发板就不能使用 CAN2，否则的话 ECSPI3 外设就无法使用。

```c
// &flexcan2 {
// 	pinctrl-names = "default";
// 	pinctrl-0 = <&pinctrl_flexcan2>;
// 	xceiver-supply = <&reg_can_3v3>;
// 	status = "okay";
// };
```

- 重新编译设备树
- 首先打开 CAN 总线子系统，在 Linux 下 CAN 总线是作为网络子系统的：
  - -> Networking support
  - -> <*> CAN bus subsystem support
- 接着使能 Freescale 系 CPU 的 FlexCAN 外设驱动
  - -> Networking support
  - -> CAN bus subsystem support
  - -> CAN Device Drivers
  - -> <*> Support for Freescale FLEXCAN based chips
- 重新编译内核

```bash
ifconfig -a
```

- Linux 系统中把 CAN 总线接口设备作为网络设备进行统一管理，因此如果FlexCAN 驱动工作正常的话就会看到 CAN 对应的网卡接口
- 测试显示，有一个名为“can0”的网卡，这个就是 I.MX6U-ALPHA 开发板上的 CAN1 接口对应的 can 网卡设备。如果使能了 I.MX6ULL 上的 FlexCAN2 的话也会出现一个名为“can1”的 can 网卡设备。

## CAN驱动 - iproute2 和 can-utils

**在移植 ip 命令的时候必须先对根文件系统做个备份！防止操作失误导致系统启动失败**

- 移植好后，查看版本信息验证移植是否成功

```bash
/ # ip -V
ip utility, iproute2-ss160111
```

## CAN驱动 - CAN通信测试

- 如果要在一个板子上进行 CAN 回环测试，按照如下命令设置 CAN：

```bash
ifconfig can0 down                                      //如果 can0 已经打开了，先关闭
ip link set can0 type can bitrate 500000 loopback on    //开启回环测试
ifconfig can0 up                                        //重新打开 can0
candump can0 &                                          //candump 后台接收数据
cansend can0 5A1#11.22.33.44.55.66.77.88                //cansend 发送数据
```

- 如果回环测试成功的话那么开发板就会收到发送给自己的数据

```bash
/ # cansend can0 5A1#11.22.33.44.55.66.77.88
  can0  5A1   [8]  11 22 33 44 55 66 77 88
```

- 如果要编写 CAN 总线应用的话就直接使用 Linux 提供的
SocketCAN 接口，使用方法类似网络通信