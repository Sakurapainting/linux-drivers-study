# IIO

## IIO - IIO 子系统简介

IIO 全称是 Industrial I/O ，IIO 就是为 ADC/DAC 类传感器准备的。内部 ADC 将原始的模拟数据转换为数字量，然后通过其他的通信接口，比如 IIC、SPI 等传输给 SOC。

因此，当你使用的传感器本质是 ADC 或 DAC 器件的时候，可以优先考虑使用 IIO 驱动框架。

## IIO - iio_dev

此设备结构体定义在 include/linux/iio/iio.h 文件中

### iio_dev 结构体重要成员变量

1. **info**：指向 `iio_info` 结构体，包含设备的读写操作函数
   - `read_raw()`：读取通道原始数据
   - `write_raw()`：写入通道原始数据

2. **channels**：指向 `iio_chan_spec` 数组，定义每个通道的属性
   - 通道类型（电压、电流、温度等）
   - 通道索引
   - 数据格式

3. **num_channels**：通道总数

4. **name**：设备名称，会在 `/sys/bus/iio/devices/` 下显示

5. **priv**：指向驱动私有数据的指针，通过 `iio_priv()` 访问

6. **modes**：设备支持的操作模式，可以是以下值的组合：
   - `INDIO_DIRECT_MODE`：直接模式，通过 sysfs 直接读取
   - `INDIO_BUFFER_TRIGGERED`：缓冲触发模式，使用触发器和缓冲区
   - `INDIO_BUFFER_HARDWARE`：硬件缓冲模式，硬件自动填充缓冲区
   - `INDIO_BUFFER_SOFTWARE`：软件缓冲模式，软件填充缓冲区
   - `INDIO_EVENT_TRIGGERED`：事件触发模式

7. **setup_ops**：setup_ops 为 iio_buffer_setup_ops 结构体类型

```c
struct iio_buffer_setup_ops {
    int (*preenable)(struct iio_dev *);     /* 缓冲区使能之前调用 */
    int (*postenable)(struct iio_dev *);    /* 缓冲区使能之后调用 */
    int (*predisable)(struct iio_dev *);    /* 缓冲区禁用之前调用 */
    int (*postdisable)(struct iio_dev *);   /* 缓冲区禁用之后调用 */
    bool (*validate_scan_mask)(struct iio_dev *indio_dev,
        const unsigned long *scan_mask);    /* 检查扫描掩码是否有效 */
};
```

- iio_buffer_setup_ops 里面都是一些回调函数，在使能或禁用缓冲区的时候会调用这些函数。如果未指定的话就默认使用 iio_triggered_buffer_setup_ops。

### iio_dev 申请与释放

在使用之前要先申请 iio_dev，申请函数为 iio_device_alloc

```c
struct iio_dev *iio_device_alloc(int sizeof_priv)
```

- sizeof_priv：私有数据内存空间大小，一般我们会将自己定义的设备结构体变量作为 iio_dev的私有数据，这样可以直接通过 iio_device_alloc 函数同时完成 iio_dev 和设备结构体变量的内存申请。申请成功以后使用 iio_priv 函数来得到自定义的设备结构体变量首地址。
- 返回值：如果申请成功就返回 iio_dev 首地址，如果失败就返回 NULL。

一般 iio_device_alloc 和 iio_priv 之间的配合使用如下所示：

```c
struct icm20608_dev *dev;
struct iio_dev *indio_dev;

/* 1、申请 iio_dev 内存 */
indio_dev = iio_device_alloc(sizeof(*dev));
if (!indio_dev)
    return -ENOMEM;

/* 2、获取设备结构体变量地址 */
dev = iio_priv(indio_dev);
```

如果要释放 iio_dev，需要使用 iio_device_free 函数

```c
void iio_device_free(struct iio_dev *indio_dev)
```

- indio_dev：需要释放的 iio_dev。
- 返回值：无。

也可以使用 devm_iio_device_alloc 来分配 iio_dev ， 这样就不需要我们手动调用 iio_device_free 函数完成 iio_dev 的释放工作。

### iio_dev 注册与注销

前面分配好 iio_dev 以后就要初始化各种成员变量，初始化完成以后就需要将 iio_dev 注册到内核中，需要用到 iio_device_register 函数

```c
int iio_device_register(struct iio_dev *indio_dev)
```

- indio_dev：需要注册的 iio_dev。
- 返回值：0，成功；其他值，失败。

如果要注销 iio_dev 使用 iio_device_unregister 函数

```c
void iio_device_unregister(struct iio_dev *indio_dev)
```

- indio_dev：需要注销的 iio_dev。
- 返回值：0，成功；其他值，失败。

## IIO - iio_info

iio_dev 有个成员变量：info，为 iio_info 结构体指针变量，这个是我们在编写 IIO 驱动的时候需要着重去实现的，因为用户空间对设备的具体操作最终都会反映到 iio_info 里面。iio_info 结构体定义在 include/linux/iio/iio.h 中

read_raw 和 write_raw 函数，这两个函数就是最终读写设备内部数据的操作函数，需要程序编写人员去实现的。比如应用读取一个陀螺仪传感器的原始数据，那么最终完成工作的就是 read_raw 函数，我们需要在 read_raw 函数里面实现对陀螺仪芯片的读取操作。同理，write_raw 是应用程序向陀螺仪芯片写数据，一般用于配置芯片，比如量程、数据速率等。

```c
int (*read_raw)(struct iio_dev *indio_dev,
                struct iio_chan_spec const *chan,
                int *val,
                int *val2,
                long mask);

int (*write_raw)(struct iio_dev *indio_dev,
                 struct iio_chan_spec const *chan,
                 int val,
                 int val2,
                 long mask);
```

- indio_dev：需要读写的 IIO 设备。
- chan：需要读取的通道。
- val，val2：对于 read_raw 函数来说 val 和 val2 这两个就是应用程序从内核空间读取到数据，一般就是传感器指定通道值，或者传感器的量程、分辨率等。对于 write_raw 来说就是应用程序向设备写入的数据。val 和 val2 共同组成具体值，val 是整数部分，val2 是小数部分。但是val2 也是对具体的小数部分扩大 N 倍后的整数值，因为不能直接从内核向应用程序返回一个小数值。

比如现在有个值为 1.00236，那么 val 就是 1，vla2 理论上来讲是 0.00236，但是我们需要对 0.00236 扩大 N 倍，使其变为整数，这里我们扩大 1000000 倍，那么 val2 就是 2360。因此val=1，val2=2360。扩大的倍数我们不能随便设置，而是要使用 Linux 定义的倍数，Linux 内核里面定义的数据扩大倍数，或者说数据组合形式。

mask：掩码，用于指定我们读取的是什么数据，比如 ICM20608 这样的传感器，他既有原始的测量数据，比如 X,Y,Z 轴的陀螺仪、加速度计等，也有测量范围值，或者分辨率。比如加速度计测量范围设置为±16g，那么分辨率就是 32/65536≈0.000488，我们只有读出原始值以及对应的分辨率(量程)，才能计算出真实的重力加速度。此时就有两种数据值：传感器原始值、分辨率。Linux 内核使用 IIO_CHAN_INFO_RAW 和 IIO_CHAN_INFO_SCALE 这两个宏来表示原始值以及分辨率，这两个宏就是掩码。至于每个通道可以采用哪几种掩码，这个在我们初始化通道的时候需要驱动编写人员设置好。

write_raw_get_fmt 用于 设 置用 户 空间 向 内核 空 间写入的数据格式 ，write_raw_get_fmt 函数决定了 wtite_raw 函数中 val 和 val2 的意义，也就是表 75.1.2.1 中的组合形式。比如我们需要在应用程序中设置 ICM20608 加速度计的量程为±8g，那么分辨率就是16/65536 ≈ 0.000244 ， 我 们 在 write_raw_get_fmt 函 数 里 面 设 置 加 速 度 计 的 数 据 格 式 为IIO_VAL_INT_PLUS_MICRO。那么我们在应用程序里面向指定的文件写入 0.000244 以后，最终传递给内核驱动的就是 0.000244*1000000=244。也就是 write_raw 函数的 val 参数为 0，val2参数为 244。

## IIO - iio_chan_spec

IIO 的核心就是通道，一个传感器可能有多路数据，比如一个 ADC 芯片支持 8 路采集，那么这个 ADC 就有 8 个通道。

ICM20608，这是一个六轴传感器，可以输出三轴陀螺仪(X、Y、Z)、三轴加速度计(X、Y、Z)和一路温度，也就是一共有 7 路数据，因此就有 7 个通道。注意，三轴陀螺仪或加速度计的 X、Y、Z 这三个轴，每个轴都算一个通道。

Linux 内核使用 iio_chan_spec 结构体来描述通道，定义在 include/linux/iio/iio.h 文件中

### iio_chan_spec 结构体重要成员变量

```c
struct iio_chan_spec {
    enum iio_chan_type type;        /* 通道类型 */
    int channel;                    /* 通道索引 */
    int channel2;                   /* 修饰通道索引 */
    unsigned long address;          /* 通道地址,通常是寄存器地址 */
    int scan_index;                 /* 扫描索引 */
    struct {
        char sign;                  /* 's'=有符号,'u'=无符号 */
        u8 realbits;               /* 实际有效位数 */
        u8 storagebits;            /* 存储位数 */
        u8 shift;                  /* 右移位数 */
        u8 repeat;                 /* 重复次数 */
        enum iio_endian endianness; /* 字节序 */
    } scan_type;
    long info_mask_separate;        /* 当前通道独立的属性掩码 */
    long info_mask_shared_by_type;  /* 同类型通道共享的属性掩码 */
    long info_mask_shared_by_dir;   /* 同方向通道共享的属性掩码 */
    long info_mask_shared_by_all;   /* 所有通道共享的属性掩码 */
    const struct iio_event_spec *event_spec; /* 事件规格 */
    unsigned int num_event_specs;   /* 事件规格数量 */
    const struct iio_chan_spec_ext_info *ext_info; /* 扩展信息 */
    const char *extend_name;        /* 扩展名称 */
    const char *datasheet_name;     /* 数据手册中的名称 */
    unsigned modified:1;            /* 是否使用 channel2 修饰 */
    unsigned indexed:1;             /* 通道是否有索引 */
    unsigned output:1;              /* 是否为输出通道 */
    unsigned differential:1;        /* 是否为差分通道 */
};
```

#### 重点成员说明

1. **type**：通道类型，使用 `enum iio_chan_type` 枚举值：
   - `IIO_VOLTAGE`：电压通道
   - `IIO_CURRENT`：电流通道
   - `IIO_POWER`：功率通道
   - `IIO_ACCEL`：加速度计通道
   - `IIO_ANGL_VEL`：陀螺仪(角速度)通道
   - `IIO_MAGN`：磁力计通道
   - `IIO_LIGHT`：光照通道
   - `IIO_TEMP`：温度通道
   - `IIO_PROXIMITY`：接近传感器通道
   - `IIO_PRESSURE`：压力通道

2. **channel** 和 **channel2**：
   - `channel`：通道索引号，比如 X 轴=0，Y 轴=1，Z 轴=2
   - `channel2`：修饰通道，当 `modified=1` 时使用，用于进一步区分通道
   - 配合 `modified` 和 `indexed` 标志使用

3. **info_mask_xxx**：通道属性掩码，决定通道在 sysfs 下暴露哪些属性文件：
   - `info_mask_separate`：当前通道独立的属性
   - `info_mask_shared_by_type`：同类型通道共享的属性
   - 常用的掩码值：
     - `IIO_CHAN_INFO_RAW`：原始数据值
     - `IIO_CHAN_INFO_SCALE`：比例因子/分辨率
     - `IIO_CHAN_INFO_OFFSET`：偏移量
     - `IIO_CHAN_INFO_SAMP_FREQ`：采样频率

4. **scan_type**：定义数据的存储格式，用于缓冲模式：
   - `sign`：数据符号，'s'=有符号，'u'=无符号
   - `realbits`：有效数据位数
   - `storagebits`：实际存储位数(通常是 8/16/32/64)
   - `shift`：数据右移位数
   - `endianness`：字节序(大端/小端)

5. **indexed** 和 **modified**：
   - `indexed=1`：通道有索引，sysfs 文件名包含 channel 值
   - `modified=1`：使用 channel2 修饰，sysfs 文件名包含修饰符

## IIO - 驱动编写

IIO 框架主要用于 ADC 类的传感器，比如陀螺仪、加速度计、磁力计、光强度计等，这些传感器基本都是 IIC 或者 SPI 接口的。因此 IIO 驱动的基础框架就是 IIC 或者 SPI。有些 SOC 内部的 ADC 也会使用 IIO 框架，那么这个时候驱动的基础框架就是 platfrom。

### 添加头文件

```c
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/unaligned/be_byteshift.h>
```

### 使能内核 IIO 相关配置

Linux 内核默认使能了 IIO 子系统，但是有一些 IIO 模块没有选择上，这样会导致我们编译驱动的时候会提示某些 API 函数不存在，需要使能的项目如下：

```
-> Device Drivers
-> Industrial I/O support (IIO [=y])
-> [*]Enable buffer support within IIO
-> <*>Industrial I/O buffering based on kfifo
```

通道宏定义，用于陀螺仪和加速度计，第 28 行 modified 成员变量为 1，所以channel2 就 是通 道 修饰 符 ， 用来 指 定 X 、 Y 、Z 轴 。 第 30 行 设置 相 同 类型 的 通道IIO_CHAN_INFO_SCALE 属性相同，“scale”是比例的意思，在这里就是量程(分辨率)，因为ICM20608 的陀螺仪和加速度计的量程是可以调整的，量程不同分辨率也就不同。陀螺仪或加速度计的三个轴也是一起设置的，因此对于陀螺仪或加速度计而言，X、Y、Z 这三个轴的量程是共享的。第 31 行，设置每个通道的IIO_CHAN_INFO_RAW 和 IIO_CHAN_INFO_CALIBBIAS这两个属性都是独立的IIO_CHAN_INFO_RAW 表示 ICM20608 每个通道的原始值，这个肯定是每个通道独立的。IIO_CHAN_INFO_CALIBBIAS 是 ICM20608 每个通道的校准值，这个是ICM20608 的特性，不是所有的传感器都有校准值，一切都要以实际所使用的传感器为准。第 34行，设置扫描数据类型，也就是 ICM20608 原始数据类型，ICM20608 的陀螺仪和加速度计都是16 位 ADC，因此这里是通用的：为有符号类型、实际位数 16bit，存储位数 16bit，大端模式(ICM20608 数据寄存器为大端模式)。

```c
#define ICM20608_CHAN(_type, _channel2, _index)                    \
	{                                                             \
		.type = _type,                                        \
		.modified = 1,                                        \
		.channel2 = _channel2,                                \
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |	      \
				      BIT(IIO_CHAN_INFO_CALIBBIAS),   \
		.scan_index = _index,                                 \
		.scan_type = {                                        \
				.sign = 's',                          \
				.realbits = 16,                       \
				.storagebits = 16,                    \
				.shift = 0,                           \
				.endianness = IIO_BE,                 \
			     },                                       \
	}
```

- type: 通道类型
- modified: 设置为1,表示使用 channel2 修饰符
- channel2: 轴向修饰符(X/Y/Z)
- info_mask_shared_by_type: 共享信息掩码,这里包含 SCALE(量程/分辨率)
- info_mask_separate: 独立信息掩码
- RAW: 原始数据读取
- CALIBBIAS: 校准偏移值
- scan_index: 在扫描缓冲区中的索引位置
- scan_type: 扫描数据格式
- sign = 's': 有符号数
- realbits = 16: 实际数据位数
- storagebits = 16: 存储位数
- shift = 0: 无位移
- endianness = IIO_BE: 大端字节序

把字符设备相关的都删除，因为使用IIO框架，字符设备那一套就不需要了。

struct icm20608_dev 也不用实例一个全局的对象了，全都在函数内部使用。

建立iio框架：

```c
#include <asm/io.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/timer.h>
#include <linux/uaccess.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include <linux/regmap.h>
#include <linux/iio/iio.h>
#include <linux/iio/sysfs.h>
#include <linux/iio/buffer.h>
#include <linux/iio/trigger.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/unaligned/be_byteshift.h>
#include "icm20608reg.h"

#define ICM20608_CNT    1
#define ICM20608_NAME   "icm20608"

#define ICM20608_CHAN(_type, _channel2, _index)                    \
	{                                                             \
		.type = _type,                                        \
		.modified = 1,                                        \
		.channel2 = _channel2,                                \
        .info_mask_shared_by_type = BIT(IIO_CHAN_INFO_SCALE), \
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW) |	      \
				      BIT(IIO_CHAN_INFO_CALIBBIAS),   \
		.scan_index = _index,                                 \
		.scan_type = {                                        \
				.sign = 's',                          \
				.realbits = 16,                       \
				.storagebits = 16,                    \
				.shift = 0,                           \
				.endianness = IIO_BE,                 \
			     },                                       \
	}

// icm20608 通道
static const struct iio_chan_spec icm20608_channels[] = {

};

struct icm20608_dev {
    struct spi_device* spi;
    struct regmap *regmap;
    struct regmap_config regmap_config;
    struct mutex lock;
};

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

// icm20608 init
int icm20608_init_hw(struct icm20608_dev* dev)
{
    int ret = 0;
    u8 value = 0;
    
    ret = icm20608_write_reg(dev, ICM20_PWR_MGMT_1, 0X80); // 复位，复位后为0x40，睡眠模式
    if (ret < 0) {
        printk("icm20608 reset failed!\n");
        return ret;
    }
    mdelay(50);
    
    ret = icm20608_write_reg(dev, ICM20_PWR_MGMT_1, 0X01); // 关闭睡眠，自动选择时钟
    if (ret < 0) {
        printk("icm20608 wakeup failed!\n");
        return ret;
    }
    mdelay(50);

    value = icm20608_read_reg(dev, ICM20_WHO_AM_I);
    printk("icm20608 id = %#x\n", value);

    icm20608_write_reg(dev, ICM20_SMPLRT_DIV, 0x00); 	    // 输出速率是内部采样率
	icm20608_write_reg(dev, ICM20_GYRO_CONFIG, 0x18); 	    // 陀螺仪±2000dps量程 	
	icm20608_write_reg(dev, ICM20_ACCEL_CONFIG, 0x18); 	// 加速度计±16G量程 
	icm20608_write_reg(dev, ICM20_CONFIG, 0x04); 		    // 陀螺仪低通滤波BW=20Hz 
	icm20608_write_reg(dev, ICM20_ACCEL_CONFIG2, 0x04);    // 加速度计低通滤波BW=21.2Hz 
	icm20608_write_reg(dev, ICM20_PWR_MGMT_2, 0x00); 	    // 打开加速度计和陀螺仪所有轴 
	icm20608_write_reg(dev, ICM20_LP_MODE_CFG, 0x00); 	    // 关闭低功耗 
	icm20608_write_reg(dev, ICM20_FIFO_EN, 0x00);		    // 关闭FIFO	

    return 0;
}

// iio info
static const struct iio_info icm20608_info = {
    .driver_module = THIS_MODULE,

};

static int icm20608_probe(struct spi_device* spi){
    int ret = 0;
    struct icm20608_dev* dev;
    struct iio_dev *indio_dev;

    printk("icm20608_probe!\n");

    // 申请iio_dev , icm20608_dev
    indio_dev = devm_iio_device_alloc(&spi->dev, sizeof(*dev));
    if(indio_dev == NULL){
        printk("devm_iio_device_alloc failed!\n");
        ret = -ENOMEM;
        goto fail_iio_dev;
    }

    dev = iio_priv(indio_dev);          // 得到icm20608_dev首地址
    dev->spi = spi;                     // 保存spi_device指针
    spi_set_drvdata(spi, indio_dev);    // 保存indio_dev指针，使能够通过spi获取indio_dev
    mutex_init(&dev->lock);

    // init iio_dev
    indio_dev->dev.parent = &spi->dev;
    indio_dev->channels = icm20608_channels;
    indio_dev->num_channels = ARRAY_SIZE(icm20608_channels);
    indio_dev->name = ICM20608_NAME;
    indio_dev->modes = INDIO_DIRECT_MODE;       // 直接模式，提供sysfs接口
    indio_dev->info = &icm20608_info;

    // register iio_dev
    ret = iio_device_register(indio_dev);
    if (ret < 0) {
        printk("iio_device_register failed: %d\n", ret);
        goto fail_iio_register;
    }


    /* regmap申请和初始化初始化 */
    dev->regmap_config.reg_bits = 8;
    dev->regmap_config.val_bits = 8;
    dev->regmap_config.read_flag_mask = 0x80;
    dev->regmap = regmap_init_spi(spi, &dev->regmap_config);
    if (IS_ERR(dev->regmap)) {
        ret = PTR_ERR(dev->regmap);
        printk("regmap_init_spi failed: %d\n", ret);
        goto fail_regmap;
    }


    /* 初始化spi_device */
    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    // 初始化icm20608硬件
    ret = icm20608_init_hw(dev);
    if (ret < 0) {
        printk("icm20608 hardware init failed!\n");
        goto fail_init;
    }

    return 0;

fail_init:
    regmap_exit(dev->regmap);
fail_regmap:
    iio_device_unregister(indio_dev);
fail_iio_register:
fail_iio_dev:
    return ret;
}

static int icm20608_remove(struct spi_device* spi){
    int ret = 0;

    struct iio_dev *indio_dev = spi_get_drvdata(spi);
    struct icm20608_dev* dev;
    
    dev = iio_priv(indio_dev);

    // 注销iio_dev
    iio_device_unregister(indio_dev);

    // 删除regmap
    regmap_exit(dev->regmap);
    
    return ret;
}

// 传统匹配表
struct spi_device_id icm20608_idtable[] = {
    {"invensense,icm20608", 0},
    {}
};

// 设备树匹配表
static const struct of_device_id icm20608_of_match[] = {
    {.compatible = "invensense,icm20608"},
    {}
};

struct spi_driver icm20608_driver = {
    .driver = {
        .name = "icm20608",
        .owner = THIS_MODULE,
        .of_match_table = icm20608_of_match,
    },
    .probe  = icm20608_probe,
    .remove = icm20608_remove,
    .id_table = icm20608_idtable,
};

static int __init icm20608_init(void)
{
    int ret = 0;
    spi_register_driver(&icm20608_driver);

    return ret;
}

static void __exit icm20608_exit(void)
{
    spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
```

测试框架：

```bash
/lib/modules/4.1.15 # modprobe icm20608.ko 
icm20608_probe!
icm20608 id = 0xae
```

```bash
/lib/modules/4.1.15 # cd /sys/bus/iio/devices/
/sys/bus/iio/devices # ls
iio:device0
/sys/bus/iio/devices # cd iio\:device0/
/sys/devices/platform/soc/2000000.aips-bus/2000000.spba-bus/2010000.ecspi/spi_master/spi2/spi2.0/iio:device0 # ls
dev        name       of_node    power      subsystem  uevent
/sys/devices/platform/soc/2000000.aips-bus/2000000.spba-bus/2010000.ecspi/spi_master/spi2/spi2.0/iio:device0 # cat name
icm20608
```