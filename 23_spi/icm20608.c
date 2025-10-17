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
#include "icm20608reg.h"

#define ICM20608_CNT    1
#define ICM20608_NAME   "icm20608"

struct icm20608_dev {
    dev_t devid;            // 设备号
    int major;              // 主设备号
    int minor;              // 次设备号
    struct cdev cdev;       // 字符设备
    struct class* class;    // 类
    struct device* device;  // 设备
    struct device_node* nd; // 设备节点

    void* private_data;

    int cs_gpio;            // 片选GPIO

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

// spi 读寄存器
static int icm20608_read_regs(struct icm20608_dev* dev, u8 reg, void* buf, int len)
{
    int ret = 0;
    u8 txdata[len];
    struct spi_message m;
    struct spi_transfer* t;
    struct spi_device* spi = (struct spi_device*)dev->private_data;

    // 片选拉低
    gpio_set_value(dev->cs_gpio, 0);

    // 构建spi_transfer
    t = kzalloc(2 * sizeof(struct spi_transfer), GFP_KERNEL);

    // 1. 发送寄存器地址
    txdata[0] = reg | 0x80; // 读操作，最高位置1
    t[0].tx_buf = txdata;
    t[0].len = 1;

    spi_message_init(&m);
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);
    if(ret < 0) {
        printk("icm20608 read regs failed at send reg!\n");
        goto fail_sync;
    }

    // 2. 读取数据
    txdata[0] = 0xFF;       // 发送无意义数据
    t[0].rx_buf = buf;
    t[0].len = len;

    spi_message_init(&m);
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);
    if(ret < 0) {
        printk("icm20608 read regs failed at read data!\n");
        goto fail_sync;
    }

    kfree(t);

    // 片选拉高
    gpio_set_value(dev->cs_gpio, 1);

    return 0;
fail_sync:
    kfree(t);
    gpio_set_value(dev->cs_gpio, 1);
    return ret;
}

// spi 写寄存器
static int icm20608_write_regs(struct icm20608_dev* dev, u8 reg, void* buf, int len)
{
    int ret = 0;
    u8 txdata[len];
    struct spi_message m;
    struct spi_transfer* t;
    struct spi_device* spi = (struct spi_device*)dev->private_data;

    // 片选拉低
    gpio_set_value(dev->cs_gpio, 0);

    // 构建spi_transfer
    t = kzalloc(2 * sizeof(struct spi_transfer), GFP_KERNEL);

    // 1. 发送寄存器地址
    txdata[0] = reg & ~0x80;
    t[0].tx_buf = txdata;
    t[0].len = 1;

    spi_message_init(&m);
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);
    if(ret < 0) {
        printk("icm20608 write regs failed at send reg!\n");
        goto fail_sync;
    }

    // 2. 发送数据
    t[0].tx_buf = buf;
    t[0].len = len;

    spi_message_init(&m);
    spi_message_add_tail(t, &m);
    ret = spi_sync(spi, &m);
    if(ret < 0) {
        printk("icm20608 write regs failed at read data!\n");
        goto fail_sync;
    }

    kfree(t);

    // 片选拉高
    gpio_set_value(dev->cs_gpio, 1);

    return 0;

fail_sync:
    kfree(t);
    gpio_set_value(dev->cs_gpio, 1);
    return ret;
}

#endif

static int icm20608_read_regs(struct icm20608_dev* dev, u8 reg, void* buf, int len)
{
    int ret = 0;
    u8 data = 0;
    struct spi_device* spi = (struct spi_device*)dev->private_data;

    // 片选拉低
    gpio_set_value(dev->cs_gpio, 0);

    data = reg | 0x80;          // 读操作，最高位置1
    ret = spi_write(spi, &data, 1);   // 发送寄存器地址
    if (ret < 0) {
        printk("icm20608 spi_write reg failed!\n");
        goto fail;
    }

    ret = spi_read(spi, buf, len);    // 读取数据
    if (ret < 0) {
        printk("icm20608 spi_read data failed!\n");
        goto fail;
    }

    // 片选拉高
    gpio_set_value(dev->cs_gpio, 1);
    return 0;

fail:
    // 片选拉高
    gpio_set_value(dev->cs_gpio, 1);
    return ret;
}

static int icm20608_write_regs(struct icm20608_dev* dev, u8 reg, void* buf, int len)
{
    int ret = 0;
    u8 data = 0;
    struct spi_device* spi = (struct spi_device*)dev->private_data;

    // 片选拉低
    gpio_set_value(dev->cs_gpio, 0);

    data = reg & ~0x80;         // 写操作，最高位置0
    ret = spi_write(spi, &data, 1);   // 发送寄存器地址
    if (ret < 0) {
        printk("icm20608 spi_write reg failed!\n");
        goto fail;
    }

    ret = spi_write(spi, buf, len);   // 发送数据
    if (ret < 0) {
        printk("icm20608 spi_write data failed!\n");
        goto fail;
    }

    // 片选拉高
    gpio_set_value(dev->cs_gpio, 1);
    return 0;

fail:
    // 片选拉高
    gpio_set_value(dev->cs_gpio, 1);
    return ret;
}

// icm20608 读取单个寄存器
static u8 icm20608_read_reg(struct icm20608_dev* dev, u8 reg)
{
    u8 val = 0;
    int ret = 0;
    
    ret = icm20608_read_regs(dev, reg, &val, 1);
    if (ret < 0) {
        printk("icm20608_read_reg failed, reg=0x%x\n", reg);
        return 0;  // 返回默认值
    }
    return val;
}

// icm20608 写单个寄存器
static int icm20608_write_reg(struct icm20608_dev* dev, u8 reg, u8 val)
{
    return icm20608_write_regs(dev, reg, &val, 1);
}

int icm20608_readdata(struct icm20608_dev *dev)
{
	unsigned char data[14] = { 0 };
	int ret = 0;
	
	ret = icm20608_read_regs(dev, ICM20_ACCEL_XOUT_H, data, 14);
	if (ret < 0) {
		printk("icm20608_read_regs failed!\n");
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

static int icm20608_open(struct inode* inode, struct file* file)
{
	file->private_data = &icm20608dev; /* 设置私有数据 */
	return 0;
}

ssize_t icm20608_read(struct file* file, char __user* buf, size_t size, loff_t* offset)
{
	signed int data[7];
	long err = 0;
	int ret = 0;
	struct icm20608_dev *dev = (struct icm20608_dev *)file->private_data;

	ret = icm20608_readdata(dev);
	if (ret < 0) {
		printk("icm20608_readdata failed!\n");
		return ret;  // 返回错误码
	}

	data[0] = dev->gyro_x_adc;
	data[1] = dev->gyro_y_adc;
	data[2] = dev->gyro_z_adc;
	data[3] = dev->accel_x_adc;
	data[4] = dev->accel_y_adc;
	data[5] = dev->accel_z_adc;
	data[6] = dev->temp_adc;
	
	err = copy_to_user(buf, data, sizeof(data));
	if (err) {
		printk("copy_to_user failed!\n");
		return -EFAULT;
	}
	
	return sizeof(data);  // 返回实际读取的字节数
}

static int icm20608_release(struct inode* inode, struct file* file)
{
    return 0;
}

static const struct file_operations icm20608_fops = {
    .owner = THIS_MODULE,
    .open = icm20608_open,
    .read = icm20608_read,
    .release = icm20608_release,
};


static int icm20608_probe(struct spi_device* spi){
    int ret = 0;
    printk("icm20608_probe!\n");

    // 注册字符设备
    icm20608dev.major = 0;          // 由系统分配主设备号
    if (icm20608dev.major) {        // 给定主设备号
        icm20608dev.devid = MKDEV(icm20608dev.major, 0);
        register_chrdev_region(icm20608dev.devid, ICM20608_CNT, ICM20608_NAME);
    } else {                        // 由系统分配主设备号
        alloc_chrdev_region(&icm20608dev.devid, 0, ICM20608_CNT, ICM20608_NAME);
        icm20608dev.major = MAJOR(icm20608dev.devid);
        icm20608dev.minor = MINOR(icm20608dev.devid);
    }

    if(ret < 0) {
        printk("icm20608 chrdev_region failed!\n");
        goto fail_devid;
    }
    printk("icm20608dev major=%d, minor=%d\n", icm20608dev.major, icm20608dev.minor);

    // cdev 字符设备操作集
    icm20608dev.cdev.owner = THIS_MODULE;
    cdev_init(&icm20608dev.cdev, &icm20608_fops);
    ret = cdev_add(&icm20608dev.cdev, icm20608dev.devid, ICM20608_CNT);
    if(ret < 0) {
        printk("icm20608 cdev_add failed!\n");
        goto fail_cdev;
    }

    // class
    icm20608dev.class = class_create(THIS_MODULE, ICM20608_NAME);
    if (IS_ERR(icm20608dev.class)) {
        ret = PTR_ERR(icm20608dev.class);
        printk("icm20608 class_create failed!\n");
        goto fail_class;
    }

    // device
    icm20608dev.device = device_create(icm20608dev.class, NULL, icm20608dev.devid, NULL, ICM20608_NAME);
    if (IS_ERR(icm20608dev.device)) {
        ret = PTR_ERR(icm20608dev.device);
        printk("icm20608 device_create failed!\n");
        goto fail_device;
    }

    // 获取片选引脚
    icm20608dev.nd = of_get_parent(spi->dev.of_node); // 获取设备节点
    if (icm20608dev.nd == NULL) {
        printk("icm20608 get device node failed!\n");
        ret = -EINVAL;
        goto fail_nd;
    }

    icm20608dev.cs_gpio = of_get_named_gpio(icm20608dev.nd, "cs-gpio", 0);
    if (icm20608dev.cs_gpio < 0) {
        printk("icm20608 get cs-gpio failed!\n");
        ret = -EINVAL;
        goto fail_csgpio;
    }

    ret = gpio_request(icm20608dev.cs_gpio, "icm20608 cs-gpio");
    if (ret) {
        printk("icm20608 cs-gpio request failed!\n");
        goto fail_request;
    }

    ret = gpio_direction_output(icm20608dev.cs_gpio, 1); // 输出，高电平
    if(ret < 0) {
        printk("icm20608 cs-gpio direction failed!\n");
        goto fail_setoutput;
    }

    // 初始化spi_device
    spi->mode = SPI_MODE_0;
    spi_setup(spi);

    // 设置icm20608dev私有数据
    icm20608dev.private_data = spi;

    // 初始化icm20608硬件
    ret = icm20608_init_hw(&icm20608dev);
    if (ret < 0) {
        printk("icm20608 hardware init failed!\n");
        goto fail_setoutput;
    }

    return 0;

fail_setoutput:
    gpio_free(icm20608dev.cs_gpio);
fail_request:
    of_node_put(icm20608dev.nd);
fail_csgpio:
fail_nd:
    device_destroy(icm20608dev.class, icm20608dev.devid);
fail_device:
    class_destroy(icm20608dev.class);
fail_class:
    cdev_del(&icm20608dev.cdev);
fail_cdev:
    unregister_chrdev_region(icm20608dev.devid, ICM20608_CNT);
fail_devid:
    return ret;
}

static int icm20608_remove(struct spi_device* spi){
    int ret = 0;

    gpio_free(icm20608dev.cs_gpio);
    of_node_put(icm20608dev.nd);
    device_destroy(icm20608dev.class, icm20608dev.devid);
    class_destroy(icm20608dev.class);
    cdev_del(&icm20608dev.cdev);
    unregister_chrdev_region(icm20608dev.devid, ICM20608_CNT);
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
