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

#define KEY_CNT 1
#define KEY_NAME "key"
#define KEY0VALUE 0xf0 // 自定义
#define INVALKEY 0x00 // 自定义

#define KEYON 1
#define KEYOFF 0

struct key_dev {
    struct cdev cdev; // 字符设备结构体
    struct class* class; // 设备类
    struct device* device; // 设备
    struct device_node* nd; // 设备树节点
    dev_t devid;
    int major;
    int minor;
    int key_gpio;
    atomic_t key_value; // 按键值
};

struct key_dev key;

static int key_open(struct inode* inode, struct file* file)
{
    file->private_data = &key; // 将设备结构体指针存储在文件私有数据中
    return 0;
}

static int key_release(struct inode* inode, struct file* file)
{
    // struct key_dev *dev = (struct key_dev *)file->private_data;
    return 0;
}

static ssize_t key_read(struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
    int value = 0;
    int ret = 0;
    struct key_dev* dev = (struct key_dev*)file->private_data;

    if (!gpio_get_value(dev->key_gpio)) { // 按键按下
        while (!gpio_get_value(dev->key_gpio))
            ; // 等待按键松开
        atomic_set(&dev->key_value, KEY0VALUE); // 设置按键值
    } else {
        atomic_long_set(&dev->key_value, INVALKEY); // 设置按键值为无效
    }

    value = atomic_read(&dev->key_value); // 读取按键值

    ret = copy_to_user(buf, &value, sizeof(value)); // 将按键值复制到用户空间

    // 成功时返回实际读取的字节数
    return sizeof(value);
}

static ssize_t key_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
    int ret = 0;

    return ret;
}

static const struct file_operations key_fops = {
    .owner = THIS_MODULE,
    .open = key_open, // 打开设备的函数
    .read = key_read, // 读取设备的函数
    .write = key_write, // 写入设备的函数
    .release = key_release, // 释放设备的函数
};

/* key io init*/
static int key_io_init(struct key_dev* dev)
{
    int ret = 0;

    atomic_set(&key.key_value, INVALKEY); // 初始化按键值为无效

    dev->nd = of_find_node_by_path("/key"); // find in dts /key
    if (!dev->nd) {
        ret = -EINVAL;
        goto fail_nd;
    }

    dev->key_gpio = of_get_named_gpio(dev->nd, "key-gpios", 0);
    if (dev->key_gpio < 0) {
        ret = -EINVAL;
        goto fail_gpio;
    }

    ret = gpio_request(dev->key_gpio, "key0");
    if (ret) {
        ret = -EBUSY;
        printk(KERN_ERR "Failed to request GPIO %d\n", dev->key_gpio);
        goto fail_request;
    }

    ret = gpio_direction_input(dev->key_gpio); // 设置GPIO为输入模式
    if (ret < 0) {
        ret = -EINVAL;
        printk(KERN_ERR "Failed to set key GPIO as input\n");
        goto fail_setinput;
    }
    return 0;

fail_setinput:
    gpio_free(dev->key_gpio);
fail_request:
    of_node_put(dev->nd);
fail_gpio:
fail_nd:
    return ret;
}

static int __init key_init(void)
{
    int ret = 0;
    // device id
    key.major = 0;
    if (key.major) {
        key.devid = MKDEV(key.major, 0);
        register_chrdev_region(key.devid, KEY_CNT, KEY_NAME);
    } else {
        alloc_chrdev_region(&key.devid, 0, KEY_CNT, KEY_NAME);
        key.major = MAJOR(key.devid);
        key.minor = MINOR(key.devid);
    }
    if (ret < 0) {
        goto fail_devid;
    }
    printk("key major=%d, minor=%d\n", key.major, key.minor);

    // cdev
    key.cdev.owner = THIS_MODULE;
    cdev_init(&key.cdev, &key_fops);
    ret = cdev_add(&key.cdev, key.devid, KEY_CNT);
    if (ret < 0) {
        goto fail_cdev;
    }

    // class
    key.class = class_create(THIS_MODULE, KEY_NAME);
    if (IS_ERR(key.class)) {
        ret = PTR_ERR(key.class);
        goto fail_class;
    }

    // device
    key.device = device_create(key.class, NULL, key.devid, NULL, KEY_NAME);
    if (IS_ERR(key.device)) {
        ret = PTR_ERR(key.device);
        goto fail_device;
    }

    ret = key_io_init(&key); // 初始化GPIO
    if (ret < 0) {
        printk(KERN_ERR "Failed to initialize key GPIO\n");
        goto fail_io_init;
    }

    return 0;

fail_io_init:
    device_destroy(key.class, key.devid); // 销毁设备
fail_device:
    class_destroy(key.class);
fail_class:
    cdev_del(&key.cdev); // 删除字符设备
fail_cdev:
    unregister_chrdev_region(key.devid, KEY_CNT);
fail_devid:
    return ret;
}

static void __exit key_exit(void)
{
    gpio_free(key.key_gpio);
    of_node_put(key.nd);
    device_destroy(key.class, key.devid); // 销毁设备
    class_destroy(key.class); // 销毁设备类
    cdev_del(&key.cdev); // 删除字符设备
    unregister_chrdev_region(key.devid, KEY_CNT); // 注销设备号
    printk("key exit\n");
}

module_init(key_init);
module_exit(key_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
