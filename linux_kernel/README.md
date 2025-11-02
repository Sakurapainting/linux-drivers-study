# kernel启动流程

## 镜像文件

编译完成后，会生成 vmlinux、Image，zImage、uImage 文件

- vmlinux
vmlinux 是编译出来的最原始的内核文件，没有经过压缩，所以文件比较大。

- Image
Image 是 Linux 内核镜像文件，仅包含可执行的二进制数据。是使用 objcopy 取消掉 vmlinux 中的一些其他信息，比如符号表等，虽然也没有压缩，但是文件比vmlinux小了很多。

- zImage
zImage 是经过 gzip 压缩后的 Image，一般烧写的都是 zImage 镜像文件

- uImage
uImage 是老版本 uboot 专用的镜像文件，uImag 是在 zImage 前面加了一个长度为 64字节的“头”，这个头信息描述了该镜像文件的类型、加载位置、生成时间、大小等信息。

## vmlinux.lds

vmlinux.lds 是链接脚本，通过分析 kernel 顶层 Makefile 文件可知，镜像文件的打包是从 vmlinux.lds 链接脚本开始的，vmlinux.lds 文件位置在 arch/arm/kernel 目录下，在文件中使用了ENTRY(stext) 指定了入口 为 stext。

## 入口stext

在文件 arch/arm/kernel/head.S 中

- 确保CPU处于SVC模式
- 检查当前系统是否支持此CPU
- 验证atags或dtb的合法性
- 创建页表
- 将函数__mmap_switched的地址保存到 r13 寄存器中，最终会调用start_kernel 函数
- __enable_mmu，使能mmu，执行r13中的__mmap_switched -> start_kernel

## kernel初始化

start_kernel 函数通过调用众多的子函数来完成 Linux 启动之前的一些初始化工作

### kernel_thread

- 创建 **`kernel_init`** 进程，也就是 init 内核进程。init 进程的 PID 为 1。（init 进程一开始是内核进程(也就是运行在内核态)，后面 init 进程会在根文件系统中查找名为“init”这个程序，这个“init”程序处于用户态，通过运行这个“init”程序，init 进程就会实现从内核态到用户态的转变）
- 创建 kthreadd 内核进程，此内核进程的 PID 为 2。kthreadd 进程负责所有内核进程的调度和管理。

### cpu_startup_entry

进入 idle 进程，会调用 cpu_idle_loop，cpu_idle_loop 是个 while 循环，也就是 idle 进程代码。idle 进程的 PID 为 0，idle 进程叫做空闲进程。

## kernel_init

```c
do_basic_setup();        // ← 基础设备、驱动初始化
    
// ...

prepare_namespace();     // ← 挂载根文件系统
```