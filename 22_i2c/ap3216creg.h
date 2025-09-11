#ifndef AP3216C_H
#define AP3216C_H

/* AP3316C 寄存器 */
#define AP3216C_SYSTEMCONG      0x00 /* 配置寄存器 */
#define AP3216C_INTSTATUS       0X01 /* 中断状态寄存器 */
#define AP3216C_INTCLEAR        0X02 /* 中断清除寄存器 */
#define AP3216C_IRDATALOW       0x0A /* IR 数据低字节 */
#define AP3216C_IRDATAHIGH      0x0B /* IR 数据高字节 */
#define AP3216C_ALSDATALOW      0x0C /* ALS 数据低字节 */
#define AP3216C_ALSDATAHIGH     0X0D /* ALS 数据高字节 */
#define AP3216C_PSDATALOW       0X0E /* PS 数据低字节 */
#define AP3216C_PSDATAHIGH      0X0F /* PS 数据高字节 */

#endif
