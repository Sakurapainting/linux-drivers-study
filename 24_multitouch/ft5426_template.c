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

#define AP3216C_CNT 1
#define AP3216C_NAME "ft5426"

struct ft5426_dev {
    dev_t devid;            // 设备号
    int major;              // 主设备号
    int minor;              // 次设备号
    struct cdev cdev;       // 字符设备
    struct class* class;    // 类
    struct device* device;  // 设备

    void* private_data;
};

static struct ft5426_dev ft5426dev;

// 读取AP3216C的N个寄存器值
static int ft5426_read_regs(struct ft5426_dev* dev, u8 reg, void* val, int len)
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
static int ft5426_write_regs(struct ft5426_dev* dev, u8 reg, void* val, int len)
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

// 写入ft5426 1个 寄存器
static int ft5426_write_reg(struct ft5426_dev* dev, u8 reg, u8 val)
{
    return ft5426_write_regs(dev, reg, &val, 1);
}

static int ft5426_probe(struct i2c_client* client, const struct i2c_device_id* id){
    int ret = 0;    

    printk("ft5426_probe!\n");

    return ret;
}

static int ft5426_remove(struct i2c_client* client){

    return 0;
}

// 传统匹配表
static struct i2c_device_id ft5426_idtable[] = {
    {"edt,edt-ft5426", 0},
    {}
};

// 设备树匹配表
static struct of_device_id ft5426_of_match[] = {
    {.compatible = "edt,edt-ft5426"},
    {}
};

static struct i2c_driver ft5426_driver = {
    .probe = ft5426_probe,
    .remove = ft5426_remove,
    .driver = {
        .name = "ft5426",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(ft5426_of_match),
    },
    .id_table = ft5426_idtable,
};

static int __init ft5426_init(void)
{
    int ret = 0;

    ret = i2c_add_driver(&ft5426_driver);

    return 0;
}

static void __exit ft5426_exit(void)
{
    i2c_del_driver(&ft5426_driver);
}

module_init(ft5426_init);
module_exit(ft5426_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
