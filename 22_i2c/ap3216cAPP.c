#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/input.h>

/*
 * ./ap3216cAPP <filename>
 * ./ap3216cAPP /dev/ap3216c
 */

int main(int argc, char* argv[])
{
    int fd = 0;
    int err = 0;
    unsigned short data[3] = {0};
    char* filename = NULL;

    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    filename = argv[1]; // 获取设备文件名

    fd = open(filename, O_RDWR); // 打开KEY设备文件
    if (fd < 0) {
        printf("open %s failed\n", filename);
        return -1;
    }

    while(1) {
        err = read(fd, data, sizeof(data)); // 读取传感器数据
        if (err > 0) {
            printf("AP3216C IR = %d, ALS = %d, PS = %d\n", data[0], data[1], data[2]);
        }
        usleep(200000); // 200ms
    }

    close(fd); // 关闭设备文件

    return 0;
}