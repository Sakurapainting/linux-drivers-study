# I2C 驱动

## I2C驱动 - I2C 适配器驱动简析（总线）

- I2C适配器在内核中使用 `i2c_adapter` 结构体，i2c适配器驱动（控制器）核心就是申请这个结构体，然后初始化，最后注册
- 初始化完成 `i2c_adapter` 以后，使用 `i2c_add_adapter` 或者 `i2c_add_numbered_adapter` 来向内核注册i2c控制器驱动
- 在 `i2c_adapter` 里面有个重要的成员变量， `i2c_algorithm` 包含了i2c控制器访问i2c设备的API接口函数，需要i2c适配器编写人员实现
- imx6ull 的 i2c适配器驱动 `i2c-imx.c` 
- nxp创建了一个 `imx_i2c_struct` 结构体，包含imx6u 的 i2c 相关属性，这个结构体里面就有 `i2c_adapter` 

```c
struct imx_i2c_struct {
    struct i2c_adapter adapter;
    struct clk *clk;
    void __iomem *base;
    wait_queue_head_t queue;
    unsigned long i2csr;
    unsigned int disable_delay;
    int stopped;
    unsigned int ifdr;  /* IMX_I2C_IFDR */
    unsigned int cur_clk;
    unsigned int bitrate;
    const struct imx_i2c_hwdata *hwdata;
    struct imx_i2c_dma *dma;
};
```

- 设置 `i2c_adapter` 下的 `i2c_algorithm` 为 `i2c_imx_algo` ：

```c
static struct i2c_algorithm i2c_imx_algo = {
    .master_xfer = i2c_imx_xfer;
    .functionality = i2c_imx_func;
};
```

**这里的点(.)是C语言的指定初始化器(designated initializer)语法，从C99标准开始引入的特性。不需要按照结构体定义的顺序来初始化，可以只初始化需要的成员，其他成员会被自动置零**

- 通过imx6ull 的 i2c 控制器读取i2c或者向i2c设备写入数据时最终是通过 `i2c_imx_xfer` 完成的

## I2C驱动 - I2C 设备驱动框架简析

- 重点关注两个数据结构 `i2c_client` 和 `i2c_driver` 
- `i2c_client` 表示i2c设备，不需要我们自己创建 `i2c_client` ，我们一般在设备树中添加具体的i2c芯片，比如fxls8471，系统在解析设备树时就会知道有这个i2c设备，然后会创建对应的i2c_client，
- `i2c_driver` 表示i2c驱动，需要编写，就是初始化 `i2c_driver` ，然后向系统注册，注册使用 `i2c_register_driver` 和 `i2c_add_driver` ，如果注销则使用 `i2c_del_driver` 

## I2C驱动 - 驱动

在I2C1上接了一个 AP3216C，UART4_RXD作为I2C1_SDA，UART4_TXD作为I2C1_SCL

- 修改设备树、IO、添加AP3216C设备节点
- I2C设备驱动框架、字符设备驱动框架
- 初始化AP3216C，实现 `ap3216c_read` 函数
- 重点是通过i2c控制器向ap3216c里发送或读取数据，这里使用 `i2c_transfer` 这个api函数来完成i2c数据的传输

```c
int i2c_transfer(struct i2c_adapter* adap, struct i2c_msg* msgs, int num) {

}
```

- adap: i2c设备对应的适配器，也就是i2c接口，当i2c设备和驱动匹配后，probe函数执行，probe函数传递进来的第一个参数就是 `i2c_client` ，在 `i2c_client` 里面保存了此i2c设备所对应的 `i2c_adapter` 


**ap3216c 不支持连续读取寄存器，需要分别读取每个寄存器，所以得用循环分别读取六个字节**
