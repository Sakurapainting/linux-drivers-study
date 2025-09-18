# 串口驱动

## 串口驱动 - 驱动框架

- 两个重要的结构体 `uart_port` 和 `uart_driver` 
-  `uart_driver` 需要驱动编写人员实现，并使用 `uart_register` 注册到内核，卸载驱动的时候，使用 `uart_unregister_driver` 卸载
-  `uart_port` 用于描述一个具体的串口端口，驱动编写人员需要实现 `uart_port` ，然后使用 `uart_add_one_port` 函数向内核添加一个uart端口，卸载的时候 `uart_remove_one_port` 卸载
-  `uart_port` 里面有个重要的成员变量 `uart_ops` ，此结构体包含了针对uart端口进行的所有操作，需要驱动编写人员实现
- nxp官方串口驱动入口函数是 `imx_serial_init` ，此函数会调用 `uart_register_driver` 向内核注册 `uart_driver` ，为 `imx_reg` 
- imx6ull的串口名字叫 `/dev/ttymxcX` ，由 `uart_driver` 的 `dev_name` 确定
- 然后是 `uart_port` 的处理，nxp自定义了一个 `imx_port` ，里面包含 `uart_port` 
-  `uart_ops` 是 `imx_pops` 
- 串口接收终端处理函数 `imx_rxint` 获取到串口接收到的数据，然后使用 `tty_insert_flip_char` 将其放到tty里面

## 串口驱动 - 设备树

- 如果只是简单的串口通信，不需要硬件流控，把硬件流控信号引脚去除（RTS/CTS）

```c
// MX6UL_PAD_UART3_RX_DATA__UART2_DCE_RTS	0x1b0b1
// MX6UL_PAD_UART3_TX_DATA__UART2_DCE_CTS	0x1b0b1
```

- 在 `imx6ul-pinfunc.h` ，搜索 `UART3` 相关，找到引脚对应的宏

## 串口驱动 - 移植minicom

### 移植编译 `ncurses-6.0` 

- 解压 `tar -vxzf` 
- 创建一个名为 `ncurses` 的文件夹，在 `/home/you/linux/tool/ncurses` 
- 配置：

```bash
you@you-virtual-machine:~/linux/tool/ncurses-6.0$ ./configure --prefix=/home/you/linux/tool/ncurses --host=arm-linux-gnueabihf --target=arm-linux-gnueabihf --with-shared --without-profile --disable-stripping --without-progs --with-manpages --without-tests
```

- 然后进入源码目录，make编译

```bash
make
```

- 安装（配置的时候设置 `--prefix` 参数，也就是最终安装的位置，如果不设置的话就是默认安装位置）

```bash
make install
```

- 将编译出来的库放到开发板里： `/lib` 放到开发板的 `/usr/lib` ， `/share` 和 `/include` 同理，如果开发板下没有该路径，手动创建即可
- 在 `/etc/profile` 中，添加：

```c
#!/bin/sh
LD_LIBRARY_PATH=/lib:/usr/lib:$LD_LIBRARY_PATH
export LD_LIBRARY_PATH

export TERM=vt100
export TERMINFO=/usr/share/terminfo
```

### 移植编译 `minicom-2.7.1` 

- 解压 `tar -vxzf` 
- 创建一个名为 `minicom` 的文件夹，在 `/home/you/linux/tool/minicom` 
- 配置：

```bash
you@you-virtual-machine:~/linux/tool/minicom-2.7.1$ ./configure CC=arm-linux-gnueabihf-gcc --prefix=/home/you/linux/tool/minicom --host=arm-linux-gnueabihf CPPFLAGS=-I/home/you/linux/tool/ncurses/include LDFLAGS=-L/home/you/linux/tool/ncurses/lib -enable-cfg-dir=/etc/minicom
```

- 然后进入源码目录，make编译
- 安装
- 把 `minicom` 目录下的 `bin` 下所有文件拷贝到开发板的 `/usr/bin` 中
- 检验是否移植成功

```bash
minicom -v
```

- 开发板跳帽接到 `U3` 和 `COM3` 之间，插上232串口，终端执行：

```bash
minicom -s
```

- 选择 `Serial port setup` 
- 设置 `/dev/ttymxc2` `/var/lock` ，C和D是空的，E是 `115200 8N1` ，F和G（硬件流控、软件流控）都是 `NO` 