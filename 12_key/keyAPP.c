#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * ./keyAPP <filename>
 * ./keyAPP /dev/key   读取按键值
 */

#define KEY0VALUE 0xf0 // 自定义
#define INVALKEY 0x00 // 自定义

int main(int argc, char* argv[])
{
    int val = 0;
    int fd = 0;
    int ret = 0;
    char* filename = NULL;
    unsigned char databuf[1];

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

    /* circle read */
    while (1) {
        read(fd, &val, sizeof(val)); // 读取按键值
        if (val == KEY0VALUE) {
            printf("key0 Press, val = %#x\n", val);
        }
    }

    close(fd); // 关闭设备文件

    return 0;
}