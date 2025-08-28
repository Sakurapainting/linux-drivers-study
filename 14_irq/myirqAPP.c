#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * ./myirqAPP <filename>
 * ./myirqAPP /dev/myirqdev
 */

int main(int argc, char* argv[])
{
    int fd = 0;
    int ret = 0;
    char* filename = NULL;
    unsigned char data;
    unsigned int cmd = 0;
    unsigned int arg = 0;

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
        ret = read(fd, &data, sizeof(data)); // 读取keyvalue
        if (ret < 0) {
            // 并不是错误处理，而是按键没有生效的情况
        } else {
            if (data) {
                printf("keyvalue = %#x\n", data);
            }
        }
    }

    close(fd); // 关闭设备文件

    return 0;
}