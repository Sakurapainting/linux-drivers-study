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
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regmap.h>

#include "ap3216creg.h"

#define AP3216C_CNT 1
#define AP3216C_NAME "ap3216c"

struct ap3216c_dev {
    dev_t devid;            // 设备号
    int major;              // 主设备号
    int minor;              // 次设备号
    struct cdev cdev;       // 字符设备
    struct class* class;    // 类
    struct device* device;  // 设备

    void* private_data;
    u16 ir, als, ps;        // 传感器数据（应该是16位）

    struct regmap *regmap;
    struct regmap_config regmap_config;
};

static struct ap3216c_dev ap3216cdev;

// 读取ap3216c 1个 寄存器
static int ap3216c_read_reg(struct ap3216c_dev* dev, u8 reg, unsigned int* val)
{
    return regmap_read(dev->regmap, reg, val);
}

// 写入ap3216c 1个 寄存器
static int ap3216c_write_reg(struct ap3216c_dev* dev, u8 reg, u8 val)
{
    return regmap_write(dev->regmap, reg, val);
}

// ap3216c 数据读取
static int ap3216c_readdata(struct ap3216c_dev* dev)
{
    unsigned char i = 0;
    unsigned int buf[6];
    int ret = 0;
    
    /* 循环读取所有传感器数据 */
    for(i = 0; i < 6; i++)
    {
        ret = ap3216c_read_reg(dev, AP3216C_IRDATALOW + i, &buf[i]);
        if (ret < 0) {
            printk("Failed to read register 0x%02x\n", AP3216C_IRDATALOW + i);
            return ret;
        }
    }

    /* IR数据处理 */
    if(buf[0] & 0X80) {      /* IR_OF 位为 1,则数据无效 */
        dev->ir = 0;
    } else {                 /* 读取 IR 传感器的数据 */
        dev->ir = ((unsigned short)buf[1] << 2) | (buf[0] & 0X03);
    }

    /* ALS 数据 */
    dev->als = ((unsigned short)buf[3] << 8) | buf[2];

    /* PS数据处理 */
    if(buf[4] & 0x40) {      /* IR_OF 位为 1,则数据无效 */
        dev->ps = 0;
    } else {                 /* 读取 PS 传感器的数据 */
        dev->ps = ((unsigned short)(buf[5] & 0X3F) << 4) | (buf[4] & 0X0F);
    }

    return 0;
}

static int ap3216c_open(struct inode* inode, struct file* file)
{
    file->private_data = &ap3216cdev;
    printk("ap3216c_open!\n");
    return 0;
}

static ssize_t ap3216c_read(struct file* file, char __user* buf, size_t cnt, loff_t* offt)
{
    u16 data[3]; // 用于存储 IR, ALS, PS 数据
    struct ap3216c_dev* dev = (struct ap3216c_dev*)file->private_data;
    int ret;
    
    printk("ap3216c_read!\n");

    // 检查用户缓冲区大小，避免返回值无法容纳
    if (cnt < sizeof(data)) {
        return -EINVAL;
    }

    // 读取传感器数据
    ret = ap3216c_readdata(dev);
    if (ret < 0) {
        return ret;
    }

    data[0] = dev->ir;
    data[1] = dev->als;
    data[2] = dev->ps;

    // 将数据复制到用户空间
    ret = copy_to_user(buf, data, sizeof(data));
    if (ret) {
        printk("copy_to_user failed!\n");
        return -EFAULT;
    }

    return sizeof(data);  // 返回实际传输的字节数
}

static int ap3216c_release(struct inode* inode, struct file* file)
{
    // struct ap3216c_dev* dev = (struct ap3216c_dev*)file->private_data;
    printk("ap3216c_release!\n");
    return 0;
}

static const struct file_operations ap3216c_fops = {
    .owner = THIS_MODULE,
    .open = ap3216c_open,
    .read = ap3216c_read,
    .release = ap3216c_release,
};

static int ap3216c_probe(struct i2c_client* client, const struct i2c_device_id* id){
    int ret = 0;    
    unsigned int value = 0;

    printk("ap3216c_probe!\n");

    // device id
    ap3216cdev.major = 0;
    if (ap3216cdev.major) {
        ap3216cdev.devid = MKDEV(ap3216cdev.major, 0);
        register_chrdev_region(ap3216cdev.devid, AP3216C_CNT, AP3216C_NAME);
    } else {
        alloc_chrdev_region(&ap3216cdev.devid, 0, AP3216C_CNT, AP3216C_NAME);
        ap3216cdev.major = MAJOR(ap3216cdev.devid);
        ap3216cdev.minor = MINOR(ap3216cdev.devid);
    }
    if (ret < 0) {
        goto fail_devid;
    }
    printk("ap3216cdev major=%d, minor=%d\n", ap3216cdev.major, ap3216cdev.minor);

    // cdev
    ap3216cdev.cdev.owner = THIS_MODULE;
    cdev_init(&ap3216cdev.cdev, &ap3216c_fops);
    ret = cdev_add(&ap3216cdev.cdev, ap3216cdev.devid, AP3216C_CNT);
    if (ret < 0) {
        goto fail_cdev;
    }

    // class
    ap3216cdev.class = class_create(THIS_MODULE, AP3216C_NAME);
    if (IS_ERR(ap3216cdev.class)) {
        ret = PTR_ERR(ap3216cdev.class);
        goto fail_class;
    }

    // device
    ap3216cdev.device = device_create(ap3216cdev.class, NULL, ap3216cdev.devid, NULL, AP3216C_NAME);
    if (IS_ERR(ap3216cdev.device)) {
        ret = PTR_ERR(ap3216cdev.device);
        goto fail_device;
    }

    /* regmap申请和初始化初始化 */
    ap3216cdev.regmap_config.reg_bits = 8;
    ap3216cdev.regmap_config.val_bits = 8;
    ap3216cdev.regmap = regmap_init_i2c(client, &ap3216cdev.regmap_config);
    if (IS_ERR(ap3216cdev.regmap)) {
        ret = PTR_ERR(ap3216cdev.regmap);
        printk("regmap_init_i2c failed: %d\n", ret);
        goto fail_regmap;
    }

    ap3216cdev.private_data = client;

    // 初始化 AP3216C 硬件
    ret = ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, 0x04);
    if (ret < 0) {
        printk("AP3216C reset failed!\n");
        goto fail_init;
    }
    
    mdelay(50); /* AP3216C 复位最少 10ms */
    
    ret = ap3216c_write_reg(&ap3216cdev, AP3216C_SYSTEMCONG, 0X03);
    if (ret < 0) {
        printk("AP3216C init failed!\n");
        goto fail_init;
    }
    
    printk("AP3216C initialization successful!\n");

    ret = ap3216c_read_reg(&ap3216cdev, AP3216C_SYSTEMCONG, &value);
    if (ret < 0) {
        printk("AP3216C read SYSTEMCONG failed!\n");
        goto fail_init;
    }
    printk("AP3216C SYSTEMCONG=%#x\n", value);

    return 0;

fail_init:
    regmap_exit(ap3216cdev.regmap);
fail_regmap:
    device_destroy(ap3216cdev.class, ap3216cdev.devid);
fail_device:
    class_destroy(ap3216cdev.class);
fail_class:
    cdev_del(&ap3216cdev.cdev); // 删除字符设备
fail_cdev:
    unregister_chrdev_region(ap3216cdev.devid, AP3216C_CNT);
fail_devid:
    return ret;
}

static int ap3216c_remove(struct i2c_client* client){
    regmap_exit(ap3216cdev.regmap);
    cdev_del(&ap3216cdev.cdev);
    unregister_chrdev_region(ap3216cdev.devid, AP3216C_CNT);
    device_destroy(ap3216cdev.class, ap3216cdev.devid);
    class_destroy(ap3216cdev.class);
    return 0;
}

// 传统匹配表
static struct i2c_device_id ap3216c_idtable[] = {
    {"lsc,ap3216c", 0},
    {}
};

// 设备树匹配表
static struct of_device_id ap3216c_of_match[] = {
    {.compatible = "lsc,ap3216c"},
    {}
};

static struct i2c_driver ap3216c_driver = {
    .probe = ap3216c_probe,
    .remove = ap3216c_remove,
    .driver = {
        .name = "ap3216c",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(ap3216c_of_match),
    },
    .id_table = ap3216c_idtable,
};

static int __init ap3216c_init(void)
{
    int ret = 0;

    ret = i2c_add_driver(&ap3216c_driver);

    return 0;
}

static void __exit ap3216c_exit(void)
{
    i2c_del_driver(&ap3216c_driver);
}

module_init(ap3216c_init);
module_exit(ap3216c_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
