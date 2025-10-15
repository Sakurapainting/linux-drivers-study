#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/string.h>
#include <linux/irq.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/spi/spi.h>
#include <linux/delay.h>
#include "icm20608reg.h"

#define ICM20608_CNT     1
#define ICM20608_NAME    "icm20608"

/* 设备结构体 */
struct icm20608_dev {
    int major; 
    int minor; 
    dev_t devid;
    struct cdev cdev;
    struct class *class;
    struct device *device;
    void *private_data;
    int cs_gpio; 
    struct device_node *nd;   
    signed int gyro_x_adc;		/* 陀螺仪X轴原始值 	 */
	signed int gyro_y_adc;		/* 陀螺仪Y轴原始值		*/
	signed int gyro_z_adc;		/* 陀螺仪Z轴原始值 		*/
	signed int accel_x_adc;		/* 加速度计X轴原始值 	*/
	signed int accel_y_adc;		/* 加速度计Y轴原始值	*/
	signed int accel_z_adc;		/* 加速度计Z轴原始值 	*/
	signed int temp_adc;		/* 温度原始值 			*/    
};

static struct icm20608_dev icm20608dev;

#if 0
/* SPI读寄存器 */
static int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{
    int ret = 0;
    unsigned char txdata[len];
    struct spi_message m;
    struct spi_transfer *t;
    struct spi_device *spi = (struct spi_device *)dev->private_data;

    /* 片选拉低 */
    gpio_set_value(dev->cs_gpio, 0);

    /* 构建spi_transfer */
    t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

    /* 第一步：发送要读取的寄存器地址 */
    txdata[0] = reg | 0x80; 
    t->tx_buf = txdata; 
    t->len = 1;

    spi_message_init(&m);  
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);

    /* 第二步，读取数据 */
    txdata[0] = 0xff;       /* 无效的 */
    t->rx_buf = buf;
    t->len = len;

    spi_message_init(&m);  
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);

    kfree(t);
    gpio_set_value(dev->cs_gpio, 1); /* 片选拉高 */
}

/* SPI写寄存器 */
static int icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *buf, int len)
{
    int ret = 0;
    unsigned char txdata[len];
    struct spi_message m;
    struct spi_transfer *t;
    struct spi_device *spi = (struct spi_device *)dev->private_data;

    /* 片选拉低 */
    gpio_set_value(dev->cs_gpio, 0);

    /* 构建spi_transfer */
    t = kzalloc(sizeof(struct spi_transfer), GFP_KERNEL);

    /* 第一步：发送要写的寄存器地址 */
    txdata[0] = reg & ~0x80; 
    t->tx_buf = txdata; 
    t->len = 1;

    spi_message_init(&m);  
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);

    /* 第二步，读取数据 */
    t->tx_buf = buf;
    t->len = len;

    spi_message_init(&m);  
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);

    kfree(t);
    gpio_set_value(dev->cs_gpio, 1); /* 片选拉高 */
    return ret;
}
#endif

/* SPI读寄存器 */
static int icm20608_read_regs(struct icm20608_dev *dev, u8 reg, void *buf, int len)
{
    u8 data = 0;

    struct spi_device *spi = (struct spi_device *)dev->private_data;

    /* 片选拉低 */
    //gpio_set_value(dev->cs_gpio, 0);

    data = reg | 0x80;

    spi_write_then_read(spi, &data, 1, buf, len);

    //spi_write(spi, &data, 1); /* 发送要读取的寄存器地址 */
    //spi_read(spi, buf, len);/* 读取数据 */

    //gpio_set_value(dev->cs_gpio, 1); /* 片选拉高 */
    return 0;
}

/* SPI写寄存器 */
static int icm20608_write_regs(struct icm20608_dev *dev, u8 reg, u8 *buf, int len)
{
    u8 data = 0;
    u8 *txdata;
    struct spi_device *spi = (struct spi_device *)dev->private_data;

    /* 片选拉低 */
    //gpio_set_value(dev->cs_gpio, 0);

    txdata = kzalloc(len + 1, GFP_KERNEL);

    txdata[0] = reg & ~0x80;        /* 要写的寄存器地址 */
    memcpy(&txdata[1], buf, len);   /* 要发送的数据拷贝到txdata里面 */
    spi_write(spi, txdata, len + 1); /* 发送要写的寄存器地址 */

    //spi_write(spi, &data, 1); /* 发送要写的寄存器地址 */
    //spi_write(spi, buf, len); /* 发送要写的寄存器地址 */

    kfree(txdata);
    //gpio_set_value(dev->cs_gpio, 1); /* 片选拉高 */
    return 0;
}


/* ICM20608读取单个寄存器 */
static unsigned char icm20608_read_onereg(struct icm20608_dev *dev, u8 reg)
{
    u8 data = 0;
    icm20608_read_regs(dev, reg, &data, 1);
    return data;
}

/* ICM20608写一个寄存器 */
static void icm20608_write_onereg(struct icm20608_dev *dev, u8 reg, u8 value)
{
    u8 buf = value;
    icm20608_write_regs(dev, reg, &buf, 1);
}

/*
 * @description	: 读取ICM20608的数据，读取原始数据，包括三轴陀螺仪、
 * 				: 三轴加速度计和内部温度。
 * @param - dev	: ICM20608设备
 * @return 		: 无。
 */
void icm20608_readdata(struct icm20608_dev *dev)
{
	unsigned char data[14];
	icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);

	dev->accel_x_adc = (signed short)((data[0] << 8) | data[1]); 
	dev->accel_y_adc = (signed short)((data[2] << 8) | data[3]); 
	dev->accel_z_adc = (signed short)((data[4] << 8) | data[5]); 
	dev->temp_adc    = (signed short)((data[6] << 8) | data[7]); 
	dev->gyro_x_adc  = (signed short)((data[8] << 8) | data[9]); 
	dev->gyro_y_adc  = (signed short)((data[10] << 8) | data[11]);
	dev->gyro_z_adc  = (signed short)((data[12] << 8) | data[13]);
}

/* ICM20608初始化 */
void icm20608_reginit(struct icm20608_dev *dev)
{
    u8 value  = 0;

    icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x80); /* 复位，复位后为0x40,睡眠模式          */  
    mdelay(50);
    icm20608_write_onereg(dev, ICM20_PWR_MGMT_1, 0x01);   /* 关闭睡眠，自动选择时钟                   */
    mdelay(50);
 
    value = icm20608_read_onereg(dev, ICM20_WHO_AM_I);
    printk("ICM20608 ID = %#X\r\n", value);

    value = icm20608_read_onereg(dev, ICM20_PWR_MGMT_1);
    printk("ICM20_PWR_MGMT_1 = %#X\r\n", value);

    icm20608_write_onereg(&icm20608dev, ICM20_SMPLRT_DIV, 0x00); 	/* 输出速率是内部采样率					*/
	icm20608_write_onereg(&icm20608dev, ICM20_GYRO_CONFIG, 0x18); 	/* 陀螺仪±2000dps量程 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_ACCEL_CONFIG, 0x18); 	/* 加速度计±16G量程 					*/
	icm20608_write_onereg(&icm20608dev, ICM20_CONFIG, 0x04); 		/* 陀螺仪低通滤波BW=20Hz 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_ACCEL_CONFIG2, 0x04); /* 加速度计低通滤波BW=21.2Hz 			*/
	icm20608_write_onereg(&icm20608dev, ICM20_PWR_MGMT_2, 0x00); 	/* 打开加速度计和陀螺仪所有轴 				*/
	icm20608_write_onereg(&icm20608dev, ICM20_LP_MODE_CFG, 0x00); 	/* 关闭低功耗 						*/
	icm20608_write_onereg(&icm20608dev, ICM20_FIFO_EN, 0x00);		/* 关闭FIFO	 */
}

static int icm20608_open(struct inode *inode, struct file *filp)
{
	filp->private_data = &icm20608dev; /* 设置私有数据 */
	return 0;
}

ssize_t icm20608_read(struct file *filp, char __user *buf, size_t cnt, loff_t *off)
{
    signed int data[7];
	long err = 0;
	struct icm20608_dev *dev = (struct icm20608_dev *)filp->private_data;

	icm20608_readdata(dev);
	data[0] = dev->gyro_x_adc;
	data[1] = dev->gyro_y_adc;
	data[2] = dev->gyro_z_adc;
	data[3] = dev->accel_x_adc;
	data[4] = dev->accel_y_adc;
	data[5] = dev->accel_z_adc;
	data[6] = dev->temp_adc;
	err = copy_to_user(buf, data, sizeof(data));
    return 0;
}

static int icm20608_release(struct inode *inode, struct file *filp)
{
    return 0;
}

static const struct file_operations icm20608_fops = {
    .owner = THIS_MODULE,
	.open	= icm20608_open,
    .read   = icm20608_read,
	.release= icm20608_release,
};

static int icm20608_probe(struct spi_device *spi)
{
    int ret = 0;

    printk("icm20608_probe\r\n");

    /* 搭建字符设备驱动框架，在/dev/ */
    /* 2，注册字符设备 */
    icm20608dev.major = 0;   /* 由系统分配主设备号 */
    if(icm20608dev.major){    /* 给定主设备号 */
        icm20608dev.devid = MKDEV(icm20608dev.major, 0);
        ret = register_chrdev_region(icm20608dev.devid, ICM20608_CNT, ICM20608_NAME);
    } else {                /* 没有给定主设备号 */
        ret = alloc_chrdev_region(&icm20608dev.devid, 0, ICM20608_CNT, ICM20608_NAME);
        icm20608dev.major = MAJOR(icm20608dev.devid);
        icm20608dev.minor = MINOR(icm20608dev.devid);
    }
    if(ret < 0) {
        printk("icm20608 chrdev_region err!\r\n");
        goto fail_devid;
    }
    printk("icm20608 major=%d, minor=%d\r\n", icm20608dev.major, icm20608dev.minor);

    /* 3,注册字符设备 */
    icm20608dev.cdev.owner = THIS_MODULE;
    cdev_init(&icm20608dev.cdev, &icm20608_fops);
    ret = cdev_add(&icm20608dev.cdev, icm20608dev.devid, ICM20608_CNT);
    if(ret < 0) {
        goto fail_cdev;
    }

    /* 4,自动创建设备节点 */
    icm20608dev.class = class_create(THIS_MODULE, ICM20608_NAME);
	if (IS_ERR(icm20608dev.class)) {
        ret = PTR_ERR(icm20608dev.class);
		goto fail_class;
    }

    icm20608dev.device = device_create(icm20608dev.class, NULL,
			     icm20608dev.devid, NULL, ICM20608_NAME);
	if (IS_ERR(icm20608dev.device)) {
        ret = PTR_ERR(icm20608dev.device);
        goto fail_device;
    }

#if 0
    /* 获取片选引脚 */
    icm20608dev.nd = of_get_parent(spi->dev.of_node);
    icm20608dev.cs_gpio = of_get_named_gpio(icm20608dev.nd, "cs-gpio", 0);
    if(icm20608dev.cs_gpio < 0) {
        printk("can't get cs-gpio\r\n");
        goto fail_gpio;
    }

    ret = gpio_request(icm20608dev.cs_gpio, "cs");
    if (ret < 0)
    {
        printk("cs_gpio request failed!\r\n");
    }

    ret = gpio_direction_output(icm20608dev.cs_gpio, 1); /* 默认高电平 */
#endif

    /* 初始化spi_device */
    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    /* 设置icm20608dev的私有数据 */
    icm20608dev.private_data = spi;   

    /* 初始化icm20608 */
    icm20608_reginit(&icm20608dev);

    return 0;

fail_gpio:
fail_device:
    class_destroy(icm20608dev.class);
fail_class:
    cdev_del(&icm20608dev.cdev);
fail_cdev:
    unregister_chrdev_region(icm20608dev.devid, ICM20608_CNT);
fail_devid:
	return ret; 
}

static int icm20608_remove(struct spi_device *spi)
{
    int ret = 0;

    /* 1,删除字符设备 */
    cdev_del(&icm20608dev.cdev);

    /* 2,注销设备号 */
    unregister_chrdev_region(icm20608dev.devid, ICM20608_CNT);

    /* 3,摧毁设备 */
    device_destroy(icm20608dev.class, icm20608dev.devid);
  
    /* 4,摧毁类 */
    class_destroy(icm20608dev.class);

    /* 5、释放片选 */
    gpio_free(icm20608dev.cs_gpio);
    return ret;
}

/* 传统匹配 */
struct spi_device_id icm20608_id[] = {
    {"alientek,icm20608", 0},
    {}

};

/* 设备树匹配 */
static const struct of_device_id icm20608_of_match[] = {
	{ .compatible = "alientek,icm20608"},
	{ }
};

/* spi_driver */
struct spi_driver icm20608_driver = {
    .probe		= icm20608_probe,
	.remove		= icm20608_remove,
	.driver = {
		.name	= "icm20608",
		.owner	= THIS_MODULE,
		.of_match_table = icm20608_of_match,
	},
    .id_table = icm20608_id,
};

/* 驱动入口函数 */
static int __init icm20608_init(void)
{
    int ret = 0;

    ret = spi_register_driver(&icm20608_driver);

    return ret;

}

/* 驱动出口函数 */
static void __exit icm20608_exit(void)
{
    spi_unregister_driver(&icm20608_driver);
}

module_init(icm20608_init);
module_exit(icm20608_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
