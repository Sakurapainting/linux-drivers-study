# Regmap

## Regmap - i2c程序编写

### 头文件引用

```c
#include <linux/regmap.h>
```

### 修改设备结构体，添加 regmap 和 regmap_config

在 ap3216c_dev 结构体里添加

```c
struct regmap *regmap;
struct regmap_config regmap_config;
```

### probe

```c
/* regmap申请与初始化 */
    ap3216cdev.regmap_config.reg_bits = 8;
    ap3216cdev.regmap_config.val_bits = 8;
    ap3216cdev.regmap = regmap_init_i2c(client, &ap3216cdev.regmap_config);
```

和SPI不一样的是，不需要这一句：

```c
ap3216cdev.regmap_config.read_flag_mask = 0x80;
```

原因：
- I2C 协议：读写操作通过地址后的 R/W 位来区分
  - 设备地址的最低位：0 = 写，1 = 读
  - 协议本身就定义了读写标志
- SPI 协议：没有内置的读写标志位
  - 需要在寄存器地址中添加标志位来区分读写
  - 通常使用最高位（bit7）：1 = 读，0 = 写

### remove

```c
regmap_exit(ap3216cdev.regmap);
```

### write read reg 单个寄存器

```c
// 读取ap3216c 1个 寄存器
static int ap3216c_read_reg(struct ap3216c_dev* dev, u8 reg, unsigned int* val)
{
    return regmap_read(dev->regmap, reg, val);
}

// 写入ap3216c 1个 寄存器
static int ap3216c_write_reg(struct ap3216c_dev* dev, u8 reg, u8 val)
{
    return regmap_write(dev->regmap, reg, val);
}
```

### 其他

由于regmap_read的第三个参数要求是unsigned int* ，ap3216c驱动原本用u8的变量要改成unsigned int类型