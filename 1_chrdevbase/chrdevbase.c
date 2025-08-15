#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>

#define CHRDEVBASE_MAJOR 200 /* 主设备号 */
#define CHRDEVBASE_NAME "chrdevbase" /* 设备名称 */

static char readbuf[100] = { 0 }; /* 读取缓冲区 */
static char writebuf[100] = { 0 }; /* 写入缓冲区 */
static char kerneldata[] = { "kernel data" }; /* 内核数据示例 */

static int chrdevbase_open(struct inode* inode, struct file* file)
{
    printk("chrdevbase_open\n");
    return 0; /* 成功打开设备 */
}

static int chrdevbase_release(struct inode* inode, struct file* file)
{
    printk("chrdevbase_release\n");
    return 0; /* 成功释放设备 */
}

static ssize_t chrdevbase_read(struct file* file, char __user* buf, size_t count, loff_t* ppos)
{
    int ret = 0;

    printk("chrdevbase_read\n");

    memcpy(readbuf, kerneldata, sizeof(kerneldata)); /* 将内核数据复制到读取缓冲区 */
    ret = copy_to_user(buf, readbuf, count); /* 将内核数据复制到用户空间 */
    if (ret < 0) {
        printk("copy_to_user failed\n");
        return -EFAULT; /* 复制失败，返回错误 */
    }
    printk("(printk)read data: %s\n", readbuf); /* 打印读取的数据 */

    return 0; /* 成功读取数据 */
}

static ssize_t chrdevbase_write(struct file* file, const char __user* buf, size_t count, loff_t* ppos)
{
    int ret = 0;

    printk("chrdevbase_write\n");

    ret = copy_from_user(writebuf, buf, count); /* 从用户空间复制数据到写入缓冲区 */
    if (ret < 0) {
        printk("copy_from_user failed\n");
        return -EFAULT; /* 复制失败，返回错误 */
    }
    printk("(printk)write data: %s\n", writebuf); /* 打印写入的数据 */

    return count; /* 成功写入数据 */
}

static const struct file_operations chrdevbase_fops = {
    .owner = THIS_MODULE, /* 模块所有者 */
    /* 其他操作函数可以在这里定义 */
    .read = chrdevbase_read,
    .write = chrdevbase_write,
    .open = chrdevbase_open,
    .release = chrdevbase_release,
};

static int __init chrdevbase_init(void)
{

    int ret = 0;

    printk("chrdevbase_init\n");

    /* 注册字符设备 */
    ret = register_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME, &chrdevbase_fops);
    if (ret < 0) {
        printk("register_chrdev failed\n");
        return ret;
    }

    return 0;
}

static void __exit chrdevbase_exit(void)
{
    printk("chrdevbase_exit\n");

    /* 注销字符设备 */
    unregister_chrdev(CHRDEVBASE_MAJOR, CHRDEVBASE_NAME);
}

/*
    模块入口、出口
*/

module_init(chrdevbase_init); /*入口*/
module_exit(chrdevbase_exit); /*出口*/

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");