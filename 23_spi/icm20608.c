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
};

static struct icm20608_dev icm20608dev;

static int icm20608_open(struct inode* inode, struct file* file)
{

    return 0;
}

ssize_t icm20608_read(struct file* file, char __user* buf, size_t size, loff_t* offset)
{
    return 0;
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
