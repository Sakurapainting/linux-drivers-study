# 网络驱动

## 网络驱动 - 概念

### MII接口

用于以太网MAC连接PHY芯片

MII 接口一共有 16 根信号线，含义如下：
- TX_CLK：发送时钟，如果网速为 100M 的话时钟频率为 25MHz，10M 网速的话时钟频率为 2.5MHz，此时钟由 PHY 产生并发送给 MAC。
- TX_EN：发送使能信号。
- TX_ER：发送错误信号，高电平有效，表示 TX_ER 有效期内传输的数据无效。10Mpbs 网速下 TX_ER 不起作用。
- TXD[3:0]：发送数据信号线，一共 4 根。
- RXD[3:0]：接收数据信号线，一共 4 根。
- RX_CLK：接收时钟信号，如果网速为 100M 的话时钟频率为 25MHz，10M 网速的话时钟频率为 2.5MHz，RX_CLK 也是由 PHY 产生的。
- RX_ER：接收错误信号，高电平有效，表示 RX_ER 有效期内传输的数据无效。10Mpbs 网速下 RX_ER 不起作用。
- RX_DV：接收数据有效，作用类似 TX_EN。
- CRS：载波侦听信号。
- COL：冲突检测信号。

MII 接口的缺点就是所需信号线太多，这还没有算 MDIO 和 MDC 这两根管理接口的数据线，因此 MII 接口使用已经越来越少了。

### RMII接口

精简MII接口

- TX_EN：发送使能信号。
- TXD[1:0]：发送数据信号线，一共 2 根。
- RXD[1:0]：接收数据信号线，一共 2 根。
- CRS_DV：相当于 MII 接口中的 RX_DV 和 CRS 这两个信号的混合。
- EF_CLK：参考时钟，由外部时钟源提供， 频率为 50MHz。这里与 MII 不同，MII 的接收和发送时钟是独立分开的，而且都是由 PHY 芯片提供的。

除了 MII 和 RMII 以外，还有其他接口，比如 GMII、RGMII、SMII、SMII 等，关于其他接口基本都是大同小异的，这里就不做讲解了。正点原子 ALPAH 开发板上的两个网口都是采用 RMII 接口来连接 MAC 与外部 PHY 芯片。

### MDIO接口

管理数据输入输出接口，是一个简单的两线串行接口，一根 MDIO 数据线，一根 MDC 时钟线。驱动程序可以通过 MDIO 和MDC 这两根线访问 PHY 芯片的任意一个寄存器。MDIO 接口支持多达 32 个 PHY。同一时刻
内只能对一个 PHY 进行操作，那么如何区分这 32 个 PHY 芯片呢？和 IIC 一样，使用器件地址即可。同一 MDIO 接口下的所有 PHY 芯片，其器件地址不能冲突，必须保证唯一，具体器件地址值要查阅相应的 PHY 数据手册。
因此，MAC 和外部 PHY 芯片进行连接的时候主要是 MII/RMII 和 MDIO 接口，另外可能还需要复位、中断等其他引脚。

### RJ45接口

RJ45 座要与 PHY 芯片连接在一起，但是中间需要一个网络变压器，网络变压器用于隔离以及滤波等，网络变压器也是一个芯片。

但是现在很多 RJ45 座子内部已经集成了网络变压器，比如正点原子 ALPHA 开发板所使用的 HR911105A 就是内置网络变压器的 RJ45 座。内置网络变压器的 RJ45 座和不内置的引脚一样，但是一般不内置的 RJ45 座会短一点。

RJ45 座子上一般有两个灯，一个黄色(橙色)，一个绿色，绿色亮的话表示网络连接正常，黄色闪烁的话说明当前正在进行网络通信。这两个灯由 PHY 芯片控制，PHY 芯片会有两个引脚来连接 RJ45 座上的这两个灯。内部 MAC+外部 PHY+RJ45 座(内置网络变压器)就组成了一个完整的嵌入式网络接口硬件。

## 网络驱动 - 框架

### net_device结构体

net_device 结构体定义在 include/linux/netdevice.h

这是 Linux 网络驱动中最核心的结构体，代表一个网络设备。重要成员如下：

#### 基本信息成员
- `char name[IFNAMSIZ]`：网络设备名字，比如 eth0、wlan0 等
- `unsigned long state`：网络设备状态
- `struct net_device *next`：指向下一个 net_device 结构体
- `unsigned int mtu`：最大传输单元，以太网默认为 1500 字节
- `unsigned char dev_addr[MAX_ADDR_LEN]`：MAC 地址
- `unsigned char addr_len`：MAC 地址长度

#### 硬件信息成员
- `unsigned long mem_start`：设备内存起始地址
- `unsigned long mem_end`：设备内存结束地址
- `unsigned long base_addr`：设备 I/O 基地址
- `unsigned int irq`：设备使用的中断号

#### 接口相关成员
- `struct net_device_ops *netdev_ops`：网络设备操作函数集合，非常重要！
- `struct ethtool_ops *ethtool_ops`：ethtool 相关操作函数集合
- `struct header_ops *header_ops`：头部相关操作函数集合
- `unsigned int flags`：网络接口标志，如 IFF_UP、IFF_BROADCAST 等

#### 统计信息成员
- `struct net_device_stats *stats`：网络设备统计信息，包含发送/接收的包数、字节数、错误数等

#### 其他重要成员
- `void *priv`：私有数据指针，驱动可以用它保存自己的私有数据
- `unsigned char *dev_addr`：设备硬件地址(MAC 地址)
- `struct device dev`：设备模型中的 device 结构体

### net_device_ops结构体

net_device_ops 结构体定义了网络设备的各种操作函数，是网络驱动的核心操作接口。重要成员包括：

- `int (*ndo_open)(struct net_device *dev)`：打开网络设备，分配资源、使能硬件等
- `int (*ndo_stop)(struct net_device *dev)`：关闭网络设备，释放资源、禁用硬件等
- `netdev_tx_t (*ndo_start_xmit)(struct sk_buff *skb, struct net_device *dev)`：发送数据包，最重要的函数之一
- `void (*ndo_set_rx_mode)(struct net_device *dev)`：设置接收模式，如混杂模式、多播等
- `int (*ndo_set_mac_address)(struct net_device *dev, void *addr)`：设置 MAC 地址
- `int (*ndo_validate_addr)(struct net_device *dev)`：验证 MAC 地址是否有效
- `int (*ndo_do_ioctl)(struct net_device *dev, struct ifreq *ifr, int cmd)`：执行 I/O 控制命令
- `struct net_device_stats* (*ndo_get_stats)(struct net_device *dev)`：获取网络统计信息
- `int (*ndo_change_mtu)(struct net_device *dev, int new_mtu)`：修改 MTU 值

### sk_buff结构体

网络是分层的，对于应用层而言不用关心具体的底层是如何工作的，只需要按照协议将要发送或接收的数据打包好即可。打包好以后都通过 dev_queue_xmit 函数将数据发送出去，接收数据的话使用 netif_rx 函数即可

**skb**：要发送的数据，这是一个 sk_buff 结构体指针，sk_buff 是 Linux 网络驱动中一个非常重要的结构体，网络数据就是以 sk_buff 保存的，各个协议层在 sk_buff 中添加自己的协议头，最终由底层驱动将 sk_buff 中的数据发送出去。网络数据的接收过程恰好相反，网络底层驱动将
接收到的原始数据打包成 sk_buff，然后发送给上层协议，上层会取掉相应的头部，然后将最终的数据发送给用户。

返回值：0 发送成功，负值 发送失败。

sk_buff(socket buffer)是 Linux 网络子系统中用于描述数据包的核心结构体。重要成员包括：

- `struct sk_buff *next`：下一个 sk_buff
- `struct sk_buff *prev`：前一个 sk_buff
- `struct sock *sk`：关联的 socket
- `ktime_t tstamp`：时间戳
- `struct net_device *dev`：关联的网络设备
- `unsigned int len`：实际数据长度
- `unsigned int data_len`：数据区长度
- `__u16 mac_len`：MAC 头长度
- `__u16 protocol`：协议类型
- `unsigned char *head`：缓冲区头指针
- `unsigned char *data`：实际数据头指针
- `unsigned char *tail`：实际数据尾指针
- `unsigned char *end`：缓冲区尾指针

常用操作函数：
- `alloc_skb()`：分配 sk_buff
- `dev_kfree_skb()`：释放 sk_buff
- `skb_put()`：在数据尾部添加数据
- `skb_push()`：在数据头部添加数据
- `skb_pull()`：从数据头部移除数据
- `skb_reserve()`：预留头部空间

1、分配 sk_buff
要 使 用 sk_buff 必 须 先分 配， 首 先来 看一 下 alloc_skb 这个 函数 ， 此函 数定 义 在 include/linux/skbuff.h 中

```c
static inline struct sk_buff *alloc_skb(unsigned int size, gfp_t priority)
```

函数参数和返回值含义如下：
- size：要分配的大小，也就是 skb 数据段大小。
- priority：为 GFP MASK 宏，比如 GFP_KERNEL、GFP_ATOMIC 等。
- 返回值：分配成功的话就返回申请到的 sk_buff 首地址，失败的话就返回 NULL。

在网络设备驱动中常常使用 netdev_alloc_skb 来为某个设备申请一个用于接收的 skb_buff，此函数也定义在 include/linux/skbuff.h 中

```c
static inline struct sk_buff *netdev_alloc_skb(struct net_device *dev, unsigned int length)
```

函数参数和返回值含义如下：
- dev：要给哪个设备分配 sk_buff。
- length：要分配的大小。
- 返回值：分配成功的话就返回申请到的 sk_buff 首地址，失败的话就返回 NULL。

2、释放 sk_buff
当使用完成以后就要释放掉 sk_buff，释放函数 可以使用 kfree_skb，函数定义在 include/linux/skbuff.c 中

```c
void kfree_skb(struct sk_buff *skb)
```

函数参数和返回值含义如下：
- skb：要释放的 sk_buff。
- 返回值：无。

对于网络设备而言最好使用如下所示释放函数：

```c
void dev_kfree_skb (struct sk_buff *skb)
```

3、skb_put、skb_push、sbk_pull 和 skb_reserve
这四个函数用于变更 sk_buff，先来看一下 skb_put 函数，此函数用于在尾部扩展 skb_buff的数据区，也就将 skb_buff 的 tail 后移 n 个字节，从而导致 skb_buff 的 len 增加 n 个字节

```c
unsigned char *skb_put(struct sk_buff *skb, unsigned int len)
```

函数参数和返回值含义如下：
- skb：要操作的 sk_buff。
- len：要增加多少个字节。
- 返回值：扩展出来的那一段数据区首地址。

skb_push 函数用于在头部扩展 skb_buff 的数据区

```c
unsigned char *skb_push(struct sk_buff *skb, unsigned int len)
```

函数参数和返回值含义如下：
- skb：要操作的 sk_buff。
- len：要增加多少个字节。
- 返回值：扩展完成以后新的数据区首地址。

sbk_pull 函数用于从 sk_buff 的数据区起始位置删除数据，函数原型如下所示：

```c
unsigned char *skb_pull(struct sk_buff *skb, unsigned int len)
```
函数参数和返回值含义如下：
- skb：要操作的 sk_buff。
- len：要删除的字节数。
- 返回值：删除以后新的数据区首地址。

sbk_reserve 函数用于调整缓冲区的头部大小，方法很简单将 skb_buff 的 data 和 tail 同时后移 n 个字节即可，函数原型如下所示：

```c
static inline void skb_reserve(struct sk_buff *skb, int len)
```

函数参数和返回值含义如下：
- skb：要操作的 sk_buff。
- len：要增加的缓冲区头部大小。
- 返回值：无。

### NAPI

Linux 在这两个处理方式的基础上提出
了另外一种网络数据接收的处理方法：NAPI(New API)，NAPI 是一种高效的网络处理技术。NAPI 的核心思想就是不全部采用中断来读取网络数据，而是采用中断来唤醒数据接收服务程序，在接收服务程序中采用 POLL 的方法来轮询处理数据。这种方法的好处就是可以提高短数据包的接收效率，减少中断处理的时间。目前 NAPI 已经在 Linux 的网络驱动中得到了大量的应用，NXP 官方编写的网络驱动都是采用的 NAPI 机制。

1、初始化 NAPI
首先要初始化一个 napi_struct 实例，使用 netif_napi_add 函数，此函数定义在 net/core/dev.c

```c
void netif_napi_add(struct net_device *dev, struct napi_struct *napi, int (*poll)(struct napi_struct *, int), int weight)
```

函数参数和返回值含义如下：
- dev：每个 NAPI 必须关联一个网络设备，此参数指定 NAPI 要关联的网络设备。
- napi：要初始化的 NAPI 实例。
- poll：NAPI 所使用的轮询函数，非常重要，一般在此轮询函数中完成网络数据接收的工作。
- weight：NAPI 默认权重(weight)，一般为 NAPI_POLL_WEIGHT。
- 返回值：无。

……

省略其他函数

## 网络驱动 - 设备树

NXP 的 I.MX 系 列 SOC 网 络 绑 定 文 档 为
Documentation/devicetree/bindings/net/fsl-fec.txt

必要属性

```
compatible：这个肯定是必须的，一般是“fsl<soc>-fec”，比如 I.MX6ULL 的 compatible 属性就是"fsl,imx6ul-fec",和"fsl,imx6q-fec"。
reg：SOC 网络外设寄存器地址范围。
interrupts：网络中断。
phy-mode：网络所使用的 PHY 接口模式，是 MII 还是 RMII。
```

## 网络驱动 - fec_probe

对于 I.MX6ULL 而言网络驱动主要分两部分：I.MX6ULL 网络外设驱动以及 PHY 芯片驱动，网络外设驱动是 NXP 编写的，PHY 芯片有通用驱动文件，有些 PHY 芯片厂商还会针对自己的芯片编写对应的 PHY 驱动。总体来说，SOC 内置网络 MAC+外置 PHY 芯片这种方案我们是不需要编写什么驱动的，基本可以直接使用。但是为了学习，我们还是要简单分析一下具体的网络驱动编写过程。

以这个为例
drivers/net/ethernet/freescale/fec_main.c

- 使用 fec_enet_get_queue_num 函数来获取设备树中的“fsl,num-tx-queues”和“fsl,num-rx-queues”这两个属性值，也就是发送队列和接收队列的大小，设备树中这两个属性都设置为 1。
- 使用 alloc_etherdev_mqs 函数申请 net_device。
- 获取 net_device 中私有数据内存首地址，net_device 中的私有数据用来存放I.MX6ULL 网络设备结构体，此结构体为 fec_enet_private。
- 获取设备树中 I.MX6ULL 网络外设(ENET)相关寄存器起始地址，ENET1 的寄存器起始地址 0X02188000，ENET2 的寄存器起始地址 0X20B4000。
- 对第 45 行获取到的地址做虚拟地址转换，转换后的 ENET 虚拟寄存器起始地址保存在 fep 的 hwp 成员中。
- 使用 fec_enet_of_parse_stop_mode 函数解析设备树中关于 ENET 的停止模式属性值，属性名字为“stop-mode”，我们没有用到。
- 从设备树查找“fsl,magic-packet”属性是否存在，如果存在的话就说明有魔术包，有魔术包的话就将 fep 的 wol_flag 成员与 FEC_WOL_HAS_MAGIC_PACKET 进行或运算，也就是在 wol_flag 中做登记，登记支持魔术包。（魔术包（Magic Packet）是一种特殊格式的网络数据包，核心作用是远程唤醒处于休眠 / 关机状态的网络设备。它无需设备正常响应网络，只需网卡保持低功耗监听，收到匹配的魔术包后即可触发设备开机。）
- 获取“phy-handle”属性的值，phy-handle 属性指定了 I.MX6ULL 网络外设所对应获取 PHY 的设备节点
- ethphy0 和 ethphy1 都是与 MDIO 相关的，而 MDIO 接口是配置 PHY 芯片的，通过一个 MDIO 接口可以配置多个 PHY 芯片，不同的 PHY 芯片通过不同的地址进行区别。正点原子 ALPHA 开发板中 ENET 的 PHY 地址为 0X00，ENET2 的 PHY 地址为 0X01。
- 并且 ethphy0 和 ethphy1 节点中的 reg 属性也是 PHY 地址，如果我们要更换其他的网络PHY 芯片，第一步就是要修改设备树中的 PHY 地址。
- 获取 PHY 工作模式，函数 of_get_phy_mode 会读取属性 phy-mode 的值，” phy-mode”中保存了 PHY 的工作方式，即 PHY 是 RMII 还是 MII，IMX6ULL 中的 PHY 工作在RMII 模式
- 分别获取时钟 ipg、ahb、enet_out、enet_clk_ref 和 ptp，对应结构体 fec_enet_private 有如下成员函数

```c
struct clk *clk_ipg;
struct clk *clk_ahb;
struct clk *clk_ref;
struct clk *clk_enet_out;
struct clk *clk_ptp;
```

- 使能时钟。
- 调用函数 fec_reset_phy 复位 PHY。
- 调用函数 fec_enet_init()初始化 enet，此函数会分配队列、申请 dma、设置 MAC地址，初始化 net_device 的 netdev_ops 和 ethtool_ops 成员
- net_device_ops 是驱动核心功能接口，负责数据包收发、设备启停等基础网络操作，直接与内核协议栈交互。
- ethtool_ops 是用户态配置接口，通过 ethtool 工具暴露硬件细节，方便用户查询 / 修改高级特性（如速率、WOL 等）。
- fec_enet_init 函数还会调用 netif_napi_add 来设置 poll 函数，说明 NXP 官方编写的此网络驱动是 NAPI 兼容驱动
- 通过 netif_napi_add 函数向网卡添加了一个 napi 示例，使用 NAPI驱动要提供一个 poll 函数来轮询处理接收数据
- 从设备树中获取中断号。
- 申请中断，中断处理函数为 fec_enet_interrupt
- 从设备树中获取属性“fsl,wakeup_irq”的值，也就是唤醒中断
- 初始化完成量 completion
- 函数 fec_enet_mii_init 完成 MII/RMII 接口的初始化
- mii_bus 下的 read 和 write 这两个成员变量分别是读/写 PHY 寄存器的操作函数，这里设置为 fec_enet_mdio_read 和 fec_enet_mdio_write，这两个函数就是 I.MX 系列 SOC 读写 PHY 内部寄存器的函数，读取或者配置 PHY 寄存器都会通过这两个 MDIO 总线函数完成。
- 从设备树中获取 mdio 节点，如果节点存在的话就会通过 of_mdiobus_register 或者 mdiobus_register 来向内核注册 MDIO 总线，如果采用设备树的话就使用 of_mdiobus_register 来注册 MDIO 总线，否则就使用 mdiobus_register 函数
- 先调用函数 netif_carrier_off 通知内核，先关闭链路，phylib 会打开。
- 调用函数 fec_enet_clk_enable 使能网络相关时钟。
- 调用函数 register_netdev 注册 net_device