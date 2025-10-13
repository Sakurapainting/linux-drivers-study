# RTC

## RTC 驱动 - imx6ull 内核自带驱动简析

- 在 `imx6ull.dtsi`

```c
snvs_rtc: snvs-rtc-lp {
    compatible = "fsl,sec-v4.0-mon-rtc-lp";
    regmap = <&snvs>;
    offset = <0x34>;
    interrupts = <GIC_SPI 19 IRQ_TYPE_LEVEL_HIGH>, <GIC_SPI 20 IRQ_TYPE_LEVEL_HIGH>;
};
```

- 选 LP 的原因
  - SNVS_LP：专用的 always-powered-on 电源域，系统主电源和备用电源都可以为其供电
  - SNVS_HP：系统（芯片）电源
- 根据 compatible 找到驱动文件，在 `rtc-snvs.c` ，里面就是初始化 `rtc_device` 并注册
- `rtc_device` 结构体里面重点是 `rtc_class_ops` 操作集
- 包含在 `snvs_rtc_data` 结构体中：

```c
struct snvs_rtc_data {
    struct rtc_device *rtc;
    struct regmap *regmap;
    int offset;
    int irq;
    struct clk *clk;
};
```

- 地址：0x020cc000+0x34 = 0x020cc034
- 首先从设备树里面获取 SNCS RTC 外设寄存器，初始化 RTC，申请中断处理闹钟，中断处理函数是 `snvs_rtc_irq_handler` ，最后通过 `devm_rtc_device_register` 函数向内核注册 `rtc_device` ，重点是注册的时候设置了 `snvs_rtc_ops` ：

```c
static const struct rtc_class_ops snvs_rtc_ops = {
    .read_time = snvs_rtc_read_time,
    .set_time = snvs_rtc_set_time,
    .read_alarm = snvs_rtc_read_alarm,
    .set_alarm = snvs_rtc_set_alarm,
    .alarm_irq_enable = snvs_rtc_alarm_irq_enable,
};
```

- 当应用通过 ioctl 读取 RTC 时间的时候，RTC 核心层的 `rtc_dev_ioctl` 会执行，通知 cmd 来决定具体操作，比如 `RTC_ALM_READ` 就是读取闹钟，此时 `rtc_read_alarm` 就会执行，会找到具体的 `rtc_device` ，运行其下的 ops 里面的 `read_alarm`

## RTC 驱动 - 使用和测试

- 查看、设置系统时间：

```bash
/ # date --help
BusyBox v1.29.0 (2019-06-13 11:08:23 CST) multi-call binary.

Usage: date [OPTIONS] [+FMT] [TIME]

Display time (using +FMT), or set time

	[-s,--set] TIME	Set time to TIME
	-u,--utc	Work in UTC (don't convert to local time)
	-R,--rfc-2822	Output RFC-2822 compliant date string
	-I[SPEC]	Output ISO-8601 compliant date string
			SPEC='date' (default) for date only,
			'hours', 'minutes', or 'seconds' for date and
			time to the indicated precision
	-r,--reference FILE	Display last modification time of FILE
	-d,--date TIME	Display TIME, not 'now'
	-D FMT		Use FMT for -d TIME conversion

Recognized TIME formats:
	hh:mm[:ss]
	[YYYY.]MM.DD-hh:mm[:ss]
	YYYY-MM-DD hh:mm[:ss]
	[[[[[YY]YY]MM]DD]hh]mm[.ss]
	'date TIME' form accepts MMDDhhmm[[YY]YY][.ss] instead
/ # date -s "2025-09-09 16:34:00"
Tue Sep  9 16:34:00 UTC 2025
/ # date
Tue Sep  9 16:34:04 UTC 2025
```

- 写入 RTC 寄存器（需要开发板装有纽扣电池供 snvs）。**但是，imx6 的内部 RTC 耗电太快，过不了多久就要换新纽扣电池。解决方案是采取外部时钟的方式，比如外部 RTC——PCF85063（I2C 接口）**：

```bash
/ # hwclock -w
```

- 断电后，等一会（等电容的电耗尽）再上电，能读取寄存器中的日期
