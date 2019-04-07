#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

void big_fibnum(char *buf, long long sz)
{
    char res[65];
    memset(res, '\0', 65);
    char str[65];
    memset(str, '\0', 65);
    sprintf(str, "%lld", sz);
    for (int i = 0; i < strlen(str) / 2; i++) {
        char tmp = str[i];
        str[i] = str[strlen(str) - 1 - i];
        str[strlen(str) - 1 - i] = tmp;
    }
    char scale[65];
    memset(scale, '\0', 65);
    sprintf(scale, "%ld", 7540113804746346429);
    for (int i = 0; i < strlen(scale) / 2; i++) {
        char tmp = scale[i];
        scale[i] = scale[strlen(scale) - 1 - i];
        scale[strlen(scale) - 1 - i] = tmp;
    }

    long long num;
    sscanf(buf, "%lld", &num);
    int carry = 0;

    int len = 65;
    for (int i = 0; i < 65; i++) {
        if ((num == 0 || scale[i] == '\0') && str[i] == '\0' && carry == 0) {
            break;
        }
        int x = 0, y = 0;
        if (scale[i] != '\0')
            x = num * (scale[i] - '0');
        if (str[i] != '\0')
            y = (str[i] - '0');
        int tmp = x + y + carry;
        buf[i] = (char) (tmp % 10 + '0');
        carry = tmp / 10;
    }
    for (int i = 0; i < strlen(buf) / 2; i++) {
        char tmp = buf[i];
        buf[i] = buf[strlen(buf) - 1 - i];
        buf[strlen(buf) - 1 - i] = tmp;
    }
    return;
}

int main()
{
    long long sz;

    char buf[65];
    char write_buf[] = "testing writing";
    int offset = 100; /* TODO: try test something bigger than the limit */

    int fd = open(FIB_DEV, O_RDWR);
    if (fd < 0) {
        perror("Failed to open character device");
        exit(1);
    }

    for (int i = 0; i <= offset; i++) {
        sz = write(fd, write_buf, strlen(write_buf));
        printf("Writing to " FIB_DEV ", returned the sequence %lld\n", sz);
    }

    for (int i = 0; i <= offset; i++) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 65);
        big_fibnum(buf, sz);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    for (int i = offset; i >= 0; i--) {
        lseek(fd, i, SEEK_SET);
        sz = read(fd, buf, 65);
        big_fibnum(buf, sz);
        printf("Reading from " FIB_DEV
               " at offset %d, returned the sequence "
               "%s.\n",
               i, buf);
    }

    close(fd);
    return 0;
}
