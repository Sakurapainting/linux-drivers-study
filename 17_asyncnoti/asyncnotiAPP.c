#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>

/*
 * ./asyncnotiAPP <filename>
 * ./asyncnotiAPP /dev/myirqdev
 */

static int fd = 0;  // 全局变量，供信号处理函数使用

static void sigio_handler(int signum)
{
    int ret;
    unsigned int keyval = 0;
    
    printf("Received SIGIO signal\n");
    
    // 读取按键数据
    ret = read(fd, &keyval, sizeof(keyval));
    if (ret <0) {

    } else {
        printf("sigio signal Key value = %#x\n", keyval);
    }
}

int main(int argc, char* argv[])
{
    int ret = 0;
    char* filename = NULL;
    unsigned char data;
    unsigned int cmd = 0;
    unsigned int arg = 0;

    int flags = 0;

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

    // 设置信号处理函数
    signal(SIGIO, sigio_handler);

    // 设置当前进程能够接收SIGIO信号
    fcntl(fd, F_SETOWN, getpid());
    
    // 设置设备文件为异步通知模式
    flags = fcntl(fd, F_GETFL);
    fcntl(fd, F_SETFL, flags | FASYNC);

    close(fd); // 关闭设备文件

    printf("Waiting for key events... Press Ctrl+C to exit\n");
    
    // 保持程序运行，等待异步信号
    while (1) {
        sleep(1);
    }
    
    return 0;
}