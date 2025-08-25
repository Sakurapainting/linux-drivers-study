#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define BEEP_CNT 1
#define BEEP_NAME "beep"

#define BEEPON 1
#define BEEPOFF 0

struct beep_dev {
    struct cdev cdev; // 字符设备结构体
    struct class* class; // 设备类
    struct device* device; // 设备
    struct device_node* nd; // 设备树节点
    dev_t devid;
    int major;
    int minor;
    int beep_gpio;
};

struct beep_dev beep;

static int beep_open(struct inode* inode, struct file* file)
{
    file->private_data = &beep; // 将设备结构体指针存储在文件私有数据中
    return 0;
}

static int beep_release(struct inode* inode, struct file* file)
{
    // struct beep_dev *dev = (struct beep_dev *)file->private_data;
    return 0;
}

static ssize_t beep_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
    int ret = 0;
    u8 databuf[1];
    struct beep_dev* dev = (struct beep_dev*)file->private_data;

    ret = copy_from_user(databuf, buf, count); // 从用户空间复制数据到内核空间
    if (ret < 0) {
        return -EFAULT;
    }

    if (databuf[0] == BEEPON) {
        gpio_set_value(dev->beep_gpio, 0); // 打开蜂鸣器

    } else if (databuf[0] == BEEPOFF) {
        gpio_set_value(dev->beep_gpio, 1); // 关闭蜂鸣器

    } else {
        return -EINVAL; // 无效参数
    }

    return 0;
}

static const struct file_operations beep_fops = {
    .owner = THIS_MODULE,
    .open = beep_open, // 打开设备的函数
    .write = beep_write, // 写入设备的函数
    .release = beep_release, // 释放设备的函数
};

static int __init beep_init(void)
{
    int ret = 0;
    // device id
    beep.major = 0;
    if (beep.major) {
        beep.devid = MKDEV(beep.major, 0);
        register_chrdev_region(beep.devid, BEEP_CNT, BEEP_NAME);
    } else {
        alloc_chrdev_region(&beep.devid, 0, BEEP_CNT, BEEP_NAME);
        beep.major = MAJOR(beep.devid);
        beep.minor = MINOR(beep.devid);
    }
    if (ret < 0) {
        goto fail_devid;
    }
    printk("beep major=%d, minor=%d\n", beep.major, beep.minor);

    // cdev
    beep.cdev.owner = THIS_MODULE;
    cdev_init(&beep.cdev, &beep_fops);
    ret = cdev_add(&beep.cdev, beep.devid, BEEP_CNT);
    if (ret < 0) {
        goto fail_cdev;
    }

    // class
    beep.class = class_create(THIS_MODULE, BEEP_NAME);
    if (IS_ERR(beep.class)) {
        ret = PTR_ERR(beep.class);
        goto fail_class;
    }

    // device
    beep.device = device_create(beep.class, NULL, beep.devid, NULL, BEEP_NAME);
    if (IS_ERR(beep.device)) {
        ret = PTR_ERR(beep.device);
        goto fail_device;
    }

    // node
    beep.nd = of_find_node_by_path("/beep");
    if (!beep.nd) {
        ret = -EINVAL;
        goto fail_findnd;
    }

    // 获取GPIO
    beep.beep_gpio = of_get_named_gpio(beep.nd, "beep-gpios", 0);
    if (beep.beep_gpio < 0) {
        ret = -EINVAL;
        goto fail_findnd;
    }

    // request GPIO
    ret = gpio_request(beep.beep_gpio, "beep-gpio");
    if (ret < 0) {
        goto fail_request;
    }

    // 设置GPIO为输出
    ret = gpio_direction_output(beep.beep_gpio, 1);
    if (ret < 0) {
        goto fail_setoutput;
    }

    // 默认关闭蜂鸣器
    gpio_set_value(beep.beep_gpio, 1);

    return 0;

fail_setoutput:
    gpio_free(beep.beep_gpio); // 释放GPIO
fail_request:
    of_node_put(beep.nd); // 释放设备树节点
fail_findnd:
    device_destroy(beep.class, beep.devid); // 销毁设备
fail_device:
    class_destroy(beep.class);
fail_class:
    cdev_del(&beep.cdev); // 删除字符设备
fail_cdev:
    unregister_chrdev_region(beep.devid, BEEP_CNT);
fail_devid:
    return ret;
}

static void __exit beep_exit(void)
{

    gpio_set_value(beep.beep_gpio, 1); // 确保蜂鸣器关闭
    gpio_free(beep.beep_gpio); // 释放GPIO
    of_node_put(beep.nd); // 释放设备树节点引用

    device_destroy(beep.class, beep.devid); // 销毁设备
    class_destroy(beep.class); // 销毁设备类
    cdev_del(&beep.cdev); // 删除字符设备
    unregister_chrdev_region(beep.devid, BEEP_CNT); // 注销设备号
    printk("beep exit\n");
}

module_init(beep_init);
module_exit(beep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
