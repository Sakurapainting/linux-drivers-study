#### 内核自带 LED 驱动使能

- 内核自带的驱动，都是通过图形化界面配置，选择使能与否。

```bash
# 在SDK根目录
make menuconfig
```

- Device Driver

  - -> LED Support
    - -> LED Support for GPIO connected LEDs

- 按 `y` 选择编译到内核

- 在 menuconfig 可视化菜单做的修改会在 `.config` 文件中体现。

```bash
# 在SDK根目录
vi .config
```

- 其中，用 `/CONFIG_LEDS_GPIO` 搜索

```conf
CONFIG_LEDS_GPIO=y
```

- 看到注释被打开，然后编译应用修改

```bash
# 在SDK根目录
make -j16
```

- 在 linux 内核源码中一般驱动文件夹下 Makefile 会使用 CONFIG_XXX 来决定要编译哪个文件。

```makefile
obj-${CONFIG_LEDS_GPIO} += leds-gpio.o
```

也就是

```makefile
obj-y += leds-gpio.o
```

`leds-gpio.c` 就是 linux 内核源码中 leds-gpio 的驱动文件

#### 内核自带 LED 驱动使用

- 首先将驱动编译进内核里
- 根据绑定文档（`${KERNEL_SDK}/Documentation/devicetree/bindings/leds`）在设备树里面添加对应的设备节点信息。

  - 如果无设备树，那么就要使用 platform_device_register 向总线注册设备。
  - 如果有设备树，那么就直接修改设备树，按要求添加指定节点。

- 在根节点下新建

```c
dtsleds {
		compatible = "gpio-leds";

		led0 {

		};
	};
```

- 注意，compatible 属性一定要和 `自带驱动` 对应，驱动文件中有

```c
.of_match_table = of_gpio_leds_match,
```

去 `of_gpio_leds_match` 定义中，找到

```c
.compatible = "gpio-leds",
```

补充完整：

```c
dtsleds {
		compatible = "gpio-leds";

		led0 {
			label = "red";  // 任意名字

            // pinctrl
			pinctrl-names = "default";
			pinctrl-0 = <&pinctrl_gpioled>;

			gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
			default-state = "off";
		};
	};
```

再试试心跳灯：

```c
dtsleds {
		compatible = "gpio-leds";

		led0 {
			label = "red";
			pinctrl-names = "default";
			pinctrl-0 = <&pinctrl_gpioled>;
			gpios = <&gpio1 3 GPIO_ACTIVE_LOW>;
			default-state = "on";
			linux,default-trigger = "heartbeat";
		};
	};
```

- 开启后，除了可以在设备树里关闭然后重新编译以外，还可以再用户空间里关闭。

```bash
cd /sys/devices/platform/dtsleds/leds/red

/sys/devices/platform/dtsleds/leds/red # ls
brightness      max_brightness  subsystem       uevent
device          power           trigger
```

- 因为当前是 trigger 在执行心跳灯，所以要先关闭 trigger，才能写开关控制亮灭。
- 关闭方法就是 echo 写 none 进 trigger。同理，写 heartbeat 进去就是打开心跳灯。
- 同理，开关灯就是向 brightness 写 0/1 控制灭亮。

```bash
/sys/devices/platform/dtsleds/leds/red # cat trigger
none rc-feedback nand-disk mmc0 mmc1 timer oneshot [heartbeat] backlight gpio

/sys/devices/platform/dtsleds/leds/red # echo heartbeat > trigger
/sys/devices/platform/dtsleds/leds/red # echo none > trigger

/sys/devices/platform/dtsleds/leds/red # echo 1 > brightness
/sys/devices/platform/dtsleds/leds/red # echo 0 > brightness
```

往 brightness 里写 0/1 就可以控制 off/on

- **这里一定要检查 gpio 是否有冲突，ctrl+f 搜索 `gpio1 3` ，看看是否有其他地方用到了，找到一个我们前面自己添加的节点 `gpioled` ，但是这个要模块手动注册时才会使用，而不是内核一开始就加载，所以可以不用注释掉**
