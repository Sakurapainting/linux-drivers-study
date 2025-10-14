# Regmap

## Regmap - Regmap简介

Linux 下使用 i2c_transfer 来读写 I2C 设备中的寄存器，SPI 接口的话使用 spi_write/spi_read等。I2C/SPI 芯片又非常的多，因此 Linux 内核里面就会充斥了大量的 i2c_transfer 这类的冗余代码，再者，代码的复用性也会降低。比如 icm20608 这个芯片既支持 I2C 接口，也支持 SPI 接口。假设我们在产品设计阶段一开始将 icm20608 设计为 SPI 接口，但是后面发现 SPI 接口不够用，或者 SOC 的引脚不够用，我们需要将 icm20608 改为 I2C 接口。这个时候 icm20608 的驱动就要大改，我们需要将 SPI 接口函数换为 I2C 的，工作量比较大。

Linux 内核引入了 regmap 模型，regmap 将寄存器访问的共同逻辑抽象出来，驱动开发人员不需要再去纠结使用 SPI 或者 I2C 接口 API 函数，统一使用 regmap API函数。这样的好处就是统一使用 regmap，降低了代码冗余，提高了驱动的可以移植性。

regmap模型的重点在于：

- 通过 regmap 模型提供的统一接口函数来访问器件的寄存器，SOC 内部的寄存器也可以使用 regmap 接口函数来访问。
- regmap 是 Linux 内核为了减少慢速 I/O 在驱动上的冗余开销，提供了一种通用的接口来操作硬件寄存器。另外，regmap 在驱动和硬件之间添加了 cache，降低了低速 I/O 的操作次数，提高了访问效率，缺点是实时性会降低。

什么情况下会使用 regmap：

①、硬件寄存器操作，比如选用通过 I2C/SPI 接口来读写设备的内部寄存器，或者需要读写 SOC 内部的硬件寄存器。
②、提高代码复用性和驱动一致性，简化驱动开发过程。
③、减少底层 I/O 操作次数，提高访问效率。

## Regmap - Regmap驱动框架

regmap 框架分为三层：

①、底层物理总线：regmap 就是对不同的物理总线进行封装，目前 regmap 支持的物理总线有 i2c、i3c、spi、mmio、sccb、sdw、slimbus、irq、spmi 和 w1。
②、regmap 核心层，用于实现 regmap，我们不用关心具体实现。
③、regmap API 抽象层，regmap 向驱动编写人员提供的 API 接口，驱动编写人员使用这些API 接口来操作具体的芯片设备，也是驱动编写人员重点要掌握的。

### regmap 结构体

Linux 内核将 regmap 框架抽象为 regmap 结构体，这个结构体定义在文件 drivers/base/regmap/internal.h 中

### regmap_config 结构体

regmap_config 结构体就是用来初始化 regmap 的，这个结构体也定义在include/linux/regmap.h 文件中

## Regmap - Regmap操作函数

regmap 支持多种物理总线，比如 I2C 和 SPI，我们需要根据所使用的接口来选择合适的 regmap 初始化函数。Linux 内核提供了针对不同接口的 regmap 初始化函数，SPI 接口初始化函数为 regmap_init_spi

```c
struct regmap * regmap_init_spi(struct spi_device
*spi, const struct regmap_config *config)
```

- spi：需要使用 regmap 的 spi_device。
- config：regmap_config 结构体，需要程序编写人员初始化一个 regmap_config 实例，然后将其地址赋值给此参数。
- 返回值：申请到的并进过初始化的 regmap。

I2C 接口的 regmap 初始化函数为 regmap_init_i2c

```c
struct regmap * regmap_init_i2c(struct i2c_client
*i2c, const struct regmap_config *config)
```

- i2c：需要使用 regmap 的 i2c_client。
- config：regmap_config 结构体，需要程序编写人员初始化一个 regmap_config 实例，然后将其地址赋值给此参数。
- 返回值：申请到的并进过初始化的 regmap。

在退出驱动的时候需要释放掉申请到的 regmap，不管是什么接口，全部使用 regmap_exit 这个函数来释放 regmap

```c
void regmap_exit(struct regmap *map)
```

我们一般会在 probe 函数中初始化 regmap_config，然后申请并初始化 regmap。

不管是 I2C 还是 SPI 等接口，还是 SOC 内部的寄存器，对于寄存器的操作就两种：读和写。regmap 提供了最核心的两个读写操作：regmap_read 和 regmap_write。这两个函数分别用来读/写寄存器

```c
int regmap_read(struct regmap *map, unsigned int reg, unsigned int *val)
```

- map：要操作的 regmap。
- reg：要读的寄存器。
- val：读到的寄存器值。
- 返回值：0，读取成功；其他值，读取失败。

```c
int regmap_write(struct regmap *map, unsigned int reg, unsigned int val)
```

- map：要操作的 regmap。
- reg：要写的寄存器。
- val：要写的寄存器值。
- 返回值：0，写成功；其他值，写失败。

regmap_update_bits 函数，此函数用来修改寄存器指定的 bit

```c
int regmap_update_bits (struct regmap *map, unsigned int reg, unsigned int mask, unsigned int val)
```

- map：要操作的 regmap。
- reg：要操作的寄存器。
- mask：掩码，需要更新的位必须在掩码中设置为 1。
- val：需要更新的位值。
- 返回值：0，写成功；其他值，写失败。

比如要将寄存器的 bit1 和 bit2 置 1，那么 mask 应该设置为 0X00000011，此时 val 的 bit1 和 bit2 应该设置为 1，也就是 0Xxxxxxx11。如果要清除寄存器的 bit4 和 bit7，那么 mask 应该设置为 0X10010000，val 的 bit4 和 bit7 设置为 0，也就是 0X0xx0xxxx。

regmap_bulk_read 函数，此函数用于读取多个寄存器的值

```c
int regmap_bulk_read(struct regmap *map, unsigned int reg, void *val, size_t val_count)
```

- map：要操作的 regmap。
- reg：要读取的第一个寄存器。
- al：读取到的数据缓冲区。
- val_count：要读取的寄存器数量。
- 返回值：0，写成功；其他值，读失败。

另外也有多个寄存器写函数 regmap_bulk_write

```c
int regmap_bulk_write(struct regmap *map, unsigned int reg, const void *val, size_t val_count)
```

- map：要操作的 regmap。
- reg：要写的第一个寄存器。
- val：要写的寄存器数据缓冲区。
- val_count：要写的寄存器数量。
- 返回值：0，写成功；其他值，读失败。

## Regmap - regmap_config 掩码设置

结构体 regmap_config 里面有三个关于掩码的成员变量：read_flag_mask 和 write_flag_mask，这二个掩码非常重要。

icm20608 支持 i2c 和 spi 接口，但是当使用 spi 接口的时候，读取 icm20608 寄存器的时候地址最高位必须置 1，写内部寄存器的是时候地址最高位要设置为 0。因此这里就涉及到对寄存器地址最高位的操作。 

在标准的SPI驱动中，要将寄存器地址bit7置1，要用

```c
txdata[0] = reg | 0x80;
```

表示这是一个读操作

当我们使用 regmap 的时候就不需要手动将寄存器地址的 bit7 置 1，在初始化 regmap_config的时候直接将 read_flag_mask 设置为 0X80 即可，这样通过 regmap 读取 SPI 内部寄存器的时候就会将寄存器地址与 read_flag_mask 进行或运算，结果就是将 bit7 置 1，但是整个过程不需要我们来操作，全部由 regmap 框架来完成的。

同理 write_flag_mask 用法也一样的，只是 write_flag_mask 用于写寄存器操作。

由于regmap_spi 默认将 read_flag_mask 设置为 0X80，当你所使用的 SPI 设备不需要读掩码，在初始化 regmap_config 的时候一定要将 read_flag_mask 设置为 0X00。

## Regmap - 程序编写

### 设备树

这里我们使用 Linux 内核自带的片选信号，而不是自己操作片选信号高低，所以要选硬件片选信号，带"s"的这个！！！

```c
    cs-gpios = <&gpio1 20 GPIO_ACTIVE_LOW>;		// 硬件片选引脚
    // cs-gpio = <&gpio1 20 GPIO_ACTIVE_LOW>;			// 软件片选引脚
```

### 头文件引用

```c
#include <linux/regmap.h>
```

### 修改设备结构体，添加 regmap 和 regmap_config

首先在 icm20608_dev 结构体里面添加 regmap 和 regmap_config

```c
struct regmap *regmap;
struct regmap_config regmap_config;
```

### 初始化 regmap

一般在 probe 函数中初始化 regmap

```c
    /* regmap申请和初始化初始化 */
    icm20608dev.regmap_config.reg_bits = 8;
    icm20608dev.regmap_config.val_bits = 8;
    icm20608dev.regmap_config.read_flag_mask = 0x80;
    icm20608dev.regmap = regmap_init_spi(spi, &icm20608dev.regmap_config);


    /* 初始化spi_device */
    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    /* 设置icm20608dev的私有数据 */
    icm20608dev.private_data = spi;   

    // 初始化icm20608硬件
    ret = icm20608_init_hw(&icm20608dev);
    if (ret < 0) {
        printk("icm20608 hardware init failed!\n");
    }
```

在 remove 函数中要删除 probe 里面申请的 regmap

```c
regmap_exit(icm20608dev.regmap);
```

### 读写设备内部寄存器

regmap 已经设置好了，接下来就是使用 regmap API 函数来读写 icm20608 内部寄存器了。以前我们使用 spi 驱动框架编写读写函数，现在直接使用 regmap_read、regmap_write 等函数

```c
// icm20608 读取单个寄存器
static u8 icm20608_read_reg(struct icm20608_dev* dev, u8 reg)
{
    unsigned int val = 0;
    int ret = 0;

    ret = regmap_read(dev->regmap, reg, &val);
    if (ret < 0) {
        printk("icm20608 regmap_read failed, reg=0x%x\n", reg);
        return 0;  // 返回默认值
    }
    return (u8)val;
}

// icm20608 写单个寄存器
static int icm20608_write_reg(struct icm20608_dev* dev, u8 reg, u8 val)
{
    return regmap_write(dev->regmap, reg, val);
}

int icm20608_readdata(struct icm20608_dev *dev)
{
	unsigned char data[14] = { 0 };
	int ret = 0;
	
	ret = regmap_bulk_read(dev->regmap, ICM20_ACCEL_XOUT_H, data, 14);
	if (ret < 0) {
		printk("icm20608 regmap_bulk_read failed!\n");
		return ret;
	}

	dev->accel_x_adc = (signed short)((data[0] << 8) | data[1]); 
	dev->accel_y_adc = (signed short)((data[2] << 8) | data[3]); 
	dev->accel_z_adc = (signed short)((data[4] << 8) | data[5]); 
	dev->temp_adc    = (signed short)((data[6] << 8) | data[7]); 
	dev->gyro_x_adc  = (signed short)((data[8] << 8) | data[9]); 
	dev->gyro_y_adc  = (signed short)((data[10] << 8) | data[11]);
	dev->gyro_z_adc  = (signed short)((data[12] << 8) | data[13]);
	
	return 0;
}
```

使用regmap的连续多个寄存器读写函数，也就不用自己写的读写函数了，可以删掉。