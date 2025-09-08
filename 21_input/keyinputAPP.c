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
 * ./keyinputAPP <filename>
 * ./keyinputAPP /dev/input/event1
 */

static struct input_event inputevent;

int main(int argc, char* argv[])
{
    int fd = 0;
    int err = 0;
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

    while(1) {
        err = read(fd, &inputevent, sizeof(inputevent)); // 读取按键事件
        if(err > 0) {       // 数据读取成功
            switch(inputevent.type) {
                case EV_KEY:    // 按键事件
                    printf("Key/Button %d %s\n", inputevent.code, inputevent.value ? "press" : "release");
                    break;
                case EV_SYN:   // 同步事件
                    //printf("EV_SYN\n");
                    break;
                default:
                    break;
            }
        } else {
            printf("read key error\n");
        }
    }

    close(fd); // 关闭设备文件

    return 0;
}