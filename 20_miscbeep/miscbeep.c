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

// miscbeep 设备结构体
struct miscbeep_dev {
    struct device_node *nd; // 设备节点
    int beep_gpio;          // 蜂鸣器IO
};

struct miscbeep_dev miscbeep;

// platform match table
static const struct of_device_id miscbeep_of_match[] = {
    { .compatible = "alientek,beep", },
    {/* Sentinel */},
};

static int miscbeep_probe(struct platform_device *pdev) {
    int ret = 0;
    // 初始化蜂鸣器IO
    miscbeep.nd = pdev->dev.of_node;

    miscbeep.beep_gpio = of_get_named_gpio(miscbeep.nd, "beep-gpios", 0);
    if(miscbeep.beep_gpio < 0) {
        ret = -EINVAL;
        goto fail_findgpio;
    }

    ret = gpio_request(miscbeep.beep_gpio, "beep-gpios");
    if(ret) {
        printk("gpio_request %d failed!\n", miscbeep.beep_gpio);
        ret = -EINVAL;
        goto fail_requestgpio;
    }

    ret = gpio_direction_output(miscbeep.beep_gpio, 1); // 设置GPIO输出，默认关闭蜂鸣器
    if(ret) {
        goto fail_setoutput;
    }

    // 默认关闭蜂鸣器（多余  可去掉）
    // gpio_set_value(beep.beep_gpio, 1);



    // misc驱动注册

    return 0;

fail_setoutput:
    gpio_free(miscbeep.beep_gpio);
fail_requestgpio:
fail_findgpio:
    of_node_put(miscbeep.nd);
    return ret;
}

static int miscbeep_remove(struct platform_device *pdev) {
    gpio_set_value(miscbeep.beep_gpio, 1); // 确保蜂鸣器关闭
    gpio_free(miscbeep.beep_gpio);
    of_node_put(miscbeep.nd);
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
