# uboot启动流程

## u-boot.lds

从这个文件中可知，uboot入口地址为 _start

ENTRY(_start) 声明的入口函数 _start

函数 _start 在 arch/arm/lib/vectors.S 文件中，此函数的作用是声明一些中断函数，当上电启动时会跳转到 reset 复位函数

下面的名字可以在 u-boot.map 里搜索到

**这些值要编译之后才会知道！** 所以不同测试看到的值不会一样

**__image_copy_start**  -> 0x87800000 img_cp（重定位时要复制的内容）

.vectors                -> 0x87800000 存放中断向量表

arch/arm/cpu/armv7/start.o 

**__image_copy_end**    -> 0x8784f1a4

**__rel_dyn_start**     -> 0x8784f1a4 rel段（重定位relocation，当U-Boot从加载地址复制到运行地址时，需要修正代码中的绝对地址引用，使代码能够在不同的内存地址正确运行）

**__rel_dyn_end**       -> 0x8785794c

**__end**               -> 0x8785794c

_image_binary_end       -> 0x8785794c

**__bss_start**         -> 0x8784f1a4 bss段（未初始化全局变量、静态变量Block Started by Symbol，在程序启动时，这段内存会被清零，在二进制文件中不占用实际空间，只记录大小）

**__bss_end**           -> 0x8789a194

## uboot启动入口

### reset函数

在文件 arch/arm/cpu/armv7/start.S 文件中

bicne = bic (bit clean) + ne(not equal)

- reset 函数目的是将处理器设置为SVC模式，并且关闭FIQ和IRQ
- 设置中断向量。
- 初始化CP15系统控制协处理器

### lowlevel_init函数

在文件 arch/arm/cpu/armv7/lowlevel_init.S 中

设置SP指针（为函数调用、局部变量分配、中断 / 异常处理提供栈空间。）、R9寄存器（存放全局数据指针gd地址）

### s_init函数

s_init 函数在 arch/arm/cpu/armv7/xxx/soc.c 文件中，有的芯片型号中没有 soc.c 文件，而 s_init 函数没有什么作用，就可以不用了解了 。

## uboot外设初始化

### _main

在文件 arch/arm/lib/crt0.S

### board_init_f函数

在文件 common/board_f.c 中

执行 init_sequence_f 表中的函数，主要有两个工作：

- 初始化一系列外设，比如串口、定时器，或者打印一些消息等。
- 初始化 gd 的各个成员变量，uboot 会将自己重定位到 DRAM 最后面的地址区域，也就是将自己拷贝到 DRAM 最后面的内存区域中。

其中 serial_init 函数初始串口后，我们就可以使用 printf 函数打印日志，打印后便会在控制台中看到相应的信息，和C语言中的用法一样，

display_options 函数中会打印 uboot 的版本信息等

### relocate_code函数

在文件 arch/arm/lib/relocate.S 中，主要作用是用于代码拷贝

### relocate_vectors函数

在文件 arch/arm/lib/relocate.S 中，主要作用是用于重定位向量表。

设置VBAR寄存器为重定位后的中断向量表起始地址。

### board_init_r函数

在文件 common/board_r.c 中，主要作用是完成 board_init_f 没有初始化的外设，以及一些后续工作。也会执行 init_sequence_r 表中的函数，在函数最后会调用 run_main_loop 函数。

### run_main_loop函数

在文件 common/board_r.c 中，此函数主要是在死循环中调用 main_loop() 函数

## uboot命令执行

### main_loop

在文件 common/main.c 中，在函数中主要执行 autoboot_command 和 cli_loop 函数。

#### autoboot_command

在 common/autoboot.c 中，其中会通过 Abortboot 函数判断在控制台打印的倒计时结束之前是否有按键按下，如果存在按键按下时，会进入uboot系统，不执行 bootcmd → 直接进入 cli_loop。反之会返回bootcmd启动内核。

"Hit any key to stop autoboot" → "按任意键中止自动启动"

#### cli_loop

在文件 common/cli.c 中，主要作用是执行相应的命令操作，在 cli_simple_loop 函数存在一个死循环，用于接收控制台的command，并处理相应的命令工作。

## bootz启动linux内核

### bootm和bootz的关系？

bootm 用于启动 uImage，bootz 用于启动 zImage；bootz 在实现上复用了 bootm 的核心启动流程，并增加了对 zImage 的解压处理。

bootz：用于启动 zImage
bootm：用于启动 uImage

### bootz_start()

获取内核镜像zImage 保存在images

获取设备树，设备树首地址保存在images

### bootm_disable_interrupts()

禁用中断

### do_bootm_states()

在文件 common/bootm.c 中，函数会根据不同的 BOOT 状态执行不同的代码段。

**do_bootm_linux** 函数在 arch/arm/lib/bootm.c 文件中，次函数就是最终启动 Linux 内核的函数。

