#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

/*
* ./timerAPP <filename>      
* ./timerAPP /dev/timer   读取定时器值                       
*/

#define CLOSE_CMD       _IO(0xEF, 1) // 关闭定时器  0xEF 自定义幻数
#define OPEN_CMD        _IO(0xEF, 2) // 打开定时器
#define SETPERIOD_CMD   _IOW(0xEF, 3, int) // 设置定时器周期

int main(int argc, char *argv[]){
    int fd = 0;
    int ret = 0;
    char *filename = NULL;
    unsigned char databuf[1];
    unsigned int cmd = 0;
    unsigned int arg = 0;
    unsigned char str[100];

    if(argc != 2){
        printf("Usage: %s <filename>\n", argv[0]);
        return -1;
    }

    filename = argv[1]; // 获取设备文件名

    fd = open(filename, O_RDWR); // 打开KEY设备文件
    if(fd < 0){
        printf("open %s failed\n", filename);
        return -1;
    }

    /* circle read */
    while(1){
        printf("input cmd");
        ret = scanf("%d", &cmd);
        if(ret != 1){
            fgets(str, sizeof(str), stdin);      // 防止卡死
        }

        if(cmd == 1){
            ioctl(fd, CLOSE_CMD, &arg); // 关闭定时器
        }
        else if(cmd == 2){
            ioctl(fd, OPEN_CMD, &arg); // 打开定时器
        }
        else if(cmd == 3){
            printf("input period(ms):");
            ret = scanf("%d", &arg);
            if(ret != 1){
                fgets(str, sizeof(str), stdin); // 防止卡死
            }
            ioctl(fd, SETPERIOD_CMD, &arg); // 设置定时器周期
        }
        else{
            printf("Invalid command\n");
            continue;
        }
    }

    close(fd); // 关闭设备文件
    
    return 0;
}