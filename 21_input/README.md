# INPUT子系统

## INPUT子系统驱动 - 驱动

- input 子系统 核心层 源码： `${KERNEL_SDK}/drivers/input/input.c`
- fops那套函数配置都不需要了，因为子系统已经帮我们做了。
- 这种框架，在驱动init里，把外设初始化之后，要注册这种框架

- 注册完 `input_dev` ：
  - evbit：是 struct input_dev 结构体中的一个位图（bitmap）字段，用于标识该输入设备能够产生哪些类型的事件。
  - EV_KEY：表示按键事件类型，是 Linux input 子系统预定义的事件类型之一。
  - __set_bit(EV_KEY, keyinputdev.inputdev->evbit)：在 evbit 位图中设置 EV_KEY 位，告诉内核这个输入设备支持按键事件。

```c
    __set_bit(EV_KEY, keyinputdev.inputdev->evbit); // 支持按键事件  evbit标识能支持什么类型的事件

```

- 不需要手动 `input_free_device` 

```c
// 注销 input_dev
    input_unregister_device(keyinputdev.inputdev);
    // input_free_device(keyinputdev.inputdev); // unregister 会自动 free
```

- `/dev/input` 下有 input设备
- 检查input设备是否注册能用：（hexdump会显示输入数据的原始值）

```bash
hexdump /dev/input/event1
```

## INPUT子系统驱动 - 应用理论

- 应用程序可以通过 `input_event` 来获取输入事件数据，比如按键值，这是一个结构体：

```c
struct timeval {
    // 宏 其实是 long
    __kernel_time_t tv_sec;         // seconds
    __kernel_suseconds_t tv_usec;   // microseconds
};

struct input_event {
    struct timeval time;
    __u16 type;
    __u16 code;
    __s32 value;
};
```

最终将 `input_event` 展开后：

```c
struct input_event {
    struct timeval {
        long tv_sec;        // 32位表示秒
        long tv_usec;       // 32位表示微秒
    };
    __u16 type;             // 16位的事件类型（如EV_KEY）
    __u16 code;             // 16位的事件码（对EV_KEY来说就是按键号，如KEY_0）
    __s32 value;            // 32位的值（对EV_KEY来说就是按键值：按下/抬起）
};
```

- 加载 `keyinput.ko` 后，检查input设备是否注册能用：（hexdump会显示输入数据的原始值）

```bash
hexdump /dev/input/event1
```

```bash
/lib/modules/4.1.15 # hexdump /dev/input/event1
Key key0 pressed
0000000 00d9 0000 f450 0008 0001 000b 0001 0000
0000010 00d9 0000 f450 0008 0000 0000 0000 0000
Key key0 released
0000020 00d9 0000 f019 000a 0001 000b 0000 0000
0000030 00d9 0000 f019 000a 0000 0000 0000 0000
Key key0 pressed
0000040 00dd 0000 71da 0005 0001 000b 0001 0000
0000050 00dd 0000 71da 0005 0000 0000 0000 0000
Key key0 released
0000060 00dd 0000 09ef 0008 0001 000b 0000 0000
0000070 00dd 0000 09ef 0008 0000 0000 0000 0000
```

- 第一列是编号，和结构体无关
- 接下来8个（32位）是tv_sec（秒）
- 接下来8个（32位）是tv_usec（微秒）
- 接下来4个（16位）是type
- 接下来4个（16位）是code
- 接下来8个（32位）是value
- type是事件类型，因为EV_KEY为1，EV_SYN为0，所以第一行表示EV_KEY，第二行表示EV_SYN
- value显示 `0001 0000` 而不是 `0000 0001` 的原因是，这个系统是小端序存储，低位字节在前，高位字节在后。

## INPUT子系统驱动 - 应用编写

- 按键驱动对应的文件是 `/dev/input/eventX` `(X=0,1,2,3...)` ，应用程序读取这个文件来得到按键信息，也就是按键有没有被按下。
- 通过 `/dev/input/eventX` 读到的信息是 `input_event` 结构体形式的
- fix : 按键号code应该是BTN而不是KEY。**KEY是模拟键盘的，BTN才是按钮**