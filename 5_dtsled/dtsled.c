#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/slab.h>
#include <linux/uaccess.h>

#define DTSLED_NAME "dtsled" /* 主设备号 */
#define DTSLED_COUNT 1 /* 设备数量 */

#define LEDOFF 0 /* 关闭LED */
#define LEDON 1 /* 打开LED */

/* 地址映射后的虚拟地址指针 */
static void __iomem* ccm_ccgr1; /* CCM_CCGR1寄存器 */
static void __iomem* sw_mux_gpio1_io03; /* SW_MUX_GPIO */
static void __iomem* sw_pad_gpio1_io03; /* SW_PAD_GPIO */
static void __iomem* gpio1_gdir; /* GPIO方向寄存器 */
static void __iomem* gpio1_dr; /* GPIO数据寄存器 */

struct dtsled_dev {
    struct cdev cdev; // 字符设备结构体
    dev_t devid; // 设备号
    struct class* class; // 设备类
    struct device* device; // 设备
    int major;
    int minor;
    struct device_node* nd; // 设备树节点
};

struct dtsled_dev dtsled;

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

static int dtsled_open(struct inode* inode, struct file* file)
{
    file->private_data = &dtsled; // 将设备结构体指针存储在文件私有数据中
    return 0;
}

static int dtsled_release(struct inode* inode, struct file* file)
{
    // struct dtsled_dev *dev = (struct dtsled_dev *)file->private_data;
    return 0;
}

static ssize_t dtsled_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
    // struct dtsled_dev *dev = (struct dtsled_dev *)file->private_data;

    int retvalue = 0;
    unsigned char databuf[1];

    retvalue = copy_from_user(databuf, buf, count); // 从用户空间复制数据到内核空间
    if (retvalue < 0) {
        return -EFAULT;
    }

    /* 判断开灯还是关灯 */
    led_switch(databuf[0]);

    return 0;
}

const struct file_operations dtsled_fops = {
    .owner = THIS_MODULE,
    .open = dtsled_open, // 打开设备的函数
    .write = dtsled_write, // 写入设备的函数
    .release = dtsled_release, // 释放设备的函数
};

static int __init dtsled_init(void)
{
    int ret = 0;
    u32 val = 0;
    const char* str;
    // u32 regdata[10];
    // u8 i = 0;

    // 注册分配设备号
    dtsled.major = 0; // 动态分配主设备号
    if (dtsled.major) {
        dtsled.devid = MKDEV(dtsled.major, 0);
        ret = register_chrdev_region(dtsled.devid, DTSLED_COUNT, DTSLED_NAME);
    } else {
        ret = alloc_chrdev_region(&dtsled.devid, 0, DTSLED_COUNT, DTSLED_NAME);
        dtsled.major = MAJOR(dtsled.devid);
        dtsled.minor = MINOR(dtsled.devid);
    }
    if (ret < 0) {
        goto fail_devid;
    }

    // cdev
    dtsled.cdev.owner = THIS_MODULE;
    cdev_init(&dtsled.cdev, &dtsled_fops);

    ret = cdev_add(&dtsled.cdev, dtsled.devid, DTSLED_COUNT);
    if (ret < 0) {
        goto fail_cdev;
    }

    dtsled.class = class_create(THIS_MODULE, DTSLED_NAME);
    if (IS_ERR(dtsled.class)) {
        ret = PTR_ERR(dtsled.class);
        goto fail_class;
    }

    dtsled.device = device_create(dtsled.class, NULL, dtsled.devid, NULL, DTSLED_NAME);
    if (IS_ERR(dtsled.device)) {
        ret = PTR_ERR(dtsled.device);
        goto fail_device;
    }

    /* 获取设备树属性内容 */
    dtsled.nd = of_find_node_by_path("/alphaled");
    if (!dtsled.nd) {
        ret = -ENODEV;
        goto fail_findnd;
    }

    ret = of_property_read_string(dtsled.nd, "compatible", &str);
    if (ret < 0) {
        goto fail_rs;
    } else {
        printk("compatible: %s\n", str);
    }

    ret = of_property_read_string(dtsled.nd, "status", &str);
    if (ret < 0) {
        goto fail_rs;
    } else {
        printk("status: %s\n", str);
    }

#if 0
    ret = of_property_read_u32_array(dtsled.nd, "reg", regdata, ARRAY_SIZE(regdata));
    if(ret < 0){
        goto fail_rs;
    }
    else{
        printk("reg data:\n");
        for(i = 0; i < ARRAY_SIZE(regdata); i++) {
            printk("%#x\n", regdata[i]);
        }
        printk("\n");
    }

#endif

    /* led init */

#if 0
    /*  sakoroyou 20250731 */ 
	alphaled {
#address - cells = < 1>;
#size - cells = < 1>;
		compatible ="alientek, alphaled";
		status = "okay";
		reg = < 0x020c406c 0x04	/*ccm_ccgr1_base*/
				0x020e0068 0x04 /*sw_mux_gpio1_io03_base*/
				0x020e02f4 0x04 /*sw_pad_gpio1_io03_base*/
				0x0209c004 0x04 /*gpio1_gdir_base*/
				0x0209c000 0x04 /*gpio1_dr_base*/
			>;
	};
#endif

#if 0
    ccm_ccgr1 = ioremap(regdata[0], regdata[1]);
    sw_mux_gpio1_io03 = ioremap(regdata[2], regdata[3]);
    sw_pad_gpio1_io03 = ioremap(regdata[4], regdata[5]);
    gpio1_gdir = ioremap(regdata[6], regdata[7]);
    gpio1_dr = ioremap(regdata[8], regdata[9]);

#endif
    ccm_ccgr1 = of_iomap(dtsled.nd, 0); // 获取ccm_ccgr1寄存器的物理地址并映射到虚拟地址
    sw_mux_gpio1_io03 = of_iomap(dtsled.nd, 1); // 获取sw_mux_gpio1_io03寄存器的物理地址并映射到虚拟地址
    sw_pad_gpio1_io03 = of_iomap(dtsled.nd, 2); // 获取sw_pad_gpio1_io03寄存器的物理地址并映射到虚拟地址
    gpio1_gdir = of_iomap(dtsled.nd, 3); // 获取gpio1_gdir寄存器的物理地址并映射到虚拟地址
    gpio1_dr = of_iomap(dtsled.nd, 4); // 获取gpio1_dr寄存器的物理地址并映射到虚拟地址

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

    return 0;

fail_rs:

fail_findnd:
    device_destroy(dtsled.class, dtsled.devid); // 销毁设备
fail_device:
    class_destroy(dtsled.class);
fail_class:
    cdev_del(&dtsled.cdev); // 删除字符设备
fail_cdev:
    unregister_chrdev_region(dtsled.devid, DTSLED_COUNT); // 注销设备号
fail_devid:
    return ret;
}

static void __exit dtsled_exit(void)
{
    u32 val = 0;

    val = readl(gpio1_dr);
    val |= (1 << 3); // 设置GPIO1_DR寄存器的bit3，关闭LED
    writel(val, gpio1_dr); // 写入GPIO1_DR寄存器

    iounmap(ccm_ccgr1);
    iounmap(sw_mux_gpio1_io03);
    iounmap(sw_pad_gpio1_io03);
    iounmap(gpio1_gdir);
    iounmap(gpio1_dr);

    device_destroy(dtsled.class, dtsled.devid); // 销毁设备
    class_destroy(dtsled.class); // 销毁设备类
    cdev_del(&dtsled.cdev); // 删除字符设备
    unregister_chrdev_region(dtsled.devid, DTSLED_COUNT); // 注销设备号
}

module_init(dtsled_init);
module_exit(dtsled_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
