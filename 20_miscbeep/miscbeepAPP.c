#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * ./miscbeepAPP <filename> <0:1>      0表示关，1表示开
 * ./miscbeepAPP /dev/miscbeep 0            关
 * ./miscbeepAPP /dev/miscbeep 1            开
 */

int main(int argc, char* argv[])
{
    int fd = 0;
    int ret = 0;
    char* filename = NULL;
    unsigned char databuf[1];

    if (argc != 3) {
        printf("Usage: %s <filename> <0:1>\n", argv[0]);
        return -1;
    }

    filename = argv[1]; // 获取设备文件名

    fd = open(filename, O_RDWR);
    if (fd < 0) {
        printf("open %s failed\n", filename);
        return -1;
    }

    databuf[0] = atoi(argv[2]); // 将命令行参数转换为整数

    ret = write(fd, databuf, sizeof(databuf));
    if (ret < 0) {
        printf("beep control %s failed\n", filename);
        close(fd);
        return -1;
    } else {
        printf("beep control %s success, command: %d\n", filename, databuf[0]);
    }

    close(fd); // 关闭设备文件

    return 0;
}