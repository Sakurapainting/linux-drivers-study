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
#include <linux/fcntl.h>
#include <linux/poll.h>
#include <linux/signal.h>
#include <linux/platform_device.h>


// platform match table
static const struct of_device_id miscbeep_of_match[] = {
    { .compatible = "alientek,beep", },
    {/* Sentinel */},
};

static int miscbeep_probe(struct platform_device *pdev) {

    return 0;
}

static int miscbeep_remove(struct platform_device *pdev) {

    return 0;
}

// platform
static struct platform_driver miscbeep_driver = {
    .driver = {
        .name = "miscbeep",
        .owner = THIS_MODULE,
        .of_match_table = of_match_ptr(miscbeep_of_match),  // of_match_ptr 条件编译是否支持设备树
    },
    .probe = miscbeep_probe,
    .remove = miscbeep_remove,
};

static int __init miscbeep_init(void) {
    return platform_driver_register(&miscbeep_driver);
}

static void __exit miscbeep_exit(void) {
    platform_driver_unregister(&miscbeep_driver);
}

module_init(miscbeep_init);
module_exit(miscbeep_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
