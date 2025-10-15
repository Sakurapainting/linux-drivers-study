# SPI驱动

## SPI驱动 - SPI控制器驱动简析

- 主机控制器驱动：SOC的SPI外设驱动，半导体原厂编写 `spi-imx.c` 
  - SPI控制器驱动的核心就是 `spi_master` 的构建，里面有如何通过SPI控制器与SPI外设进行通信的函数，这些函数由原厂编写
    - `spi_master` -> `transfer`
    - `spi_master` -> `transfer_one_message`
    - `spi_imx_data` -> `bitbang` -> `master`, `setup_transfer`, `txrx_bufs`
    - `spi_master` ，一般需要申请 `spi_alloc_master` （释放这一段内存就是 `spi_master_put` ），需要初始化 `spi_master` ，最终使用函数 `spi_register_master` 注册
    - `spi_imx_setupxfer` -> `spi_imx` -> `rx` (`tx`) = `spi_imx_buf_rx_u8` (`spi_imx_buf_tx_u8`) (最终的SPI接收(发送)函数)
    - `spi_imx_setupxfer` -> `spi_imx` -> `devtype_data` -> `config(spi_imx, &config)` -> `mx51_ecspi_config` (配置6ull的SPI控制器寄存器) 
    - ……
  - 经过一系列复杂操作，最终目的是设置 `spi_master` 的 `transfer_one_message` 函数为 `spi_imx_buf_tx_u8` 
  - SPI接收：SPI通过中断接收，中断处理函数为 `spi_imx_isr` ，此函数会调用 `spi_imx` -> `rx(spi_imx)` 函数来完成具体的接收过程
- 设备驱动：具体的SPI芯片驱动，比如ICM20608。
- `spi_device` （有设备树，不用关心）：每个 `spi_device` 下都有一个 `spi_master` ，每个SPI设备，肯定挂载到了一个SPI控制器，比如ICM20608挂载到了6ull的ECSPI3接口上
- `spi_driver` ：申请或者定义一个 `spi_driver` ，然后初始化 `spi_driver` 中的各个成员变量，当设备驱动匹配后， `spi_driver` 下的probe执行
- `spi_driver` 初始化成功后，向内核注册，函数为 `spi_register_driver` ，当注销驱动的时候需要 `spi_unregister_driver` 
  
## SPI驱动 - 驱动

- 查原理图、手册：
- ECSPI3_SCLK   ->  UART2_RXD
- ECSPI3_MOSI   ->  UART2_CTS
- ECSPI3_SS0    ->  UART2_TXD
- ECSPI3_MISO   ->  UART2_RTS
- 片选信号不作为硬件片选，而是作为普通的GPIO，我们在程序里面自行控制片选引脚
- 在 `imx6ul-pinfunc.h` （不在`imx6ull-pinfunc.h` 中，6ull只比6ul多了一些6ul没有的）里找宏定义

```c
/*you spi*/
		pinctrl_ecspi3: ecspi3grp{
			fsl,pins = <
				MX6UL_PAD_UART2_TX_DATA__GPIO1_IO20	  	0x10b0
				MX6UL_PAD_UART2_RX_DATA__ECSPI3_SCLK	0x10b1
				MX6UL_PAD_UART2_CTS_B__ECSPI3_MOSI		0x10b1
				MX6UL_PAD_UART2_RTS_B__ECSPI3_MISO		0x10b1
			>;
		};
```

**然后找有没有引脚冲突：搜索 `UART2_TX` , `UART2_RX` , `UART2_CTS` , `UART2_RTS` , `GPIO1_IO20` , 把其他地方用不到的行注释掉**

- 在ECSPI3节点下创建 `icm20608` 子节点，其中属性配置查询 `${sdk}/Documentation/devicetree/bindings/spi/fsl-imx-cspi.txt` ， `cs-gpios` 是nxp的软件片选

```c
	// cs-gpios = <&gpio1 20 GPIO_ACTIVE_LOW>;		// 硬件片选引脚
	cs-gpio = <&gpio1 20 GPIO_ACTIVE_LOW>;			// 软件片选引脚
```

- SPI总线整体方案，以及SPI子节点的文档查询在 `${sdk}/Documentation/devicetree/bindings/spi/spi-bus.txt` 

- 初始化icm20608芯片，然后从里面读取原始数据，此过程需要用到如何使用linux内的SPI驱动API来读写ICM20608
- 用到两个重要的结构体： `spi_transfer` 和 `spi_message` 
- `spi_transfer` 用来构建收发数据内容
- 构建 `spi_transfer` ，然后将其打包到 `spi_message` 里面，需要使用 `spi_message_init` 初始化 `spi_message` 然后再使用 `spi_message_add_tail` 将 `spi_transfer` 添加到 `spi_message` 里面，最终使用 `spi_sync` 和 `spi_async` 来发送。

## Linux 内核 自动管理的GPIO片选/SPI框架管理的片选

参考icm20608_cs-gpios.c，和 23_spi_cs-gpios.dts，且修改如下设备树

```c
	cs-gpios = <&gpio1 20 GPIO_ACTIVE_LOW>;		// 硬件片选引脚
	// cs-gpio = <&gpio1 20 GPIO_ACTIVE_LOW>;			// 软件片选引脚
```

