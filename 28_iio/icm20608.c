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
#define ICM20608_TEMP_OFFSET	     0
#define ICM20608_TEMP_SCALE		     326800000

/*
 * icm20608陀螺仪分辨率，对应250、500、1000、2000，计算方法：
 * 以正负250度量程为例，500/2^16=0.007629，扩大1000000倍，就是7629
 */
static const int gyro_scale_icm20608[] = {7629, 15258, 30517, 61035};

/* 
 * icm20608加速度计分辨率，对应2、4、8、16 计算方法：
 * 以正负2g量程为例，4/2^16=0.000061035，扩大1000000000倍，就是61035
 */
static const int accel_scale_icm20608[] = {61035, 122070, 244140, 488281};

/* 
 * ICM20608的扫描元素，3轴加速度计、
 * 3轴陀螺仪、1路温度传感器，1路时间戳 
 */
enum inv_icm20608_scan {
	INV_ICM20608_SCAN_ACCL_X,
	INV_ICM20608_SCAN_ACCL_Y,
	INV_ICM20608_SCAN_ACCL_Z,
	INV_ICM20608_SCAN_TEMP,
	INV_ICM20608_SCAN_GYRO_X,
	INV_ICM20608_SCAN_GYRO_Y,
	INV_ICM20608_SCAN_GYRO_Z,
	INV_ICM20608_SCAN_TIMESTAMP,
};

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
    /* 温度 */
    {
        .type = IIO_TEMP,
		.info_mask_separate = BIT(IIO_CHAN_INFO_RAW)
				| BIT(IIO_CHAN_INFO_OFFSET)
				| BIT(IIO_CHAN_INFO_SCALE),
		.scan_index = INV_ICM20608_SCAN_TEMP,
		.scan_type = {
				.sign = 's',
				.realbits = 16,
				.storagebits = 16,
				.shift = 0,
				.endianness = IIO_BE,
			     },
    },

    /* 加速度XYZ三个通道 */
    ICM20608_CHAN(IIO_ACCEL, IIO_MOD_X, INV_ICM20608_SCAN_ACCL_X),
    ICM20608_CHAN(IIO_ACCEL, IIO_MOD_Y, INV_ICM20608_SCAN_ACCL_Y),
    ICM20608_CHAN(IIO_ACCEL, IIO_MOD_Z, INV_ICM20608_SCAN_ACCL_Z),

    /* 陀螺仪XYZ三个通道 */
    ICM20608_CHAN(IIO_ANGL_VEL, IIO_MOD_X, INV_ICM20608_SCAN_GYRO_X),
    ICM20608_CHAN(IIO_ANGL_VEL, IIO_MOD_Y, INV_ICM20608_SCAN_GYRO_Y),
    ICM20608_CHAN(IIO_ANGL_VEL, IIO_MOD_Z, INV_ICM20608_SCAN_GYRO_Z),
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

static int icm20608_sensor_show(struct icm20608_dev *dev, int reg,
				   int axis, int *val)
{
	int ind, result;
	__be16 d;

	ind = (axis - IIO_MOD_X) * 2;
	result = regmap_bulk_read(dev->regmap, reg + ind, (u8 *)&d, 2);
	if (result)
		return -EINVAL;
	*val = (short)be16_to_cpup(&d);

	return IIO_VAL_INT;
}

static int icm20608_sensor_set(struct icm20608_dev *dev, int reg,
				int axis, int val)
{
	int ind, result;
	__be16 d = cpu_to_be16(val);

	ind = (axis - IIO_MOD_X) * 2;
	result = regmap_bulk_write(dev->regmap, reg + ind, (u8 *)&d, 2);
	if (result)
		return -EINVAL;

	return 0;
}

static int icm20608_read_channel_data(struct iio_dev *indio_dev,
					 struct iio_chan_spec const *chan,
					 int *val)
{
	struct icm20608_dev *dev = iio_priv(indio_dev);
	int ret = 0;

	switch (chan->type) {
        case IIO_ANGL_VEL:	/* 读取陀螺仪数据 */
            ret = icm20608_sensor_show(dev, ICM20_GYRO_XOUT_H, chan->channel2, val);  /* channel2为X、Y、Z轴 */
            break;
        case IIO_ACCEL:		/* 读取加速度计数据 */
            ret = icm20608_sensor_show(dev, ICM20_ACCEL_XOUT_H, chan->channel2, val); /* channel2为X、Y、Z轴 */
            break;
        case IIO_TEMP:		/* 读取温度 */
            ret = icm20608_sensor_show(dev, ICM20_TEMP_OUT_H, IIO_MOD_X, val);  
            break;
        default:
            ret = -EINVAL;
            break;
        }
	return ret;
}

static int icm20608_write_gyro_scale(struct icm20608_dev *dev, int val)
{
	int result, i;
	u8 d;

	for (i = 0; i < ARRAY_SIZE(gyro_scale_icm20608); ++i) {
		if (gyro_scale_icm20608[i] == val) {
			d = (i << 3);
			result = regmap_write(dev->regmap, ICM20_GYRO_CONFIG, d);
			if (result)
				return result;
			return 0;
		}
	}
	return -EINVAL;
}

static int icm20608_write_accel_scale(struct icm20608_dev *dev, int val)
{
	int result, i;
	u8 d;

	for (i = 0; i < ARRAY_SIZE(accel_scale_icm20608); ++i) {
		if (accel_scale_icm20608[i] == val) {
			d = (i << 3);
			result = regmap_write(dev->regmap, ICM20_ACCEL_CONFIG, d);
			if (result)
				return result;
			return 0;
		}
	}
	return -EINVAL;
}

static int icm20608_read_raw(struct iio_dev *indio_dev,
            struct iio_chan_spec const *chan,
            int *val,
            int *val2,
            long mask)
{
    int ret = 0;
    unsigned char regdata = 0;
    struct icm20608_dev* dev = iio_priv(indio_dev);

    /* 区分读取的数据类型，是RAW/OFFSET/SCALE */
    switch(mask) {
        case IIO_CHAN_INFO_RAW:
            mutex_lock(&dev->lock);								/* 上锁 			*/
		    ret = icm20608_read_channel_data(indio_dev, chan, val); 	/* 读取通道值 */
		    mutex_unlock(&dev->lock);							/* 释放锁 			*/
            return ret;
        case IIO_CHAN_INFO_SCALE:
            switch (chan->type) {
                case IIO_ANGL_VEL:
                    mutex_lock(&dev->lock);
                    // bit4:3陀螺仪量程选择
                    regdata = (icm20608_read_reg(dev, ICM20_GYRO_CONFIG) & 0X18) >> 3;
                    *val  = 0;
                    *val2 = gyro_scale_icm20608[regdata];
                    mutex_unlock(&dev->lock);
                    return IIO_VAL_INT_PLUS_MICRO;	/* 值为val+val2/1000000 */
                case IIO_ACCEL:
                    mutex_lock(&dev->lock);
                    regdata = (icm20608_read_reg(dev, ICM20_ACCEL_CONFIG) & 0X18) >> 3;
                    *val = 0;
                    *val2 = accel_scale_icm20608[regdata];
                    mutex_unlock(&dev->lock);
                    return IIO_VAL_INT_PLUS_NANO;/* 值为val+val2/1000000000 */
                case IIO_TEMP:					
                    *val = ICM20608_TEMP_SCALE/ 1000000;
                    *val2 = ICM20608_TEMP_SCALE % 1000000;
                    return IIO_VAL_INT_PLUS_MICRO;	/* 值为val+val2/1000000 */
                default:
                    return -EINVAL;
		    }
            return ret;
        case IIO_CHAN_INFO_OFFSET:
           	switch (chan->type) {
                case IIO_TEMP:
                    *val = ICM20608_TEMP_OFFSET;
                    return IIO_VAL_INT;
                default:
                    return -EINVAL;
		    }
            return ret;
        case IIO_CHAN_INFO_CALIBBIAS:
            switch (chan->type) {
                case IIO_ANGL_VEL:		/* 陀螺仪的校准值 */
                    mutex_lock(&dev->lock);
                    ret = icm20608_sensor_show(dev, ICM20_XG_OFFS_USRH, chan->channel2, val);
                    mutex_unlock(&dev->lock);
                    return ret;
                case IIO_ACCEL:			/* 加速度计的校准值 */
                    mutex_lock(&dev->lock);	
                    ret = icm20608_sensor_show(dev, ICM20_XA_OFFSET_H, chan->channel2, val);
                    mutex_unlock(&dev->lock);
                    return ret;
                default:
                    return -EINVAL;
		    }
            return ret;
        default:
            return -EINVAL;
    }

    return 0;
}

static int icm20608_write_raw(struct iio_dev *indio_dev,
             struct iio_chan_spec const *chan,
             int val,
             int val2,
             long mask)
{
    int ret = 0;
	struct icm20608_dev *dev = iio_priv(indio_dev);

	switch (mask) {
	case IIO_CHAN_INFO_SCALE:	/* 设置陀螺仪和加速度计的分辨率 */
		switch (chan->type) {
		case IIO_ANGL_VEL:		/* 设置陀螺仪 */
			mutex_lock(&dev->lock);
			ret = icm20608_write_gyro_scale(dev, val2);
			mutex_unlock(&dev->lock);
			break;
		case IIO_ACCEL:			/* 设置加速度计 */
			mutex_lock(&dev->lock);
			ret = icm20608_write_accel_scale(dev, val2);
			mutex_unlock(&dev->lock);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	case IIO_CHAN_INFO_CALIBBIAS:	/* 设置陀螺仪和加速度计的校准值*/
		switch (chan->type) {
		case IIO_ANGL_VEL:		/* 设置陀螺仪校准值 */
			mutex_lock(&dev->lock);
			ret = icm20608_sensor_set(dev, ICM20_XG_OFFS_USRH,
									    chan->channel2, val);
			mutex_unlock(&dev->lock);
			break;
		case IIO_ACCEL:			/* 加速度计校准值 */
			mutex_lock(&dev->lock);
			ret = icm20608_sensor_set(dev, ICM20_XA_OFFSET_H,
							             chan->channel2, val);
			mutex_unlock(&dev->lock);
			break;
		default:
			ret = -EINVAL;
			break;
		}
		break;
	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int icm20608_write_raw_get_fmt(struct iio_dev *indio_dev,
             struct iio_chan_spec const *chan,
             long mask)
{
    switch (mask) {
	case IIO_CHAN_INFO_SCALE:
		switch (chan->type) {
		case IIO_ANGL_VEL:		/* 用户空间写的陀螺仪分辨率数据要乘以1000000 */
			return IIO_VAL_INT_PLUS_MICRO;
		default:				/* 用户空间写的加速度计分辨率数据要乘以1000000000 */
			return IIO_VAL_INT_PLUS_NANO;
		}
	default:
		return IIO_VAL_INT_PLUS_MICRO;
	}
	return -EINVAL;
}

/* iio_info */
static const struct iio_info icm20608_info = {
	.driver_module  = THIS_MODULE,
	.read_raw		= icm20608_read_raw,
	.write_raw		= icm20608_write_raw,
    .write_raw_get_fmt = icm20608_write_raw_get_fmt,
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
