#### LCD 驱动 - Framebuffer 设备

把显存抽象后的设备，如 `/dev/fb0` ，允许上层应用程序在图形模式下直接对显示缓冲区进行读写操作，是HAL层（硬件抽象层），用户不用关心硬件层是怎么实施的。这些都由Framebuffer设备驱动完成。

- Framebuffer 在内核中的表现就是fb_info结构体，屏幕驱动的重点就是初始化 `fb_info` 结构体里的各个成员变量，初始化完成后，通过 `register_framebuffer` 函数向内核注册刚刚初始化以后的 `fb_info`。卸载驱动时，调用 `unregister_framebuffer` 来卸载前面注册的 `fb_info` 
- imx6ull.dtsi里，设备树：

```c
/ {
    soc {
        aips2: aips-bus@02100000 {
            lcdif: lcdif@021c8000 {
				compatible = "fsl,imx6ul-lcdif", "fsl,imx28-lcdif";
				reg = <0x021c8000 0x4000>;
				interrupts = <GIC_SPI 5 IRQ_TYPE_LEVEL_HIGH>;
				clocks = <&clks IMX6UL_CLK_LCDIF_PIX>,
					 <&clks IMX6UL_CLK_LCDIF_APB>,
					 <&clks IMX6UL_CLK_DUMMY>;
				clock-names = "pix", "axi", "disp_axi";
				status = "disabled";
			};
        };
    };
};
```

#### LCD 驱动 - NXP 提供的 LCD 驱动简析

- 在nxp给的驱动中，lcd控制器的驱动， `${sdk}/drivers/video/fbdev/mxsfb.c` ,为platform驱动框架，驱动和设备匹配后， `mxsfb_probe` 函数就会执行。
- 结构体 `mxsfb_info` 
- 给 `mxsfb_info` 申请内存，申请 `fb_info` ，然后将这两个联系起来
- `host_base` 就是内存映射后的 LCDIF 外设基地址
- `mxsfb_probe` 函数会调用 `mxsfb_init_fbinfo` 来初始化 `fb_info` 
- `fb_ops` 结构体，包含了 Framebuffer 设备驱动需要实现的各种操作函数指针，类似于字符设备驱动中的 file_operations。LCD 驱动程序会实现这些函数，并将 fb_ops 结构体赋值给 `fb_info->fbops`：

```c
static struct fb_ops mxsfb_ops = {
    .owner          = THIS_MODULE,
    .fb_check_var   = mxsfb_check_var,
    .fb_set_par     = mxsfb_set_par,
    .fb_setcolreg   = mxsfb_setcolreg,
    .fb_blank       = mxsfb_blank,
    .fb_pan_display = mxsfb_pan_display,
    .fb_fillrect    = cfb_fillrect,     // 使用内核通用函数
    .fb_copyarea    = cfb_copyarea,     // 使用内核通用函数
    .fb_imageblit   = cfb_imageblit,    // 使用内核通用函数
};

// 在 probe 函数中
fb_info->fbops = &mxsfb_ops;
```

- `mxsfb_probe` 函数重点工作：
  - 初始化 `fb_info` 并且向内核注册
  - 初始化 LCDIF 控制器
-  `mxsfb_init_fbinfo_dt` 函数会从设备树中读取相关属性信息

#### LCD 驱动 - 驱动编写

- 屏幕引脚设置
  - 将屏幕引脚电气属性改为0x49，也就是修改LCD驱动能力：把 `&pinctrl_lcdif_dat` 和 `&pinctrl_lcdif_ctrl` 里的 `0x79` 改为 `0x49` 
    - 0x79：高速模式
    - 0x49：中等速度模式
  - pinctrl_lcdif_reset 是板子的复位，并不是控制屏幕的复位，所以可以删了
  - 在内核 `${sdk}/Documentation/devicetree/bindings/fb/mxsfb.txt` 有 属性配置的说明
    - bits-per-pixel: <16> for RGB565, <32> for RGB888/666
    - bus-width: number of data lines. Could be <8>, <16>, <18> or <24>
  - 这里我们用RGB888，区别：
    - RGB565
        每个像素占 16位（2字节）。
        R（红色）：5位（0~31）
        G（绿色）：6位（0~63）
        B（蓝色）：5位（0~31）
        优点：数据量小，适合嵌入式、低成本显示屏。
        缺点：颜色精度较低，色彩不够细腻。
    - RGB888
        每个像素占 24位（3字节）。
        R（红色）：8位（0~255）
        G（绿色）：8位（0~255）
        B（蓝色）：8位（0~255）
        优点：颜色精度高，色彩丰富，显示效果好。
        缺点：数据量大，对带宽和存储要求高。
  - 在内核 `${sdk}/Documentation/devicetree/bindings/video/display-timing.txt` 有 `display-timings` 属性配置的说明

**参考屏幕数据手册，我这个是4.3寸 800x480**
