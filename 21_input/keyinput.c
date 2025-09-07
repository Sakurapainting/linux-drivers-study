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

#define KEYINPUT_CNT 1
#define KEYINPUT_NAME "keyinput"

#define KEY_NUM 1

#define KEY0VALUE 0x01
#define INVAKEY 0xFF

struct irq_keydesc { // 按键中断描述
    int gpio; // io 编号
    int irqnum; // 中断号
    u8 value; // 键值
    char name[10];
    irqreturn_t (*handler)(int, void*); // 中断处理函数
};

struct keyinput_dev {
    struct device_node* nd; // 设备树节点
    struct irq_keydesc irqkey[KEY_NUM];
    struct timer_list timer;
    spinlock_t lock;

    struct input_dev *inputdev;
};

struct keyinput_dev keyinputdev;

static irqreturn_t key0_handler(int irq, void* dev_id)
{
    struct irq_keydesc* keydesc = (struct irq_keydesc*)dev_id;
    struct keyinput_dev* dev = container_of(keydesc, struct keyinput_dev, irqkey[0]);

    dev->timer.data = (volatile unsigned long)keydesc; // 传递当前按键描述符
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10)); // 10ms 消抖

    return IRQ_HANDLED;
}

static void timer_func(unsigned long arg)
{
    int value = 0;
    struct irq_keydesc* keydesc = (struct irq_keydesc*)arg;
    struct keyinput_dev* dev = container_of(keydesc, struct keyinput_dev, irqkey[0]);

    value = gpio_get_value(keydesc->gpio);
    if (value == 0) {
        // 上报按键值
        input_event(dev->inputdev, EV_KEY, KEY_0, 1); // 按下
        input_sync(dev->inputdev); // 同步事件
        printk("Key %s pressed\n", keydesc->name);
    } else if (value == 1) {
        // 上报按键值
        input_event(dev->inputdev, EV_KEY, KEY_0, 0); // 松开
        input_sync(dev->inputdev); // 同步事件
        printk("Key %s released\n", keydesc->name);
    }
}

/* key init */
static int keyio_init(struct keyinput_dev* dev)
{
    int ret = 0;
    int i = 0;
    int j = 0;

    dev->nd = of_find_node_by_path("/key"); // 查找设备树节点
    if (!dev->nd) {
        ret = -EINVAL;
        goto fail_nd;
    }

    for (i = 0; i < KEY_NUM; i++) {
        dev->irqkey[i].gpio = of_get_named_gpio(dev->nd, "key-gpios", i);
        if (dev->irqkey[i].gpio < 0) {
            ret = -EINVAL;
            goto fail_name;
        }
    }

    for (i = 0; i < KEY_NUM; i++) {
        memset(dev->irqkey[i].name, 0, sizeof(dev->irqkey[i].name));
        sprintf(dev->irqkey[i].name, "key%d", i); // 设置按键名称
        ret = gpio_request(dev->irqkey[i].gpio, dev->irqkey[i].name); // 请求GPIO
        if (ret) {
            ret = -EBUSY;
            printk("IO %d request failed\n", dev->irqkey[i].gpio);
            goto fail_request;
        }

        ret = gpio_direction_input(dev->irqkey[i].gpio); // 设置GPIO为输入
        if (ret < 0) {
            ret = -EINVAL;
            goto fail_gpioset;
        }

        dev->irqkey[i].irqnum = gpio_to_irq(dev->irqkey[i].gpio); // 获取中断号

#if 0
        dev->irqkey[i].irqnum = irq_of_parse_and_map(dev->nd, i); // 通用方法获取中断号
#endif
    }

    dev->irqkey[0].handler = key0_handler;
    dev->irqkey[0].value = KEY_0;

    // interrupt init
    for (i = 0; i < KEY_NUM; i++) {
        ret = request_irq(dev->irqkey[i].irqnum, dev->irqkey[i].handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, dev->irqkey[i].name, &dev->irqkey[i]); // 请求中断
        if (ret) {
            printk("request_irq failed for %s\n", dev->irqkey[i].name);
            goto fail_irq;
        }
    }

    // timer init
    init_timer(&keyinputdev.timer);
    spin_lock_init(&keyinputdev.lock);
    keyinputdev.timer.function = timer_func;

    return 0;

fail_irq:
    for (j = 0; j < i; j++) {
        free_irq(dev->irqkey[j].irqnum, &dev->irqkey[j]);
    }
fail_gpioset:
    gpio_free(dev->irqkey[i].gpio);
fail_request:
    for (j = 0; j < i; j++) {
        gpio_free(dev->irqkey[j].gpio); // 释放GPIO
    }
fail_name:
    of_node_put(dev->nd);
fail_nd:
    return ret;
}

static int __init keyinput_init(void)
{
    int ret = 0;

    // io init
    ret = keyio_init(&keyinputdev);
    if (ret < 0) {
        printk(KERN_ERR "Failed to initialize key GPIO\n");
        goto fail_keyio;
    }

    // 注册 input_dev
    keyinputdev.inputdev = input_allocate_device();
    if (!keyinputdev.inputdev) {
        ret = -ENOMEM;
        goto fail_inputalloc;
    }

    keyinputdev.inputdev->name = KEYINPUT_NAME;
    __set_bit(EV_KEY, keyinputdev.inputdev->evbit); // 支持按键事件  evbit标识能支持什么类型的事件
    __set_bit(EV_REP, keyinputdev.inputdev->evbit); // 支持按键重复事件
    __set_bit(KEY_0, keyinputdev.inputdev->keybit); // 按键号 是 按键0

    ret = input_register_device(keyinputdev.inputdev);
    if (ret) {
        printk(KERN_ERR "Failed to register input device\n");
        goto fail_input_register;
    }

    return 0;

fail_input_register:
    input_free_device(keyinputdev.inputdev);
fail_inputalloc:
fail_keyio:
    return ret;
}

static void __exit keyinput_exit(void)
{
    int i;

    // 注销 input_dev
    input_unregister_device(keyinputdev.inputdev);
    // input_free_device(keyinputdev.inputdev); // unregister 会自动 free

    // del timer
    del_timer_sync(&keyinputdev.timer);

    for (i = 0; i < KEY_NUM; i++) {
        free_irq(keyinputdev.irqkey[i].irqnum, &keyinputdev.irqkey[i]);
    }

    for (i = 0; i < KEY_NUM; i++) {
        gpio_free(keyinputdev.irqkey[i].gpio); // 释放GPIO
    }

    printk("keyinputdev exit\n");
}

module_init(keyinput_init);
module_exit(keyinput_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
