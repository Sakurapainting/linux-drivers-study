#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/time.h>
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
#if 0
    fd_set readfds;
    struct timeval timeout;
#endif
    struct pollfd fds;

    char* filename = NULL;
    unsigned char data;
    unsigned int cmd = 0;
    unsigned int arg = 0;

    if (argc != 2) {
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    filename = argv[1]; // 获取设备文件名

    fd = open(filename, O_RDWR | O_NONBLOCK); // 非阻塞 打开KEY设备文件
    if (fd < 0) {
        printf("open %s failed\n", filename);
        return -1;
    }

    /* circle read */
    while (1) {

#if 0
        FD_ZERO(&readfds);                      // 清空读文件描述符集合
        FD_SET(fd, &readfds);                   // 将设备文件描述符添加到集合中

        timeout.tv_sec = 0;
        timeout.tv_usec = 500000; // 设置超时时间为500ms

        ret = select(fd + 1, &readfds, NULL, NULL, &timeout); // 监听文件描述符
#endif

        fds.fd = fd;
        fds.events = POLLIN;
        ret = poll(&fds, 1, 500); // num 1 个, 500ms timeout

#if 0
        switch(ret) {
            case 0:     // timeout 轮询之外可以做其他事情
                break;
            case -1:    // error
                perror("select");
                break;
            default:    // data available
                if(FD_ISSET(fd, &readfds)) { // 检查文件描述符是否就绪
                    ret = read(fd, &data, sizeof(data));    // 读取keyvalue
                    if(ret < 0){
                        // 并不是错误处理，而是按键没有生效的情况
                    } else {
                        if(data){
                            printf("keyvalue = %#x\n", data);
                        }
                    }
                }
                break;
        }
#endif
        if (ret == 0) { // timeout
            // 轮询之外可以做其他事情
        } else if (ret < 0) { // error
            perror("poll");
        } else {
            if (fds.revents | POLLIN) { // 可读取
                ret = read(fd, &data, sizeof(data)); // 读取keyvalue
                if (ret < 0) {
                    // 并不是错误处理，而是按键没有生效的情况
                } else {
                    if (data) {
                        printf("keyvalue = %#x\n", data);
                    }
                }
            }
        }
    }

    close(fd); // 关闭设备文件

    return 0;
}