# EMMC4线修改为8线模式

正点原子 EMMC 版本核心板上的 EMMC 采用的 8 位数据线。Linux 内核驱动里面 EMMC 默认是 4 线模式的，4 线模式肯定没有 8 线模式的速度快，所以本节我们将 EMMC 的驱动修改为 8 线模式。

EMMC 工作电压是 3.3V 的，因此我们要在 usdhc2 设备树节点中添加“no-1-8-v”选项，也就是关闭 1.8V 这个功能选项。防止内核在运行的时候用 1.8V 去驱动 EMMC，导致 EMMC 驱动出现问题

原来的设备树：

```c
&usdhc2 {
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_usdhc2>;
	non-removable;
	status = "okay";
};
```

修改后的设备树：

```c
&usdhc2 {
	pinctrl-names = "default", "state_100mhz", "state_200mhz";
	pinctrl-0 = <&pinctrl_usdhc2_8bit>;
	pinctrl-1 = <&pinctrl_usdhc2_8bit_100mhz>;
	pinctrl-2 = <&pinctrl_usdhc2_8bit_200mhz>;
	bus-width = <8>;
	non-removable;
	no-1-8-v;
	status = "okay";
};
```