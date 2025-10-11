#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/regmap.h>
#include <linux/gpio/consumer.h>
#include <linux/of_irq.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/debugfs.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/input/mt.h>
#include <linux/input/touchscreen.h>
#include <linux/i2c.h>
#include <asm/unaligned.h>
#include <linux/blkdev.h>
#include <linux/hdreg.h>

#define RAMDISK_SIZE (2 * 1024 * 1024)      // 2 MB
#define RAMDISK_NAME "ramdisk"
#define RAMDISK_MINOR 3                     // 三个磁盘分区

/* ramdisk设备结构体 */
struct ramdisk_dev{
    int major;                              /* 主设备号 */
    unsigned char *ramdiskbuf;              /* ramdisk的内存空间，模拟磁盘的空间 */
    struct gendisk *gendisk;
    struct request_queue *queue;
    spinlock_t lock;                        /* 自旋锁 */
};

struct ramdisk_dev ramdisk;

/* 制造请求函数 */
static void ramdisk_make_request(struct request_queue *queue, struct bio *bio)
{
	int offset;
	struct bio_vec bvec;
	struct bvec_iter iter;
    unsigned long len = 0;

    offset = bio->bi_iter.bi_sector << 9;  /* 要操作的磁盘起始扇区偏移,改为字节地址 1个扇区（sector）= 512字节 512 = 2^9 */

    /* 循环处理每个段 */
    bio_for_each_segment(bvec, bio, iter) {
        /* 获取bio里面的缓冲区:
         * 如果是读：从磁盘里面读取到的数据保存在次缓冲区里面
         * 如果是写：此缓冲区保存着要写入到磁盘里面的数据   
         */
        char *ptr = page_address(bvec.bv_page) + bvec.bv_offset; 
        len = bvec.bv_len;  /* 长度 */

        if (bio_data_dir(bio) == READ)       /* 读操作 */ 
		    memcpy(ptr, ramdisk.ramdiskbuf + offset, len);     
	    else                                /* 写操作 */
		    memcpy(ramdisk.ramdiskbuf + offset, ptr, len);

        offset += len;
    }

    set_bit(BIO_UPTODATE, &bio->bi_flags);
	bio_endio(bio, 0);
}

static int ramdisk_open(struct block_device *bdev, fmode_t mode)
{
    printk("ramdisk_open\r\n");
    return 0;
}

static void ramdisk_release(struct gendisk *disk, fmode_t mode)
{
    printk("ramdisk_release\r\n");
}

static int ramdisk_getgeo(struct block_device *dev, struct hd_geometry *geo)
{
    printk("ramdisk_getgeo\r\n");
    /* https://www.cnblogs.com/itplay/p/11026084.html */
    /* 硬盘信息 */
    geo->heads = 2; /* 磁头 */
    geo->cylinders = 32; /* 柱面，磁道 */
    geo->sectors = RAMDISK_SIZE/(2 * 32 * 512); /* 一个磁道里面的扇区数量 */

    return 0;
}

/* 块设备操作集 */
static const struct block_device_operations ramdisk_fops =
{
	.owner		= THIS_MODULE,
	.open		= ramdisk_open,
	.release	= ramdisk_release,
    .getgeo     = ramdisk_getgeo,
};

/* 驱动入口函数 */
static int __init ramdisk_init(void)
{

    int ret = 0;
    printk("ramdisk_init\r\n");

    /* 1、先申请内存 */
    ramdisk.ramdiskbuf = kzalloc(RAMDISK_SIZE, GFP_KERNEL);
    if(ramdisk.ramdiskbuf == NULL) {
        ret = -EINVAL;
        goto ramalloc_fail;
    }

    /* 2、注册块设备 */
    ramdisk.major = register_blkdev(0, RAMDISK_NAME);
    if(ramdisk.major < 0) {
        ret = -EINVAL;
        goto ramdisk_register_blkdev_fail;
    }
    printk("rmadisk major = %d\r\n", ramdisk.major);

    /* 3、申请gendisk */
    ramdisk.gendisk = alloc_disk(RAMDISK_MINOR);
    if (!ramdisk.gendisk){
        ret = -EINVAL;
        goto gendisk_alloc_fail;
    }
	   
    /* 4、初始化自旋锁 */
    spin_lock_init(&ramdisk.lock);

    /* 5、申请请求队列 */
    ramdisk.queue = blk_alloc_queue(GFP_KERNEL);
    if (!ramdisk.queue) {
        ret = -EINVAL;
	    goto blk_queue_fail;
    }

    /* 绑定“制造请求”函数 */
    blk_queue_make_request(ramdisk.queue, ramdisk_make_request);

    /* 6、初始化gendisk */
    ramdisk.gendisk->major = ramdisk.major;  /* 主设备号 */
    ramdisk.gendisk->first_minor = 0;
    ramdisk.gendisk->fops = &ramdisk_fops;
    ramdisk.gendisk->private_data = &ramdisk;
    ramdisk.gendisk->queue = ramdisk.queue;
    sprintf(ramdisk.gendisk->disk_name, RAMDISK_NAME);
    set_capacity(ramdisk.gendisk, RAMDISK_SIZE/512);  /* 设置gendisk容量，单位扇区 */
    add_disk(ramdisk.gendisk);

    return 0;


blk_queue_fail:
    put_disk(ramdisk.gendisk);
gendisk_alloc_fail:
    unregister_blkdev(ramdisk.major, RAMDISK_NAME);
ramdisk_register_blkdev_fail:
    kfree(ramdisk.ramdiskbuf);
ramalloc_fail:
    return ret;
}

/* 驱动出口函数 */
static void __exit ramdisk_exit(void)
{
    printk("ramdisk_exit\r\n");
    del_gendisk(ramdisk.gendisk);
    put_disk(ramdisk.gendisk);

    blk_cleanup_queue(ramdisk.queue);

    unregister_blkdev(ramdisk.major, RAMDISK_NAME);
    kfree(ramdisk.ramdiskbuf);
}

module_init(ramdisk_init);
module_exit(ramdisk_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
