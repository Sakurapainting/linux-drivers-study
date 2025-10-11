# 块设备驱动

## 块设备驱动 - 不使用请求队列实验

请求队列会用到 I/O 调度器，适合机械硬盘这种存储设备。对于 EMMC、SD、ramdisk 这样没有机械结构的存储设备，我们可以直接访问任意一个扇区，因此可以不需要 I/O 调度器，也就不需要请求队列了。

本实验在上一个实验的基础上修改而来，参考了linux 内核 drivers/block/zram/zram_drv.c。重点来看一下与上一个实验不同的地方，首先是驱动入口函数 ramdisk_init，ramdisk_init 函数大部分和上一个实验相同，只是本实验中改为使用blk_queue_make_request 函数设置“制造请求”函数。

与上一个实验不同的地方，这里使用 blk_alloc_queue和 blk_queue_make_request 这两个函数取代了上一个实验的 blk_init_queue 函数。

```c
/* 5、申请请求队列 */
    ramdisk.queue = blk_alloc_queue(GFP_KERNEL);
    if (!ramdisk.queue) {
        ret = -EINVAL;
	    goto blk_queue_fail;
    }

    /* 绑定“制造请求”函数 */
    blk_queue_make_request(ramdisk.queue, ramdisk_make_request);
```

“制造请求”函数:

```c
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
```

虽然 ramdisk_make_request_fn 函数第一个参数依旧是请求队列，但是实际上这个请求队列不包含真正的请求，所有的处理内容都在第二个 bio 参数里面，所以 ramdisk_make_request_fn函数里面是全部是对 bio 的操作。