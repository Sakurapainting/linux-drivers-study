#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/init.h>
#include <linux/io.h>
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

#define TIMER_CNT 1
#define TIMER_NAME "timerdev"

#define CLOSE_CMD _IO(0xEF, 1) // 关闭定时器
#define OPEN_CMD _IO(0xEF, 2) // 打开定时器
#define SETPERIOD_CMD _IOW(0xEF, 3, int) // 设置定时器周期

struct timer_dev {
    struct cdev cdev; // 字符设备结构体
    struct class* class; // 设备类
    struct device* device; // 设备
    struct device_node* nd; // 设备树节点
    dev_t devid;
    int major;
    int minor;
    struct timer_list timer;
    int timer_period; // 定时器周期 ms
    spinlock_t lock; // 保护timer_period的自旋锁
    int led_gpio;
};

struct timer_dev timerdev;

static void timer_func(unsigned long arg)
{
    struct timer_dev* dev = (struct timer_dev*)arg; // 获取设备结构体指针
    static int sta = 1;
    int period;

    sta = !sta; // 切换LED状态

    gpio_set_value(dev->led_gpio, sta); // 设置LED状态

    // 加锁读取timer_period
    spin_lock(&dev->lock);
    period = dev->timer_period;
    spin_unlock(&dev->lock);

    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(period)); // 重新设置定时器
}

// init led
int led_init(struct timer_dev* dev)
{
    int ret = 0;

    dev->nd = of_find_node_by_path("/gpioled");
    if (dev->nd == NULL) {
        ret = -EINVAL;
        goto fail_nd;
    }

    dev->led_gpio = of_get_named_gpio(dev->nd, "led-gpios", 0);
    if (dev->led_gpio < 0) {
        ret = -EINVAL;
        goto fail_gpio;
    }

    ret = gpio_request(dev->led_gpio, "led");
    if (ret) {
        ret = -EBUSY;
        printk("IO %d request failed\n", dev->led_gpio);
        goto fail_request;
    }

    ret = gpio_direction_output(dev->led_gpio, 1);
    if (ret < 0) {
        ret = -EINVAL;
        goto fail_gpioset;
    }

    return 0;

fail_gpioset:
    gpio_free(dev->led_gpio);
fail_request:
    of_node_put(dev->nd);
fail_gpio:
fail_nd:
    return ret;
}

static int timerdev_open(struct inode* inode, struct file* file)
{
    file->private_data = &timerdev; // 将设备结构体指针存储在文件私有数据中
    return 0;
}

static int timerdev_release(struct inode* inode, struct file* file)
{
    // struct timer_dev *dev = (struct timer_dev *)file->private_data;
    return 0;
}

static ssize_t timerdev_read(struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
    int ret = 0;

    return ret;
}

static ssize_t timerdev_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
    int ret = 0;

    return ret;
}

static long timerdev_ioctl(struct file* file, unsigned int cmd, unsigned long arg)
{
    int ret = 0;
    struct timer_dev* dev = (struct timer_dev*)file->private_data; // 获取设备结构体指针

    switch (cmd) {
    case CLOSE_CMD: // 关闭定时器
        del_timer_sync(&dev->timer); // 删除定时器
        break;
    case OPEN_CMD: // 打开定时器
        if (!timer_pending(&dev->timer)) { // 定时器未激活时
            int period;
            spin_lock(&dev->lock);
            period = dev->timer_period;
            spin_unlock(&dev->lock);
            mod_timer(&dev->timer, jiffies + msecs_to_jiffies(period)); // 重新设置定时器
        }
        break;
    case SETPERIOD_CMD: { // 设置定时器周期
        int period;
        if (copy_from_user(&period, (int __user*)arg, sizeof(period))) {
            ret = -EFAULT;
            break;
        }
        // 加锁修改timer_period并立即应用
        spin_lock(&dev->lock);
        dev->timer_period = period;
        // 如果定时器正在运行，立即应用新周期
        if (timer_pending(&dev->timer)) {
            mod_timer(&dev->timer, jiffies + msecs_to_jiffies(period));
        }
        spin_unlock(&dev->lock);
        break;
    }
    default:
        ret = -EINVAL; // 无效命令
    }

    return ret;
}

static const struct file_operations timerdev_fops = {
    .owner = THIS_MODULE,
    .open = timerdev_open, // 打开设备的函数
    .read = timerdev_read, // 读取设备的函数
    .write = timerdev_write, // 写入设备的函数
    .release = timerdev_release, // 释放设备的函数
    .unlocked_ioctl = timerdev_ioctl, // ioctl函数
};

static int __init timerdev_init(void)
{
    int ret = 0;
    // device id
    timerdev.major = 0;
    if (timerdev.major) {
        timerdev.devid = MKDEV(timerdev.major, 0);
        register_chrdev_region(timerdev.devid, TIMER_CNT, TIMER_NAME);
    } else {
        alloc_chrdev_region(&timerdev.devid, 0, TIMER_CNT, TIMER_NAME);
        timerdev.major = MAJOR(timerdev.devid);
        timerdev.minor = MINOR(timerdev.devid);
    }
    if (ret < 0) {
        goto fail_devid;
    }
    printk("timerdev major=%d, minor=%d\n", timerdev.major, timerdev.minor);

    // cdev
    timerdev.cdev.owner = THIS_MODULE;
    cdev_init(&timerdev.cdev, &timerdev_fops);
    ret = cdev_add(&timerdev.cdev, timerdev.devid, TIMER_CNT);
    if (ret < 0) {
        printk("cdev_add failed\n");
        goto fail_cdev;
    }

    // class
    timerdev.class = class_create(THIS_MODULE, TIMER_NAME);
    if (IS_ERR(timerdev.class)) {
        ret = PTR_ERR(timerdev.class);
        printk("class_create failed\n");
        goto fail_class;
    }

    // device
    timerdev.device = device_create(timerdev.class, NULL, timerdev.devid, NULL, TIMER_NAME);
    if (IS_ERR(timerdev.device)) {
        ret = PTR_ERR(timerdev.device);
        printk("device_create failed\n");
        goto fail_device;
    }

    ret = led_init(&timerdev);
    if (ret < 0) {
        printk("led_init failed\n");
        goto fail_ledinit;
    }

    // init timer
    init_timer(&timerdev.timer);
    spin_lock_init(&timerdev.lock); // 初始化自旋锁
    timerdev.timer_period = 500;
    timerdev.timer.function = timer_func;
    timerdev.timer.expires = jiffies + msecs_to_jiffies(timerdev.timer_period);
    timerdev.timer.data = (unsigned long)&timerdev; // 将设备结构体指针传递给定时器函数
    add_timer(&timerdev.timer);

    return 0;

fail_ledinit:
    device_destroy(timerdev.class, timerdev.devid); // 销毁设备
fail_device:
    class_destroy(timerdev.class);
fail_class:
    cdev_del(&timerdev.cdev); // 删除字符设备
fail_cdev:
    unregister_chrdev_region(timerdev.devid, TIMER_CNT);
fail_devid:
    return ret;
}

static void __exit timerdev_exit(void)
{
    gpio_set_value(timerdev.led_gpio, 1); // 关闭LED
    del_timer(&timerdev.timer); // 删除定时器

    gpio_free(timerdev.led_gpio);
    of_node_put(timerdev.nd);

    device_destroy(timerdev.class, timerdev.devid); // 销毁设备
    class_destroy(timerdev.class); // 销毁设备类
    cdev_del(&timerdev.cdev); // 删除字符设备
    unregister_chrdev_region(timerdev.devid, TIMER_CNT); // 注销设备号
    printk("timerdev exit\n");
}

module_init(timerdev_init);
module_exit(timerdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
