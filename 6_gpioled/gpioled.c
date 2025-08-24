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

#define GPIOLED_CNT 1
#define GPIOLED_NAME "gpioled"

#define LEDON 1
#define LEDOFF 0

struct gpioled_dev {
    struct cdev cdev; // 字符设备结构体
    struct class* class; // 设备类
    struct device* device; // 设备
    struct device_node* nd; // 设备树节点
    dev_t devid;
    int major;
    int minor;
    int led_gpio;
};

struct gpioled_dev gpioled;

static int gpioled_open(struct inode* inode, struct file* file)
{
    file->private_data = &gpioled; // 将设备结构体指针存储在文件私有数据中
    return 0;
}

static int gpioled_release(struct inode* inode, struct file* file)
{
    // struct gpioled_dev *dev = (struct gpioled_dev *)file->private_data;
    return 0;
}

static ssize_t gpioled_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
    int ret = 0;
    u8 databuf[1];
    struct gpioled_dev* dev = (struct gpioled_dev*)file->private_data;

    ret = copy_from_user(databuf, buf, count); // 从用户空间复制数据到内核空间
    if (ret < 0) {
        printk(KERN_ERR "Failed to copy data from user space\n");
        return -EFAULT;
    }

    if (databuf[0] == LEDON) {
        gpio_set_value(dev->led_gpio, 0); // 打开LED
        printk(KERN_INFO "LED is ON\n");
    } else if (databuf[0] == LEDOFF) {
        gpio_set_value(dev->led_gpio, 1); // 关闭LED
        printk(KERN_INFO "LED is OFF\n");
    } else {
        printk(KERN_ERR "Invalid command, use 0 to turn off LED and 1 to turn on LED\n");
        return -EINVAL;
    }
    return 0;
}

static const struct file_operations gpioled_fops = {
    .owner = THIS_MODULE,
    .open = gpioled_open, // 打开设备的函数
    .write = gpioled_write, // 写入设备的函数
    .release = gpioled_release, // 释放设备的函数
};

static int __init gpioled_init(void)
{
    int ret = 0;
    // device id
    gpioled.major = 0;
    if (gpioled.major) {
        gpioled.devid = MKDEV(gpioled.major, 0);
        register_chrdev_region(gpioled.devid, GPIOLED_CNT, GPIOLED_NAME);
    } else {
        alloc_chrdev_region(&gpioled.devid, 0, GPIOLED_CNT, GPIOLED_NAME);
        gpioled.major = MAJOR(gpioled.devid);
        gpioled.minor = MINOR(gpioled.devid);
    }
    if (ret < 0) {
        goto fail_devid;
    }
    printk("gpioled major=%d, minor=%d\n", gpioled.major, gpioled.minor);

    // cdev
    gpioled.cdev.owner = THIS_MODULE;
    cdev_init(&gpioled.cdev, &gpioled_fops);
    ret = cdev_add(&gpioled.cdev, gpioled.devid, GPIOLED_CNT);
    if (ret < 0) {
        goto fail_cdev;
    }

    // class
    gpioled.class = class_create(THIS_MODULE, GPIOLED_NAME);
    if (IS_ERR(gpioled.class)) {
        ret = PTR_ERR(gpioled.class);
        goto fail_class;
    }

    // device
    gpioled.device = device_create(gpioled.class, NULL, gpioled.devid, NULL, GPIOLED_NAME);
    if (IS_ERR(gpioled.device)) {
        ret = PTR_ERR(gpioled.device);
        goto fail_device;
    }

    // node
    gpioled.nd = of_find_node_by_path("/gpioled"); // find in dts /gpioled
    if (!gpioled.nd) {
        ret = -EINVAL;
        goto fail_findnd;
    }

    // led-gpios
    gpioled.led_gpio = of_get_named_gpio(gpioled.nd, "led-gpios", 0);
    if (gpioled.led_gpio < 0) {
        printk("get led-gpios failed\n");
        ret = -EINVAL;
        goto fail_findnd;
    }

    printk(KERN_INFO "gpioled: led_gpio = %d\n", gpioled.led_gpio);

    // request gpio
    ret = gpio_request(gpioled.led_gpio, "led-gpio");
    if (ret) { // 非零即失败
        printk(KERN_ERR "gpioled: request gpio %d failed\n", gpioled.led_gpio);
        ret = -EBUSY; // 设备忙
        goto fail_request;
    }

    // 设置GPIO为输出模式
    ret = gpio_direction_output(gpioled.led_gpio, 1); // 默认led关闭
    if (ret) {

        goto fail_setoutput;
    }

    // 设置GPIO为低电平
    gpio_set_value(gpioled.led_gpio, 0);

    return 0;

fail_setoutput:
    gpio_free(gpioled.led_gpio); // 释放GPIO
fail_request:
    of_node_put(gpioled.nd); // 释放设备树节点引用
fail_findnd:
    device_destroy(gpioled.class, gpioled.devid); // 销毁设备
fail_device:
    class_destroy(gpioled.class);
fail_class:
    cdev_del(&gpioled.cdev); // 删除字符设备
fail_cdev:
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT);
fail_devid:
    return ret;
}

static void __exit gpioled_exit(void)
{
    gpio_set_value(gpioled.led_gpio, 1); // 关闭LED

    gpio_free(gpioled.led_gpio); // 释放GPIO
    of_node_put(gpioled.nd); // 释放设备树节点引用
    device_destroy(gpioled.class, gpioled.devid); // 销毁设备
    class_destroy(gpioled.class); // 销毁设备类
    cdev_del(&gpioled.cdev); // 删除字符设备
    unregister_chrdev_region(gpioled.devid, GPIOLED_CNT); // 注销设备号
    printk("gpioled exit\n");
}

module_init(gpioled_init);
module_exit(gpioled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
