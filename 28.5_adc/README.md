# ADC

## ADC - 驱动编写

### 修改设备树

ADC 驱动 NXP 已经编写好了，我们只需要修改设备树即可。首先在 imx6ull-alientek-emmc.dts 文件中添加 ADC 使用的 GPIO1_IO01 引脚配置信息：

```c
pinctrl_adc1: adc1grp {
    fsl,pins = <
        MX6UL_PAD_GPIO1_IO01__GPIO1_IO01 0xb0
    >;
};
```

```c
reg_vref_adc: regulator@2 {
    compatible = "regulator-fixed";
    regulator-name = "VREF_3V3";
    regulator-min-microvolt = <3300000>;
    regulator-max-microvolt = <3300000>;
};
```

```c
&adc1 {
    pinctrl-names = "default";
    pinctrl-0 = <&pinctrl_adc1>;
    num-channels = <2>;
    vref-supply = <&reg_vref_adc>;
    status = "okay";
};
```

### 使能ADC驱动

```
-> Device Drivers
-> Industrial I/O support
-> Analog to digital converters
-> <*> Freescale vf610 ADC driver 
```

### 测试

```bash
/sys/devices/platform/soc/2100000.aips-bus/2198000.adc/iio:device0 # cat in_voltage1_raw 
1063
/sys/devices/platform/soc/2100000.aips-bus/2198000.adc/iio:device0 # cat in_voltage_scale 
0.805664062
```

1063 * 0.805664062 大约0.8v

一根杜邦线把GPIO1接到 3.3v 上（不能大于这个电压！）

```bash
/sys/devices/platform/soc/2100000.aips-bus/2198000.adc/iio:device0 # cat in_voltage1_raw 
4032
```

4032 * 0.805664062 大约3.3v

一根杜邦线把GPIO1街道 0v 上

```bash
/sys/devices/platform/soc/2100000.aips-bus/2198000.adc/iio:device0 # cat in_voltage1_raw 
6
```

6 * 0.805664062 大约 0v

### 测试APP

直接使用正点原子团队的 adcAPP.c 

编译
```bash
arm-linux-gnueabihf-gcc adcAPP.c -o adcAPP
```

```bash
/lib/modules/4.1.15 # chmod +x adcAPP
/lib/modules/4.1.15 # ./adcAPP
ADC原始值：8，电压值：0.006V
ADC原始值：9，电压值：0.007V
ADC原始值：18，电压值：0.015V
```

```bash
/lib/modules/4.1.15 # ./adcAPP
ADC原始值：4025，电压值：3.243V
ADC原始值：4025，电压值：3.243V
ADC原始值：4034，电压值：3.250V
```