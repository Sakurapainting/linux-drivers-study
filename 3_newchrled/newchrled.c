#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define NEWCHRLED_NAME "newchrled" /* 主设备号 */
#define NEWCHRLED_COUNT 1 /* 设备数量 */

/* 寄存器物理地址 */
#define CCM_CCGR1_BASE (0x020C406C) /* 时钟控制_时钟门控 */
#define SW_MUX_GPIO1_IO03_BASE (0x020E0068) /* 复用 */
#define SW_PAD_GPIO1_IO03_BASE (0x020E02F4) /* 电气属性配置 */
#define GPIO1_GDIR_BASE (0x0209C004) /* 方向（输入/输出）寄存器 */
#define GPIO1_DR_BASE (0x0209C000) /* 数据寄存器 */

static void __iomem* ccm_ccgr1;
static void __iomem* sw_mux_gpio1_io03;
static void __iomem* sw_pad_gpio1_io03;
static void __iomem* gpio1_gdir;
static void __iomem* gpio1_dr;

#define LEDOFF 0 /* 关闭LED */
#define LEDON 1 /* 打开LED */

void led_switch(u8 sta)
{
    u32 val = 0;
    if (sta == LEDON) {
        val = readl(gpio1_dr); // 读取GPIO1_DR寄存器
        val &= ~(1 << 3); // 清除GPIO1_DR寄存器的bit3，打开LED
        writel(val, gpio1_dr); // 写入GPIO1_DR寄存器
        printk(KERN_INFO "LED is ON\n");
    } else if (sta == LEDOFF) {
        val = readl(gpio1_dr); // 读取GPIO1_DR寄存器
        val |= (1 << 3); // 设置GPIO1_DR寄存器的bit3，关闭LED
        writel(val, gpio1_dr); // 写入GPIO1_DR寄存器
        printk(KERN_INFO "LED is OFF\n");
    } else {
        printk(KERN_ERR "Invalid command, use 0 to turn off LED and 1 to turn on LED\n");
    }
}

struct newchrled_dev {
    struct cdev cdev;
    dev_t dev; /* 设备号 */
    struct class* class;
    struct device* device;
    int major;
    int minor;
};

struct newchrled_dev newchrled;

static int newchrled_open(struct inode* inode, struct file* file)
{
    return 0;
}

static int newchrled_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
    int retvalue = 0;
    unsigned char databuf[1];
    retvalue = copy_from_user(databuf, buf, count); // 从用户空间复制数据到内核空间
    if (retvalue < 0) {
        printk(KERN_ERR "Failed kernel write\n");
        return -EFAULT;
    }

    /* 判断开灯还是关灯 */
    led_switch(databuf[0]);
    return 0;
}

static int newchrled_release(struct inode* inode, struct file* file)
{
    return 0;
}

static const struct file_operations newchrled_fops = {
    .owner = THIS_MODULE,
    .write = newchrled_write,
    .open = newchrled_open,
    .release = newchrled_release,
};

static int __init newchrled_init(void)
{
    int ret = 0;
    unsigned int val = 0;

    printk(KERN_INFO "Init newchrled\n");

    /* 初始化led */
    ccm_ccgr1 = ioremap(CCM_CCGR1_BASE, 4); //一个寄存器32位，所以是4字节地址
    sw_mux_gpio1_io03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    sw_pad_gpio1_io03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    gpio1_gdir = ioremap(GPIO1_GDIR_BASE, 4);
    gpio1_dr = ioremap(GPIO1_DR_BASE, 4);

    val = readl(ccm_ccgr1);
    val &= ~(3 << 26); // 清除CCM_CCGR1寄存器的位26和27
    val |= (3 << 26); // 设置CCM_CCGR1寄存器的位26和27为1，开启GPIO1时钟
    writel(val, ccm_ccgr1); // 写入CCM_CCGR1寄存器

    writel(0x5, sw_mux_gpio1_io03); // 设置SW_MUX_GPIO1_IO03寄存器为5，选择GPIO1_IO03功能
    writel(0x10B0, sw_pad_gpio1_io03); // 设置SW_PAD_GPIO1_IO03寄存器为0x10B0，配置GPIO1_IO03的电气属性

    val = readl(gpio1_gdir);
    val |= (1 << 3); // 设置GPIO1_IO03为输出模式，bit3置1
    writel(val, gpio1_gdir); // 写入GPIO1_GDIR

    val = readl(gpio1_dr);
    val |= (1 << 3); // 关闭LED
    writel(val, gpio1_dr); // 写入GPIO1_DR寄存器

    printk(KERN_INFO "Newchrled module initialized\n");

    newchrled.major = 0; // 由系统分配设备号
    /* 注册设备号 */
    if (newchrled.major) {
        newchrled.dev = MKDEV(newchrled.major, 0);
        ret = register_chrdev_region(newchrled.dev, NEWCHRLED_COUNT, NEWCHRLED_NAME);
    } else {
        ret = alloc_chrdev_region(&newchrled.dev, 0, NEWCHRLED_COUNT, NEWCHRLED_NAME);
        newchrled.major = MAJOR(newchrled.dev);
        newchrled.minor = MINOR(newchrled.dev);
    }

    if (ret < 0) {
        printk(KERN_ERR "newchrled chrdev_region error\n");
        goto fail_dev;
    }
    printk("newchrled major: %d, minor: %d\n", newchrled.major, newchrled.minor);

    /* 注册字符设备 */
    newchrled.cdev.owner = THIS_MODULE;

    cdev_init(&newchrled.cdev, &newchrled_fops);

    ret = cdev_add(&newchrled.cdev, newchrled.dev, NEWCHRLED_COUNT);
    if (ret < 0) {
        goto fail_cdev;
    }

    /* 自动创建设备节点 */
    newchrled.class = class_create(THIS_MODULE, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.class)) {
        ret = PTR_ERR(newchrled.class);
        goto fail_class;
    }

    newchrled.device = device_create(newchrled.class, NULL, newchrled.dev, NULL, NEWCHRLED_NAME);
    if (IS_ERR(newchrled.device)) {
        printk(KERN_ERR "KERN:Failed to create device\n");
        ret = PTR_ERR(newchrled.device);
        goto fail_device;
    }
    return 0;

fail_device:
    class_destroy(newchrled.class);
fail_class:
    cdev_del(&newchrled.cdev);
fail_cdev:
    unregister_chrdev_region(newchrled.dev, NEWCHRLED_COUNT);
fail_dev:
    return ret;
}

static void __exit newchrled_exit(void)
{
    unsigned int val = 0;

    printk(KERN_INFO "Exit newchrled\n");

    val = readl(gpio1_dr);
    val |= (1 << 3); // 设置GPIO1_DR寄存器的bit3，关闭LED
    writel(val, gpio1_dr); // 写入GPIO1_DR寄存器

    /* 取消地址映射 */
    iounmap(ccm_ccgr1);
    iounmap(sw_mux_gpio1_io03);
    iounmap(sw_pad_gpio1_io03);
    iounmap(gpio1_gdir);
    iounmap(gpio1_dr);

    cdev_del(&newchrled.cdev); // 注销字符设备

    /* 注销设备号 */
    unregister_chrdev_region(newchrled.dev, NEWCHRLED_COUNT);

    /* 销毁设备 */
    device_destroy(newchrled.class, newchrled.dev);
    class_destroy(newchrled.class);

    printk(KERN_INFO "Newchrled module exited\n");
}

module_init(newchrled_init);
module_exit(newchrled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
