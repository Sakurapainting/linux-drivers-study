### IMX6ULL LinuxDriver

#### 内核自带 MISC 驱动

- MISC 设备的主设备号为 10
- MISC 的 cdev 会自动创建，不用手动
- MISC 设备是基于 platform
- 核心就是初始化 `miscdevice` 结构体变量，然后使用 `misc_register` 向内核注册，卸载时使用 `misc_deregister`
- `name` 是 设备名字，注册成功会在 `/dev` 目录下生成名为 `name` 的设备文件
- 以前要用一堆函数去创建设备，比如：

```c
alloc_chrdev_region();  // 申请设备号
cdev_init();            // 初始化cdev
cdev_add();             // 添加cdev
class_create();         // 创建类
device_create();        // 创建设备
```

现在我们可以直接用 `misc_register` 一个函数

同样的，卸载时也可以用 `misc_deregister` 代替下列函数：

```c
cdev_del();                 // 删除cdev
unregister_chrdev_region(); // 注册设备号
device_destroy();           // 删除设备
class_destroy();            // 删除类
```

- 如果设备 miscdevice 里面 minor 为 255 的话，表示由内核自动分配给次设备号

#### 使用 MISC 驱动框架

- 设备树根节点下创建 beep 节点，之前 chapter9 已经加过了
- 在 probe 初始化 io 后，注册 misc 设备，不用手动创建 cdev，在开发板 `/dev` 能找到：

```c
    // misc驱动注册
    ret = misc_register(&miscbeep_dev);
    if (ret) {
        printk("misc_register failed!\n");
        goto fail_register;
    }
```

### misc

#### of_match_ptr

- of_match_ptr 是一个内核宏，用于条件编译设备树匹配表。
- 在支持设备树（CONFIG_OF=y）的内核中，它返回 miscbeep_of_match 的指针。
- 在不支持设备树（CONFIG_OF=n）的内核中，它返回 NULL，避免编译错误。
