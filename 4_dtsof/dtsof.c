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

static int __init dtsof_init(void)
{
    int ret = 0;

    struct device_node* bl_np = NULL;
    struct property* prop = NULL;
    const char* str = NULL;
    u32 def_value = 0;
    u32 elemsize = 0;
    u32* brival = NULL;
    u8 i = 0;

    /* 找到backlight 节点 */
    bl_np = of_find_node_by_path("/backlight");
    if (bl_np == NULL) {
        ret = -ENODEV;
        goto fail_findnp;
    }

    /* 获取backlight 节点的属性 */
    prop = of_find_property(bl_np, "compatible", NULL);
    if (prop == NULL) {
        ret = -ENODEV;
        goto fail_findprop;
    } else {
        printk("compatible = %s\n", (char*)prop->value);
    }

    /* 获取backlight 节点的string属性 */
    ret = of_property_read_string(bl_np, "status", &str);
    if (ret < 0) {
        goto fail_readstatus;
    } else {
        printk("status = %s\n", str);
    }

    /* 获取backlight 节点的u32属性 */
    ret = of_property_read_u32(bl_np, "default-brightness-level", &def_value);
    if (ret < 0) {
        goto fail_read32;
    } else {
        printk("default-brightness-level = %d\n", def_value);
    }

    /* 获取数组类型的属性*/
    elemsize = of_property_count_elems_of_size(bl_np, "brightness-levels", sizeof(u32));
    if (elemsize < 0) {
        ret = -EINVAL;
        goto fail_read_elem;
    } else {
        printk("brightness-levels count = %d\n", elemsize);
    }

    /* 分配内存 */
    brival = kmalloc(elemsize * sizeof(u32), GFP_KERNEL);
    if (!brival) {
        ret = -ENOMEM;
        goto fail_kmalloc;
    }

    /* 获取数组 */
    ret = of_property_read_u32_array(bl_np, "brightness-levels", brival, elemsize);
    if (ret < 0) {
        goto fail_read_array;
    } else {
        for (i = 0; i < elemsize; i++) {
            printk("brightness-levels[%d] = %d\n", i, brival[i]);
        }
    }
    kfree(brival);

    return 0;

fail_read_array:
    kfree(brival);

fail_kmalloc:

fail_read_elem:

fail_read32:

fail_readstatus:

fail_findprop:

fail_findnp:
    return ret;
}

static void __exit dtsof_exit(void)
{
}

module_init(dtsof_init);
module_exit(dtsof_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SakoroYou");
