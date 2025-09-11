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
};

static struct ap3216c_dev ap3216cdev;

// 读取AP3216C的N个寄存器值
static int ap3216c_read_regs(struct ap3216c_dev* dev, u8 reg, void* val, int len)
{
    int ret;
    struct i2c_msg msg[2];
    struct i2c_client* client = (struct i2c_client*)dev->private_data;

    // msg[0] - 发送寄存器首地址
    msg[0].addr = client->addr;     // 从机地址
    msg[0].flags = 0;               // 写操作
    msg[0].len = 1;                 // 寄存器地址长度为1字节
    msg[0].buf = &reg;              // 寄存器地址（要发送的数据）

    // msg[1] - 读取寄存器数据
    msg[1].addr = client->addr;
    msg[1].flags = I2C_M_RD;        // 读操作
    msg[1].len = len;               // 数据长度
    msg[1].buf = val;               // 要读取的寄存器长度

    ret = i2c_transfer(client->adapter, msg, 2);
    if (ret == 2) {
        ret = 0;  // 成功
    } else {
        printk("i2c rd failed=%d reg=%06x len=%d\n", ret, reg, len);
        ret = -EIO;
    }
    
    return ret;
}

// 向AP3216C的N个寄存器写入值
static int ap3216c_write_regs(struct ap3216c_dev* dev, u8 reg, void* val, int len)
{
    u8 buf[256];
    struct i2c_msg msg;
    struct i2c_client* client = (struct i2c_client*)dev->private_data;
    int ret;
    
    // 要发送的实际长度是 len + 1(寄存器地址)
    buf[0] = reg;  // 寄存器地址
    memcpy(&buf[1], val, len);  // 寄存器值

    msg.addr = client->addr;     // 从机地址
    msg.flags = 0;               // 写操作
    msg.len = len + 1;           // 数据长度
    msg.buf = buf;               // 数据缓冲区

    ret = i2c_transfer(client->adapter, &msg, 1);
    if (ret == 1) {
        ret = 0;  // 成功
    } else {
        printk("i2c wr failed=%d reg=%06x len=%d\n", ret, reg, len);
        ret = -EIO;
    }
    
    return ret;
}

// 读取ap3216c 1个 寄存器
static int ap3216c_read_reg(struct ap3216c_dev* dev, u8 reg, u8* val)
{
    return ap3216c_read_regs(dev, reg, val, 1);
}

// 写入ap3216c 1个 寄存器
static int ap3216c_write_reg(struct ap3216c_dev* dev, u8 reg, u8 val)
{
    return ap3216c_write_regs(dev, reg, &val, 1);
}

// ap3216c 数据读取
void ap3216c_readdata(struct ap3216c_dev* dev)
{
    u8 data[6];
    
    ap3216c_read_regs(dev, AP3216C_IRDATALOW, data, 6);

    // IR 数据处理（检查有效性）
    if (data[1] & 0x80) {  // 检查 bit 15（高字节的 bit 7）
        dev->ir = 0;       // IR 数据无效，设为 0
    } else {
        dev->ir = ((data[1] & 0x3F) << 8) | data[0];  // 只取低 14 位
    }
    
    // ALS 数据（通常总是有效的）
    dev->als = (data[3] << 8) | data[2];
    
    // PS 数据处理（检查有效性和干扰）
    if (data[5] & 0x80) {  // 检查 bit 15（高字节的 bit 7）
        dev->ps = 0;       // PS 数据无效，设为 0
    } else {
        dev->ps = ((data[5] & 0x3F) << 8) | data[4];  // 只取低 14 位
        
        // 检查红外干扰标志（可选）
        if (data[5] & 0x40) {  // 检查 bit 14（高字节的 bit 6）
            printk("PS data may be affected by IR interference\n");
        }
    }
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
    ap3216c_readdata(dev);

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
    u8 value = 0;

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
