#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * ./chrdevbaseAPP <filename> <1:2>      1表示读，2表示写
 */

int main(int argc, char* argv[])
{
    int ret = 0;
    int fd = 0;
    char* filename = argv[1];
    char readbuf[100] = { 0 };
    char writebuf[100] = { 0 };
    static char usrdata[] = { "user data" }; /* 用户数据示例 */

    if (argc != 3) {
        printf("Usage: %s <filename> <1:2>\n", argv[0]);
        return -1;
    }

    fd = open(filename, O_RDWR);
    if (fd < 0) {
        printf("cannot open %s\n", filename);
        return -1;
    }

    if (atoi(argv[2]) == 2) {
        memcpy(writebuf, usrdata, sizeof(usrdata)); /* 将用户数据复制到写入缓冲区 */
        ret = write(fd, writebuf, 50);
        if (ret < 0) {
            printf("write %s failed\n", filename);
        } else {
            printf("write %s success\n", filename);
        }
    } else if (atoi(argv[2]) == 1) {
        ret = read(fd, readbuf, 50);
        if (ret < 0) {
            printf("read %s failed\n", filename);
        } else {
            printf("read %s success, data: %s\n", filename, readbuf);
        }
    } else {
        printf("Invalid option, use 1 for read and 2 for write.\n");
        return -1;
    }

    ret = close(fd);
    if (ret < 0) {
        printf("close %s failed\n", filename);
    } else {
        printf("close %s success\n", filename);
    }

    return 0;
}