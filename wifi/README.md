# wifi驱动

## wifi驱动 - 屏蔽自带驱动

正点原子的 I.MX6U-ALPHA 开发板目前支持两种接口的 WIFI：USB 和 SDIO，其中 USBWIFI 使用的芯片为 RTL8188EUS 或 RTL8188CUS，SDIO 接口的 WIFI 使用芯片为 RTL8189FS，也叫做 RTL8189FTV。这两个都是 realtek 公司出品的 WIFI 芯片。WIFI 驱动不需要我们编写，因为 realtek 公司提供了 WIFI 驱动源码，因此我们只需要将 WIFI 驱动源码添加到 Linux 内核中，然后通过图形化界面配置，选择将其编译成模块即可。

linux 内核已经自带了 RTL8192CU/8188CUS 驱动，但是经过测试，linux内核自带的驱动不稳定，最好使用alientek提供的 rtl8192CU 驱动。在编译之前要先将内核自带的驱动屏蔽掉，否则可能导致编译出错

打开drivers/net/wireless/rtlwifi/Kconfig，注释掉如下内容：

```conf
# config RTL8192CU
# 	tristate "Realtek RTL8192CU/RTL8188CU USB Wireless Network Adapter"
# 	depends on USB
# 	select RTLWIFI
# 	select RTLWIFI_USB
# 	select RTL8192C_COMMON
# 	---help---
# 	This is the driver for Realtek RTL8192CU/RTL8188CU 802.11n USB
# 	wireless network adapters.

# 	If you choose to build it as a module, it will be called rtl8192cu
```

Makefile，注释掉：

```makefile
obj-$(CONFIG_RTL8192CU)		+= rtl8192cu/
```

## wifi驱动 - 添加realtek驱动

将 realtek 整个目录拷贝到 ubuntu 下 Linux 内核源码中的 drivers/net/wireless 目录下，此目录下存放着所有 WIFI 驱动文件

打开 drivers/net/wireless/Kconfig，在里面加入下面这一行内容：

```conf
source "drivers/net/wireless/realtek/Kconfig"
```

打开 drivers/net/wireless/Makefile，在里面加入下面一行内容：

```makefile
obj-y += realtek/
```

1、配置 USB 支持设备
配置路径如下：

```
-> Device Drivers
-> <*> USB support
-> <*> Support for Host-side USB
-> <*> EHCI HCD (USB 2.0) support
-> <*> OHCI HCD (USB 1.1) support
-> <*> ChipIdea Highspeed Dual Role Controller
-> [*] ChipIdea device controller
-> [*] ChipIdea host controller
```

2、配置支持 WIFI 设备
配置路径如下：

```
-> Device Drivers
-> [*] Network device support
-> [*] Wireless LAN
-> <*> IEEE 802.11 for Host AP (Prism2/2.5/3 and WEP/TKIP/CCMP)
-> [*] Support downloading firmware images with Host AP driver
-> [*] Support for non-volatile firmware download
```

3、配置支持 IEEE 802.11
配置路径如下：

```
-> Networking support
-> -*- Wireless
-> [*] cfg80211 wireless extensions compatibility
-> <*> Generic IEEE 802.11 Networking Stack (mac80211)
```

4、wifi驱动编译为模块

```
-> Device Drivers
-> Network device support (NETDEVICES [=y])
-> Wireless LAN (WLAN [=y])
-> Realtek wifi (REALTEK_WIFI [=m])
-> <M> rtl8189ftv sdio wifi
-> <M> rtl8188eus usb wifi
-> <M> Realtek 8192C USB WiFi
```

5、modprobe加载驱动

输入“ifconfig -a”命令，查看 wlanX(X=0….n)网卡是否存在，一般都是 wlan0，除非板子上有多个 WIFI 模块在工作

## wifi驱动 - 测试

```bash
modprobe 8188eu.ko
ifconfig wlan0 up
iwlist wlan0 scan
```

在开发板根文件系统的/etc 目录下创建一个名为“wpa_supplicant.conf”的配置文件，此文件用于配置要连接的 WIFI 热点以及 WIFI 密码

```conf
/etc # cat wpa_supplicant.conf 
ctrl_interface=/var/run/wpa_supplicant
ap_scan=1
network={
 ssid="skrp"
 psk="skrptskrpt"
}
```

注意，wpa_supplicant.conf 文件对于格式要求比较严格，“=”前后一定不能有空格，也不要用 TAB 键来缩进，比如第 4 行和 5 行的缩进应该采用空格，否则的话会出现 wpa_supplicant.conf文件解析错误

wpa_supplicant.conf 文 件 编 写 好 以 后 再 在 开 发 板 根 文 件 系 统 下 创 建 一 个 “/var/run/wpa_supplicant”目录，wpa_supplicant 工具要用到此目录

```bash
wpa_supplicant -D wext -c /etc/wpa_supplicant.conf -i wlan0 &
```

```bash
/var/run # wpa_supplicant -D wext -c /etc/wpa_supplicant.conf -i wlan0 &
/var/run # Successfully initialized wpa_supplicant
rfkill: Cannot get wiphy informatIPv6: ADDRCONF(NETDEV_UP): wlan0: link is not ready
ion
RTL871X: set bssid:00:00:00:00:00:00
ioctl[SIOCSIWAP]: Operation not pRTL871X: set ssid [gũsQÿJꪍº«󼢆|T𭡳ɚ񜖈´6] fw_state=0x00000008
ermitted
ioctl[SIOCGIWSCAN]: Resource temporarily unavailable
ioctl[SIOCGIWSCAN]: Resource temporarily unavailable
RTL871X: indicate disassoc
wlan0: Trying to associate with e2:67:ac:14:85:be (SSID='skrp' frRTL871X: set ssid [skrp] fw_state=0x00000008
eq=2412 MHz)
Failed to add suppoRTL871X: set bssid:e2:67:ac:14:85:be
rted operating classes IE
RTL871X: start auth
RTL871X: auth success, start assoc
RTL871X: assoc success
RTL871X: recv eapol packet
IPv6: ADDRCONF(NETDEV_CHANGE): wlan0: link becomes ready
wlan0: Associated with e2:67:ac:14:85:be
RsvdPageNum: 8
RTL871X: send eapol packet
RTL871X: recv eapol packet
RTL871X: send eapol packet
RTL871X: set pairwise key camid:4, addr:e2:67:ac:14:85:be, kid:0, type:AES
wlan0: WPA: Key negotiation completed with e2:67:ac:14:85:be [PTKRTL871X: set group key camid:5, addr:e2:67:ac:14:85:be, kid:1, type:AES
=CCMP GTK=CCMP]
wlan0: CTRL-EVENT-CONNECTED - Connection to e2:67:ac:14:85:be completed [id=0 id_str=]
```

设置wlan0的ip地址，使用udhcpc从路由器申请ip地址

```bash
/var/run # udhcpc -i wlan0
udhcpc: started, v1.29.0
Setting IP address 0.0.0.0 on wlan0
udhcpc: sending discover
udhcpc: sending select for 192.168.194.70
udhcpc: lease of 192.168.194.70 obtained, lease time 3599
Setting IP address 192.168.194.70 on wlan0
Deleting routers
route: SIOCDELRT: No such process
Adding router 192.168.194.8
Recreating /etc/resolv.conf
 Adding DNS server 192.168.194.8
```

```bash
/var/run # ifconfig wlan0
wlan0     Link encap:Ethernet  HWaddr 00:0F:00:00:0E:29  
          inet addr:192.168.194.70  Bcast:192.168.194.255  Mask:255.255.255.0
          inet6 addr: fe80::20f:ff:fe00:e29/64 Scope:Link
          inet6 addr: 240e:428:542b:bfba:20f:ff:fe00:e29/64 Scope:Global
          UP BROADCAST RUNNING MULTICAST  MTU:1500  Metric:1
          RX packets:39 errors:0 dropped:1 overruns:0 frame:0
          TX packets:22 errors:0 dropped:4 overruns:0 carrier:0
          collisions:0 txqueuelen:1000 
          RX bytes:5242 (5.1 KiB)  TX bytes:3133 (3.0 KiB)
```

```bash
/var/run # ping -I 192.168.194.70 www.baidu.com
PING www.baidu.com (220.181.111.232) from 192.168.194.70: 56 data bytes
64 bytes from 220.181.111.232: seq=0 ttl=52 time=15.516 ms
64 bytes from 220.181.111.232: seq=1 ttl=52 time=29.941 ms
64 bytes from 220.181.111.232: seq=2 ttl=52 time=51.743 ms
64 bytes from 220.181.111.232: seq=3 ttl=52 time=49.069 ms
64 bytes from 220.181.111.232: seq=4 ttl=52 time=48.463 ms
64 bytes from 220.181.111.232: seq=5 ttl=52 time=49.315 ms
^C
--- www.baidu.com ping statistics ---
6 packets transmitted, 6 packets received, 0% packet loss
round-trip min/avg/max = 15.516/40.674/51.743 ms
```
