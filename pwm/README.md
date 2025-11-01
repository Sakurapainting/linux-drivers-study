# PWM驱动

## pwm驱动 - 简析

I.MX6ULL 有 8 路 PWM 输出，因此对应 8 个 PWM 控制器，所有在设备树下就有 8 个PWM 控制器节点。这 8 路 PWM 都属于 I.MX6ULL 的 AIPS-1 域，但是在设备树 imx6ull.dtsi 中分为了两部分，PWM1~PWM4 在一起，PWM5~PWM8 在一起，这 8 路 PWM 并没有全部放到一起。

本章我们使用GPIO1_IO04 这个引脚来完成 PWM 实验，而 GPIO1_IO04 就是 PWM3 的输出引脚。

```c
pwm3: pwm@02088000 {
    compatible = "fsl,imx6ul-pwm", "fsl,imx27-pwm";
    reg = <0x02088000 0x4000>;
    interrupts = <GIC_SPI 85 IRQ_TYPE_LEVEL_HIGH>;
    clocks = <&clks IMX6UL_CLK_PWM3>,
            <&clks IMX6UL_CLK_PWM3>;
    clock-names = "ipg", "per";
    #pwm-cells = <2>;
};
```

compatible 属性值有两个“fsl,imx6ul-pwm”和“fsl,imx27-pwm”，所以在整个 Linux源码里面搜索这两个字符窜即可找到 I.MX6ULL 的 PWM 驱动文件，这个文件就是drivers/pwm/pwm-imx.c

关 于 I.MX6ULL 的 PWM 节 点 更 为 详 细 的 信 息 请 参 考 对 应 的 绑 定 文 档 ：Documentation/devicetree/bindings/pwm/ imx-pwm.txt

## pwm驱动 - pwm子系统

PWM子系统的核心是 pwm_chip 结构体，定义在文件 include/linux/pwm.h

pwm_ops 中的这些函数不一定全部实现，但是像 config、enable 和 disable 这些肯定是需要实现的。

PWM 子系统驱动的核心就是初始化 pwm_chip 结构体各成员变量，然后向内核注册初始化完成以后的 pwm_chip。这里就要用到 pwmchip_add 函数，此函数定义在 drivers/pwm/core.c

## pwm驱动 - 源码分析

imx_pwm_config_v2 函数就是最终操作 I.MX6ULL 的 PWM 外设寄存器，进行实际配置的函数。imx_pwm_set_enable_v2 就是具体使能 PWM 的函数

```c
static struct pwm_ops imx_pwm_ops = {
    .enable = imx_pwm_enable,
    .disable = imx_pwm_disable,
    .config = imx_pwm_config,
    .owner = THIS_MODULE,
};
```

### probe

申请chip结构体内存，并初始化

从设备树中获取 PWM 节点中关于 PWM 控制器的地址信息，然后再进行内存映射，得到 PWM 控制器的基地址

设置 imx 的 config 和 set_enable 这两个成员变量为 data->config 和 data->set_enable，也就是 imx_pwm_config_v2 和 imx_pwm_set_enable_v2

### imx_pwm_set_enable_v2

```c
static void imx_pwm_set_enable_v2(struct pwm_chip *chip, bool enable)
{
    struct imx_chip *imx = to_imx_chip(chip);
    u32 val;
    val = readl(imx->mmio_base + MX3_PWMCR);
    if (enable)
        val |= MX3_PWMCR_EN;
    else
        val &= ~MX3_PWMCR_EN;
    writel(val, imx->mmio_base + MX3_PWMCR);
}
```

- 读取 PWMCR 寄存器的值。
- 如果 enable 为真，表示使能 PWM，将 PWMCR 寄存器的 bit0 置 1 即可，宏MX3_PWMCR_EN 为(1<<0)。
- 如果 enable 不为真，表示关闭 PWM，将 PWMCR 寄存器的 bit0 清 0 即可。
- 将新的 val 值写入到 PWMCR 寄存器中

### 1 << 0 为什么要这样写？

- 明确表示操作的是第 n 位
- 方便定义多个位掩码，如 (1<<0), (1<<1), (1<<2) 等

### imx_pwm_config_v2

- 根据参数 duty_ns和 period_ns来计算出应该写入到寄存器里面的值 duty_cycles 和 period_cycles
- 将计算得到的 duty_cycles 写入到 PWMSAR 寄存器中，设置 PWM 的占空比
- 将计算得到的 period_cycles 写入到 PWMPR 寄存器中，设置 PWM 的频率。

## pwm驱动 - 设备树

ALPHA 开发板上的 JP2 排针引出了 GPIO1_IO04 这个引脚

GPIO1_IO04 可以作为 PWM3 的输出引脚

iomuxc里添加

```c
&iomuxc {
    /*you PWM3*/
    pinctrl_pwm3: pwm3grp {
        fsl,pins = <
            MX6UL_PAD_GPIO1_IO04__PWM3_OUT 0x110b0
        >;
    };    
};
```

```c
/*you pwm3*/
&pwm3 {
	pinctrl-names = "default";
	pinctrl-0 = <&pinctrl_pwm3>;
	clocks = <&clks IMX6UL_CLK_PWM3>,
	        <&clks IMX6UL_CLK_PWM3>;
	status = "okay";
};
```

设置时钟，第 4 行设置 ipg 时钟，第 5 行设置 per 时钟。

## pwm驱动 - ipg时钟和per时钟详解

### 两种时钟的作用

在 i.MX6ULL 芯片中，外设通常需要两种时钟：

#### 1. IPG 时钟 (IP Bus Clock)

- **全称：** IP (Intellectual Property) Bus Clock
- **作用：** 为外设的**寄存器接口**提供时钟
- **功能：** 用于 CPU 通过总线读写外设寄存器
- **特点：** 
  - 是系统总线时钟的分频
  - 相对固定，一般不需要太高频率
  - 外设内部逻辑的工作时钟

**简单理解：** IPG 时钟是用于 CPU 访问外设寄存器的时钟，没有它就无法配置外设。

#### 2. PER 时钟 (Peripheral Clock)

- **全称：** Peripheral Functional Clock
- **作用：** 为外设的**功能模块**提供工作时钟
- **功能：** 外设实际执行功能时使用的时钟
- **特点：**
  - 是外设功能的时钟源
  - 频率可能较高，影响外设性能
  - 对于 PWM，这个时钟决定 PWM 的计数频率

**简单理解：** PER 时钟是外设实际工作的时钟，决定外设功能的精度和性能。

## pwm驱动 - 使能PWM驱动

```
-> Device Drivers
-> Pulse-Width Modulation (PWM) Support
-> <*> i.MX PWM support
```

## pwm驱动 - 测试

我们可以直接在用户层来配置 PWM，进入目录 /sys/class/pwm 中

```bash
/ # cd /sys/class/pwm/
/sys/class/pwm # ls
pwmchip0  pwmchip2  pwmchip4  pwmchip6
pwmchip1  pwmchip3  pwmchip5  pwmchip7
```

首先需要调出 pwmchip2 下的 pwm0 目录

```bash
echo 0 > /sys/class/pwm/pwmchip2/export
```

执行完成会在 pwmchip2 目录下生成一个名为“pwm0”的子目录

```bash
/sys/class/pwm # echo 0 > /sys/class/pwm/pwmchip2/export
/sys/class/pwm # ls ./pwmchip2/
device     npwm       pwm0       uevent
export     power      subsystem  unexport
```

输入如下命令使能 PWM3：

```bash
echo 1 > /sys/class/pwm/pwmchip2/pwm0/enable
```

设置 PWM3 的频率
注意，这里设置的是周期值，单位为 ns，比如 20KHz 频率的周期就是 50000ns，输入如下命令:

```bash
echo 50000 > /sys/class/pwm/pwmchip2/pwm0/period
```

设置 PWM3 的占空比
这里不能直接设置占空比，而是设置的一个周期的 ON 时间，也就是高电平时间，比如20KHz 频率下 20%占空比的 ON 时间就是 10000

```bash
echo 10000 > /sys/class/pwm/pwmchip2/pwm0/duty_cycle
```

设置完成使用示波器查看波形是否正确