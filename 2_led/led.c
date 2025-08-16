#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define LED_MAJOR 200 /* 主设备号 */
#define LED_NAME "led" /* 设备名称 */

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

static int led_open(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "LED device opened\n");
    return 0; /* 成功打开设备 */
}

static int led_release(struct inode* inode, struct file* file)
{
    printk(KERN_INFO "LED device released\n");
    return 0; /* 成功释放设备 */
}

static int led_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
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

static struct file_operations led_fops = {
    .owner = THIS_MODULE,
    .open = led_open,
    .write = led_write,
    .release = led_release,
};

static int __init led_init(void)
{
    int ret = 0;
    unsigned int val = 0;

    /* 初始化led */
    ccm_ccgr1 = ioremap(CCM_CCGR1_BASE, 4); //一个寄存器32位，所以是4字节地址
    sw_mux_gpio1_io03 = ioremap(SW_MUX_GPIO1_IO03_BASE, 4);
    sw_pad_gpio1_io03 = ioremap(SW_PAD_GPIO1_IO03_BASE, 4);
    gpio1_gdir = ioremap(GPIO1_GDIR_BASE, 4);
    gpio1_dr = ioremap(GPIO1_DR_BASE, 4);

    val = readl(ccm_ccgr1);
    val &= ~(3 << 26); // 清除CCM_CCGR1寄存器的位26和27    （11左移到26~27取反后位与，即清零。其他位上保持不变）
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

    ret = register_chrdev(LED_MAJOR, LED_NAME, &led_fops); // 注册字符设备
    if (ret < 0) {
        printk(KERN_INFO "register chrdev failed");
        return -EIO;
    }

    printk(KERN_INFO "LED module initialized\n");

    return 0;
}

static void __exit led_exit(void)
{
    unsigned int val = 0;

    val = readl(gpio1_dr);
    val |= (1 << 3); // 设置GPIO1_DR寄存器的bit3，关闭LED
    writel(val, gpio1_dr); // 写入GPIO1_DR寄存器

    /* 取消地址映射 */
    iounmap(ccm_ccgr1);
    iounmap(sw_mux_gpio1_io03);
    iounmap(sw_pad_gpio1_io03);
    iounmap(gpio1_gdir);
    iounmap(gpio1_dr);

    unregister_chrdev(LED_MAJOR, LED_NAME); // 注销字符设备

    printk(KERN_INFO "LED module exited\n");
}

module_init(led_init);
module_exit(led_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
