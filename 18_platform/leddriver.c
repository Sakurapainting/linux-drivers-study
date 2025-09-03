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

static int led_probe(struct platform_device *dev) {
    printk("led driver probe\n");
    return 0;
}

static int led_remove(struct platform_device *dev) {
    printk("led driver remove\n");
    return 0;
}

static struct platform_driver leddriver = {
    .driver = {
        .name = "imx6ull-led",
        .owner = THIS_MODULE,
    },
    .probe = led_probe,
    .remove = led_remove,
};

static int __init leddriver_init(void) {
    return platform_driver_register(&leddriver);
}

static void  __exit leddriver_exit(void) {
    platform_driver_unregister(&leddriver);
}

module_init(leddriver_init);
module_exit(leddriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
