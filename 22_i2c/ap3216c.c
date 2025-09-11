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
    struct device_node* nd; // 设备节点
    int led_gpio;           // led所使用的GPIO编号
};

static struct ap3216c_dev ap3216cdev;

static int ap3216c_open(struct inode* inode, struct file* file)
{
    file->private_data = &ap3216cdev;
    printk("ap3216c_open!\n");
    return 0;
}

static ssize_t ap3216c_read(struct file* file, char __user* buf, size_t cnt, loff_t* offt)
{
    printk("ap3216c_read!\n");
    return 0;
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

    return 0;

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
