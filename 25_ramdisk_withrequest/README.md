# 块设备驱动

## 块设备驱动 - 块设备简介

块设备是针对存储设备的，比如 SD 卡、EMMC、NAND Flash、Nor Flash、SPI Flash、机械硬盘、固态硬盘等。因此块设备驱动其实就是这些存储设备驱动，块设备驱动相比字符设备驱动的主要区别如下：

- 块设备只能以块为单位进行读写访问，块是 linux 虚拟文件系统(VFS)基本的数据传输单位。字符设备是以字节为单位进行数据传输的，不需要缓冲。
- 块设备在结构上是可以进行随机访问的，对于这些设备的读写都是按块进行的，块设备使用缓冲区来暂时存放数据，等到条件成熟以后再一次性将缓冲区中的数据写入块设备中。这么做的目的为了提高块设备寿命，大家如果仔细观察的话就会发现有些硬盘或者 NAND Flash就会标明擦除次数(flash 的特性，写之前要先擦除)，比如擦除 100000 次等。因此，为了提高块设备寿命引入了缓冲区，数据先写入到缓冲区中，等满足一定条件后再一次性写入到真正的物理存储设备中，这样就减少了对块设备的擦除次数，提高了块设备寿命。
- 字符设备是顺序的数据流设备，字符设备是按照字节进行读写访问的。字符设备不需要缓冲区，对于字符设备的访问都是实时的，而且也不需要按照固定的块大小进行访问。
- 块设备结构的不同其 I/O 算法也会不同，比如对于 EMMC、SD 卡、NAND Flash 这类没有任何机械设备的存储设备就可以任意读写任何的扇区(块设备物理存储单元)。但是对于机械硬盘这样带有磁头的设备，读取不同的盘面或者磁道里面的数据，磁头都需要进行移动，因此对于机械硬盘而言，将那些杂乱的访问按照一定的顺序进行排列可以有效提高磁盘性能，linux 里面针对不同的存储设备实现了不同的 I/O 调度算法。

## 块设备驱动 - 块设备驱动框架

### block_device 结构体

- 内核使用 block_device 来表示一个具体的块设备对象，比如一个硬盘或者分区，如果是硬盘的话 bd_disk 就指向通用磁盘结构 gendisk。gendisk 是 Linux 内核中表示 **通用磁盘(Generic Disk)** 的核心数据结构，用于描述一个完整的块设备(如整个硬盘、SD卡、EMMC等)。
- 向内核注册新的块设备、申请设备号，块设备注册函数为
register_blkdev

```c
int register_blkdev(unsigned int major, const char *name)
```

函数参数和返回值含义如下：
major：主设备号。
name：块设备名字。
返回值：如果参数 major 在 1~255 之间的话表示自定义主设备号，那么返回 0 表示注册成功，如果返回负值的话表示注册失败。如果 major 为 0 的话表示由系统自动分配主设备号，那么返回值就是系统分配的主设备号(1~255)，如果返回负值那就表示注册失败。

- 和字符设备驱动一样，如果不使用某个块设备了，那么就需要注销掉，函数为 unregister_blkdev

```c
void unregister_blkdev(unsigned int major, const char *name)
```

### gendisk 结构体

linux 内核使用 gendisk 来描述一个磁盘设备，这是一个结构体，定义在 include/linux/genhd.h 中

- `int major / int first_minor / int minors`：主设备号、起始次设备号以及最多支持的次设备数量。块设备驱动在 `add_disk()` 前必须设置好这些值，内核会据此为 `/dev` 下的节点分配设备号。
- `char disk_name[DISK_NAME_LEN]`：块设备在系统中的名字，例如 `sda`、`mmcblk0`。这既会出现在 `/dev` 中，也会用于内核日志与调试输出。
- `struct block_device_operations *fops`：块设备操作函数集，封装 `open/close/ioctl/compat_ioctl/release` 等方法。VFS 在访问磁盘时会回调这里的函数，因此驱动初始化阶段必须指向自己的 `block_device_operations` 实例。
- `struct request_queue *queue`：请求队列指针，块层 I/O 调度器与驱动的数据传输入口。大多数驱动会先通过 `blk_alloc_queue()`/`blk_init_queue()` 创建队列，再赋值给 `gendisk->queue`。
- `struct hd_struct part0` 与 `struct disk_part_tbl *part_tbl`：内核对分区的抽象。`part0` 表示整盘的伪分区，`part_tbl` 指向动态分区表结构；用户空间的分区操作最终都会映射到这些结构上。为结构体 disk_part_tbl 类型，disk_part_tbl 的核心是一个 hd_struct 结构体指针数组，此数组每一项都对应一个分区信息。
- `struct device dev`：将磁盘注册为一个标准的设备对象，挂载到 `/sys/block/<disk_name>`。这允许 sysfs、udev 以及电源管理框架对磁盘进行统一管理。
- `void *private_data`：驱动自定义上下文指针，常用来挂接控制器或底层硬件的私有数据，便于在通用回调中取回驱动上下文。

编写块的设备驱动的时候需要分配并初始化一个 gendisk，linux 内核提供了一组 gendisk 操作函数

- 使用 gendisk 之前要先申请，allo_disk 函数用于申请一个 gendisk

```c
struct gendisk *alloc_disk(int minors)
```

minors：次设备号数量，也就是 gendisk 对应的分区数量。
返回值：成功：返回申请到的 gendisk，失败：NULL。

- 如果要删除 gendisk 的话可以使用函数 del_gendisk

```c
void del_gendisk(struct gendisk *gp)
```

gp：要删除的 gendisk。
返回值：无。

- 使用 alloc_disk 申请到 gendisk 以后系统还不能使用，必须使用 add_disk 函数将申请到的 gendisk 添加到内核中

```c
void add_disk(struct gendisk *disk)
```

disk：要添加到内核的 gendisk。
返回值：无。

- 每一个磁盘都有容量，所以在初始化 gendisk 的时候也需要设置其容量，使用函数set_capacity

```c
void set_capacity(struct gendisk *disk, sector_t size)
```

disk：要设置容量的 gendisk。
size：磁盘容量大小，注意这里是扇区数量。块设备中最小的可寻址单元是扇区，一个扇区一般是 512 字节，有些设备的物理扇区可能不是 512 字节。不管物理扇区是多少，内核和块设备驱动之间的扇区都是 512 字节。所以 set_capacity 函数设置的大小就是块设备实际容量除以512 字节得到的扇区数量。比如一个 2MB 的磁盘，其扇区数量就是(2*1024*1024)/512=4096。
返回值：无。

- 内核会通过 get_disk 和 put_disk 这两个函数来调整 gendisk 的引用计数，根据名字就可以知道，get_disk 是增加 gendisk 的引用计数，put_disk 是减少 gendisk 的引用计数（kobject 是 Linux 内核统一对象管理的基础设施，它让设备驱动能够：自动管理对象生命周期(避免内存泄漏)、向用户空间暴露设备信息(sysfs)）

```c
struct kobject *get_disk(struct gendisk *disk)
void put_disk(struct gendisk *disk)
```

### block_device_operations 结构体

此结构体定义在 include/linux/blkdev.h 中。

block_device_operations 结构体里面的操作集函数和字符设备的 file_operations操作集基本类似，但是块设备的操作集函数比较少。

- `int (*open)(struct block_device *bdev, fmode_t mode)`：打开块设备时调用。当用户空间程序打开块设备文件(如 `open("/dev/sda", ...)`)或内核挂载文件系统时，VFS 会回调此函数。驱动可以在这里进行设备初始化、权限检查、引用计数管理等操作。`bdev` 指向被打开的块设备对象，`mode` 标识打开模式(只读/读写等)。
- `void (*release)(struct gendisk *disk, fmode_t mode)`：关闭块设备时调用。与 `open` 对应，当最后一个引用关闭时触发。驱动可以在这里释放资源、停止设备等。注意参数是 `gendisk` 而非 `block_device`。
- `int (*ioctl)(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)`：处理用户空间的 ioctl 系统调用。用于执行设备特定的控制命令，如获取磁盘几何信息(HDIO_GETGEO)、格式化、介质弹出等操作。驱动需要根据 `cmd` 参数执行相应操作。
- `int (*compat_ioctl)(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)`：用于 32 位程序在 64 位内核上执行 ioctl 的兼容层。如果驱动的 ioctl 命令涉及指针或结构体大小差异，需要实现此函数进行数据转换。
- `int (*getgeo)(struct block_device *bdev, struct hd_geometry *geo)`：获取磁盘几何参数。当用户空间程序使用 `ioctl(fd, HDIO_GETGEO, ...)` 查询磁盘 CHS(Cylinder/Head/Sector) 参数时调用。驱动需要填充 `hd_geometry` 结构体，包括磁头数、扇区数、柱面数和起始扇区号。虽然现代硬盘多使用 LBA 寻址，但某些分区工具和引导程序仍需要这些几何信息。
- `int (*direct_access)(struct block_device *bdev, sector_t sector, void **kaddr, unsigned long *pfn, long size)`：直接访问持久内存(如 NVDIMM、PMEM)的接口。对于支持 DAX(Direct Access)模式的存储设备，此函数可以返回指定扇区对应的内核虚拟地址和物理页帧号，允许应用程序绕过页缓存直接访问存储器。
- `int (*media_changed)(struct gendisk *disk)`：检查可移动介质是否已更换。对于软盘、光驱、可移动 USB 存储等设备，内核会定期调用此函数检测介质变化。如果介质已更换返回非零值，内核会重新读取分区表。
- `int (*revalidate_disk)(struct gendisk *disk)`：重新验证磁盘。当 `media_changed` 返回真或手动触发时，内核调用此函数让驱动重新扫描设备容量、分区信息等。常用于热插拔场景。
- `struct module *owner`：指向拥有此操作集的内核模块。通常设为 `THIS_MODULE`，用于模块引用计数管理，防止模块在设备使用期间被卸载。

### 块设备 I/O 请求过程

block_device_operations 结构体中没有 read 和 write 这样的读写函数，块设备是从物理块设备中读写数据，要通过 request_queue、request 和 bio。

- **request_queue**：内核将对块设备的读写都发送到请求队列 request_queue 中，request_queue 中是大量的request(请求结构体)，而 request 又包含了 bio，bio 保存了读写相关数据，比如从块设备的哪个
地址开始读取、读取的数据长度，读取到哪里，如果是写的话还包括要写入的数据等。我们先来看一下 request_queue，这是一个结构体，定义在文件 include/linux/blkdev.h 中
  - ①、blk_init_queue申请并初始化请求队列

```c
request_queue *blk_init_queue(request_fn_proc *rfn, spinlock_t *lock)
```

rfn：请求处理函数指针，每个 request_queue 都要有一个请求处理函数

```c
void (request_fn_proc) (struct request_queue *q)
```

lock：自旋锁指针，需要驱动编写人员定义一个自旋锁，然后传递进来。请求队列会使用这个自旋锁。
返回值：如果为 NULL 的话表示失败，成功的话就返回申请到的 request_queue 地址。

  - ②、blk_cleanup_queue删除请求队列

```c
void blk_cleanup_queue(struct request_queue *q)
```

q：需要删除的请求队列。
返回值：无。

- 分配请求队列，并绑定制造请求函数
- 对于EMMC，SD卡这样的非机械设备的块设备，可以随机访问。不需要复杂的I/O调度器。
- 使用blk_alloc_queue申请请求队列，然后使用blk_queue_make_request绑定制造请求函数。

```c
struct request_queue *blk_alloc_queue(gfp_t gfp_mask)
```

gfp_mask：内存分配掩码，具体可选择的掩码值请参考 include/linux/gfp.h 中的相关宏定义，一般为 GFP_KERNEL。
返回值：申请到的无 I/O 调度的 request_queue。

```c
void blk_queue_make_request(struct request_queue *q, make_request_fn *mfn)
```

q：需要绑定的请求队列，也就是 blk_alloc_queue 申请到的请求队列。
mfn：需要绑定的“制造”请求函数

```c
void (make_request_fn) (struct request_queue *q, struct bio *bio)
```

- blk_fetch_request函数获取request。就是直接调用了 blk_peek_request 和 blk_start_request 这两个函数。

```c
struct request *blk_fetch_request(struct request_queue *q)
{
    struct request *rq;
    rq = blk_peek_request(q);
    if (rq)
        blk_start_request(rq);
        return rq;
    }
```

- bio结构体：每个request里面有多个bio，bio保存最终要读写的数据，地址等信息。遍历请求中的 bio：

```c
#define __rq_for_each_bio(_bio, rq) \
    if ((rq->bio)) \
        for (_bio = (rq)->bio; _bio; _bio = _bio->bi_next)
```

_bio 就是遍历出来的每个 bio，rq 是要进行遍历操作的请求，_bio 参数为 bio 结构体指针类型，rq 参数为 request 结构体指针类型。

- bio 包含了最终要操作的数据，因此还需要遍历 bio 中 的所有段，这里要用到bio_for_each_segment 函数

```c
#define bio_for_each_segment(bvl, bio, iter) \
    __bio_for_each_segment(bvl, bio, iter, (bio)->bi_iter)
```

第一个 bvl 参数就是遍历出来的每个 bio_vec，第二个 bio 参数就是要遍历的 bio，类型为bio 结构体指针，第三个 iter 参数保存要遍历的 bio 中 bi_iter 成员变量。

- 如果使用“制造请求”，也就是抛开 I/O 调度器直接处理 bio 的话，在 bio 处理完成以后要通过内核 bio 处理完成，使用 bio_endio 函数

```c
bvoid bio_endio(struct bio *bio, int error)
```

error：如果 bio 处理成功的话就直接填 0，如果失败的话就填个负值，比如-EIO。
返回值：无

## 块设备驱动 - 请求队列实验

本实验参考自 linux 内核 drivers/block/z2ram.c

驱动出入口函数：

```c
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

    /* 5、申请并初始化请求队列 */
    ramdisk.queue = blk_init_queue(ramdisk_request_fn, &ramdisk.lock);
    if (!ramdisk.queue) {
        ret = -EINVAL;
	    goto blk_queue_fail;
    }

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
```

fops函数：

```c
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
```

请求函数和数据处理：

```c
/* 具体的数据处理过程 */
static void ramdisk_transfer(struct request *req)
{
    /* 数据传输三要素：源，目的，长度。
     * 内存地址，块设备地址，长度 
     */
    unsigned long start = blk_rq_pos(req) << 9;  /* blk_rq_pos获取到要操作的块设备扇区地址，左移9位，地址字节 */
    unsigned long len  = blk_rq_cur_bytes(req);  /* 数据长度 */

    /* 获取bio里面的缓冲区:
     * 如果是读：从磁盘里面读取到的数据保存在次缓冲区里面
     * 如果是写：此缓冲区保存着要写入到磁盘里面的数据   
     */
    void *buffer = bio_data(req->bio);  

    if (rq_data_dir(req) == READ)       /* 读操作 */ 
		memcpy(buffer, ramdisk.ramdiskbuf + start, len);     
	else                                /* 写操作 */
		memcpy(ramdisk.ramdiskbuf + start, buffer, len);
}

/* 请求函数 */
static void ramdisk_request_fn(struct request_queue *q)
{
    int err = 0;
    struct request *req;

	req = blk_fetch_request(q);  /* 有电梯调度算法， */
	while (req) {
    
        /* 处理request，也就是具体的数据读写操作 */
        ramdisk_transfer(req);

        if (!__blk_end_request_cur(req, err))
			req = blk_fetch_request(q);
    }
}
```

驱动加载成功以后就会在/dev/目录下生成一个名为“ramdisk”的设备， 输入如下命令查看磁盘信息

```bash
fdisk -l
```

```bash
Disk /dev/ramdisk: 2 MB, 2097152 bytes, 4096 sectors
32 cylinders, 2 heads, 64 sectors/track
Units: cylinders of 128 * 512 = 65536 bytes

Disk /dev/ramdisk doesn't contain a valid partition table
```

ramdisk 已经识别出来了，大小为 2MB，但是同时也提示/dev/ramdisk没有分区表，因为我们还没有格式化/dev/ramdisk。

使用 mkfs.vfat 命令格式化/dev/ramdisk，将其格式化成 vfat 格式

```bash
mkfs.vfat /dev/ramdisk
```

```bash
/lib/modules/4.1.15 # mkfs.vfat /dev/ramdisk 
ramdisk_open
ramdisk_getgeo
ramdisk_release
```

格式化完成以后就可以挂载/dev/ramdisk 来访问了，挂载点可以自定义

```bash
mount /dev/ramdisk /tmp
```

```bash
/lib/modules/4.1.15 # mount /dev/ramdisk /tmp
ramdisk_open
ramdisk_release
ramdisk_open
ramdisk_release
ramdisk_open
ramdisk_release
ramdisk_open
```

挂载成功以后就可以通过/tmp 来访问 ramdisk 这个磁盘了，进入到/tmp 目录中，可以通过vi 命令新建一个 txt 文件来测试磁盘访问是否正常。

恢复：

```bash
/lib/modules/4.1.15 # umount /tmp
ramdisk_release
```

```bash
/lib/modules/4.1.15 # rmmod ramdisk.ko
ramdisk_exit
```