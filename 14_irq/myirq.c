#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/uaccess.h> 
#include <linux/io.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/irq.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/timer.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>
#include <asm/mach/map.h>
#include <asm/uaccess.h>
#include <asm/io.h>

#define MYIRQ_CNT     1
#define MYIRQ_NAME    "myirqdev"

#define KEY_NUM      1

#define KEY0VALUE   0x01
#define INVAKEY     0xFF

struct irq_keydesc{     // 按键中断描述
    int gpio;           // io 编号
    int irqnum;         // 中断号
    u8 value;           // 键值    
    char name[10];
    irqreturn_t (*handler) (int, void *);        // 中断处理函数
};

struct myirq_dev{
    struct cdev cdev;      // 字符设备结构体
    struct class *class;   // 设备类
    struct device *device; // 设备
    struct device_node *nd; // 设备树节点
    dev_t devid;
    int major;
    int minor;
    struct irq_keydesc irqkey[KEY_NUM];
    struct timer_list timer;
    spinlock_t lock;

    atomic_t keyvalue;
    atomic_t releasekey;
};

struct myirq_dev myirqdev;

static int myirqdev_open(struct inode *inode, struct file *file) {
    file->private_data = &myirqdev; // 将设备结构体指针存储在文件私有数据中
    return 0;
}

static int myirqdev_release(struct inode *inode, struct file *file) {
    // struct myirq_dev *dev = (struct myirq_dev *)file->private_data;
    return 0;
}

static ssize_t myirqdev_read(struct file *file, char __user *buf, size_t count, loff_t *ppos) {
    int ret = 0;
    
    return ret;
}

static ssize_t myirqdev_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos) {
    int ret = 0;

    return ret;
}

static const struct file_operations myirqdev_fops = {
    .owner = THIS_MODULE,
    .open = myirqdev_open, // 打开设备的函数
    .read = myirqdev_read, // 读取设备的函数
    .write = myirqdev_write, // 写入设备的函数    
    .release = myirqdev_release, // 释放设备的函数

}; 

static irqreturn_t key0_handler(int irq, void *dev_id) {
    struct irq_keydesc *keydesc = (struct irq_keydesc *)dev_id;
    struct myirq_dev *dev = container_of(keydesc, struct myirq_dev, irqkey[0]);

    dev->timer.data = (volatile unsigned long)keydesc;       // 传递当前按键描述符
    mod_timer(&dev->timer, jiffies + msecs_to_jiffies(10));// 10ms 消抖

    return IRQ_HANDLED;
}

static void timer_func(unsigned long arg) {
    int value = 0;
    struct irq_keydesc *keydesc = (struct irq_keydesc *)arg;
    value = gpio_get_value(keydesc->gpio);
    if(value == 0) {
        printk("Key %s pressed\n", keydesc->name);
    } else {
        printk("Key %s released\n", keydesc->name);
    }
}

/* key init */
static int keyio_init(struct myirq_dev *dev) {
    int ret = 0;
    int i = 0;
    int j = 0;

    dev->nd = of_find_node_by_path("/key"); // 查找设备树节点
    if(!dev->nd){
        ret = -EINVAL;
        goto fail_nd;
    }

    for(i = 0; i < KEY_NUM; i++){
        dev->irqkey[i].gpio = of_get_named_gpio(dev->nd, "key-gpios", i);
        if(dev->irqkey[i].gpio < 0){
            ret = -EINVAL;
            goto fail_name;
        }
    }

    for(i = 0; i < KEY_NUM; i++){
        memset(dev->irqkey[i].name, 0, sizeof(dev->irqkey[i].name)); 
        sprintf(dev->irqkey[i].name, "key%d", i); // 设置按键名称
        ret = gpio_request(dev->irqkey[i].gpio, dev->irqkey[i].name); // 请求GPIO
        if(ret) {
            ret = -EBUSY;
            printk("IO %d request failed\n", dev->irqkey[i].gpio);
            goto fail_request;
        }
        
        ret = gpio_direction_input(dev->irqkey[i].gpio); // 设置GPIO为输入
        if(ret < 0) {
            ret = -EINVAL;
            goto fail_gpioset;
        }

        dev->irqkey[i].irqnum = gpio_to_irq(dev->irqkey[i].gpio); // 获取中断号

#if 0
        dev->irqkey[i].irqnum = irq_of_parse_and_map(dev->nd, i); // 通用方法获取中断号
#endif
    }

    dev->irqkey[0].handler = key0_handler;
    dev->irqkey[0].value = KEY0VALUE; 

    // interrupt init
    for(i = 0; i < KEY_NUM; i++){
        ret = request_irq(dev->irqkey[i].irqnum, dev->irqkey[i].handler, IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING, dev->irqkey[i].name, &dev->irqkey[i]); // 请求中断
        if(ret) {
            printk("request_irq failed for %s\n", dev->irqkey[i].name);
            goto fail_irq;
        }
    }
     
    // timer init
    init_timer(&myirqdev.timer);
    spin_lock_init(&myirqdev.lock); 
    myirqdev.timer.function = timer_func;

    return 0;

fail_irq:
    for(j = 0; j < i; j++){
        free_irq(dev->irqkey[j].irqnum, &dev->irqkey[j]);
    }
fail_gpioset:
    gpio_free(dev->irqkey[i].gpio);
fail_request:
    for(j = 0; j < i; j++){
        gpio_free(dev->irqkey[j].gpio); // 释放GPIO
    }
fail_name:
    of_node_put(dev->nd);
fail_nd:
    return ret;
}

static int __init myirqdev_init(void){
    int ret = 0;
    // device id
    myirqdev.major = 0;
    if(myirqdev.major){
        myirqdev.devid = MKDEV(myirqdev.major, 0);
        register_chrdev_region(myirqdev.devid, MYIRQ_CNT, MYIRQ_NAME);
    }
    else{
        alloc_chrdev_region(&myirqdev.devid, 0, MYIRQ_CNT, MYIRQ_NAME);
        myirqdev.major = MAJOR(myirqdev.devid);
        myirqdev.minor = MINOR(myirqdev.devid);
    }
    if(ret < 0) {
        goto fail_devid;
    }
    printk("myirqdev major=%d, minor=%d\n", myirqdev.major, myirqdev.minor);

    // cdev
    myirqdev.cdev.owner = THIS_MODULE;
    cdev_init(&myirqdev.cdev, &myirqdev_fops);
    ret = cdev_add(&myirqdev.cdev, myirqdev.devid, MYIRQ_CNT);
    if(ret < 0){
        printk("cdev_add failed\n");
        goto fail_cdev;
    }

    // class
    myirqdev.class = class_create(THIS_MODULE, MYIRQ_NAME);
    if(IS_ERR(myirqdev.class)){
        ret = PTR_ERR(myirqdev.class);
        printk("class_create failed\n");
        goto fail_class;
    }
    
    // device
    myirqdev.device = device_create(myirqdev.class, NULL, myirqdev.devid, NULL, MYIRQ_NAME);
    if(IS_ERR(myirqdev.device)){
        ret = PTR_ERR(myirqdev.device);
        printk("device_create failed\n");
        goto fail_device;
    }

    // io init
    ret = keyio_init(&myirqdev);
    if(ret < 0) {
        printk(KERN_ERR "Failed to initialize key GPIO\n");
        goto fail_keyio;
    }

    // init atomic
    atomic_set(&myirqdev.keyvalue, INVAKEY);
    atomic_set(&myirqdev.releasekey, INVAKEY);

    return 0;
fail_keyio:
    device_destroy(myirqdev.class, myirqdev.devid);
fail_device:
    class_destroy(myirqdev.class);
fail_class:
    cdev_del(&myirqdev.cdev); // 删除字符设备
fail_cdev:
    unregister_chrdev_region(myirqdev.devid, MYIRQ_CNT);
fail_devid:
    return ret;
}

static void __exit myirqdev_exit(void){
    int i;

    // del timer
    del_timer_sync(&myirqdev.timer);

    for(i = 0; i < KEY_NUM; i++){
        free_irq(myirqdev.irqkey[i].irqnum, &myirqdev.irqkey[i]);
    }

    for(i = 0; i < KEY_NUM; i++){
        gpio_free(myirqdev.irqkey[i].gpio); // 释放GPIO
    }

    device_destroy(myirqdev.class, myirqdev.devid); // 销毁设备
    class_destroy(myirqdev.class); // 销毁设备类
    cdev_del(&myirqdev.cdev); // 删除字符设备
    unregister_chrdev_region(myirqdev.devid, MYIRQ_CNT); // 注销设备号
    printk("myirqdev exit\n");
}



module_init(myirqdev_init);
module_exit(myirqdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
