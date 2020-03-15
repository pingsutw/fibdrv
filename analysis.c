#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define FIB_DEV "/dev/fibonacci"

#define MAX_LENGTH 100

struct BigN {
    unsigned long long lower;
    unsigned long long upper;
};

static inline void addBigN(struct BigN *r, struct BigN *x, struct BigN *y)
{
    r->upper = x->upper + y->upper;
    if (y->lower > ~x->lower)
        r->upper++;
    r->lower = x->lower + y->lower;
}

static struct BigN *subtractBigN(struct BigN *r, struct BigN *x, struct BigN *y)
{
    if (x->lower < y->lower) {
        unsigned long long mycarry = ULLONG_MAX;
        r->lower = mycarry + x->lower - y->lower + 1;
        r->upper = x->upper - y->upper - 1;
    } else {
        r->lower = x->lower - y->lower;
        r->upper = x->upper - y->upper;
    }
    return r;
}

static inline struct BigN *lsftBigN(struct BigN *r,
                                    struct BigN *x,
                                    unsigned char shift)
{
    if (shift == 0) {
        return x;
    } else if (shift >= 64) {
        r->upper = x->lower << (shift - 64);
        r->lower = 0ull;
        return r;
    }
    r->upper = x->upper << shift;
    r->lower = x->lower << shift;
    r->upper |= x->lower >> (64 - shift);
    return r;
}

static struct BigN *multiplieBigN(struct BigN *r,
                                  struct BigN *x,
                                  struct BigN *y)
{
    r->lower = 0;
    r->upper = 0;

    size_t w = 8 * sizeof(unsigned long long);

    for (size_t i = 0; i < w; i++) {
        if ((y->lower >> i) & 0x1) {
            struct BigN tmp;

            r->upper += x->upper << i;

            tmp.lower = (x->lower << i);
            tmp.upper = i == 0 ? 0 : (x->lower >> (w - i));
            addBigN(r, r, &tmp);
        }
    }

    for (size_t i = 0; i < w; i++) {
        if ((y->upper >> i) & 0x1) {
            r->upper += (x->lower << i);
        }
    }
    return r;
}

// static inline struct BigN *multiplieBigN(struct BigN *r, struct BigN *lo,
// struct BigN *hi)
// {
//     r->lower = 0ull;
//     r->upper = 0ull;
//     while (lo->lower) {
//         struct BigN prod;
//         unsigned char ctz = __builtin_ctzll(lo->lower);
//         lsftBigN(&prod, hi, ctz);
//         addBigN(r, &prod, r);
//         lo->lower &= ~(1ull << ctz);
//         // printf("lower: %llx, ctz=%d\n", lo.lower, ctz);
//     }
//     while (lo->upper) {
//         struct BigN prod;
//         unsigned char ctz = __builtin_ctzll(lo->upper);
//         lsftBigN(&prod, hi, ctz + 64);
//         addBigN(r, &prod, r);
//         lo->upper &= ~(1ull << ctz);
//         // printf("upper: %llx, ctz=%d\n", lo.upper, ctz);
//     }
//     return r;
// }


static struct BigN fib_sequence(long long k)
{
    /* FIXME: use clz/ctz and fast algorithms to speed up */
    struct BigN f[k + 2];

    f[0].upper = 0;
    f[0].lower = 0;

    f[1].upper = 0;
    f[1].lower = 1;

    for (int i = 2; i <= k; i++) {
        addBigN(&f[i], &f[i - 1], &f[i - 2]);
    }

    return f[k];
}

static struct BigN fast_fib_sequence_wo_clz(long long k)
{
    int bs = 0;
    long long t = k;
    while (t) {
        bs++;
        t >>= 1;
    }

    struct BigN a, b;
    a.upper = 0;
    a.lower = 0;

    b.upper = 0;
    b.lower = 1;

    struct BigN tmp;
    struct BigN t1;
    struct BigN t2;
    struct BigN t3;
    struct BigN t4;

    for (int i = bs; i >= 1; i--) {
        // addBigN(&tmp, &b, &b);
        lsftBigN(&tmp, &b, 1);
        multiplieBigN(&t1, &a, subtractBigN(&tmp, &tmp, &a));
        struct BigN *a2 = multiplieBigN(&t3, &a, &a);
        struct BigN *b2 = multiplieBigN(&t4, &b, &b);
        addBigN(&t2, a2, b2);
        a = t1;
        b = t2;
        if (k & (1 << (i - 1)) && k > 0) {
            addBigN(&t1, &a, &b);
            a = b;
            b = t1;
            k &= ~(1 << (i - 1));
        }
    }
    return a;
}

static long diff_in_ns(struct timespec t1, struct timespec t2)
{
    struct timespec diff;
    if (t2.tv_nsec - t1.tv_nsec < 0) {
        diff.tv_sec = t2.tv_sec - t1.tv_sec - 1;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec + 1000000000;
    } else {
        diff.tv_sec = t2.tv_sec - t1.tv_sec;
        diff.tv_nsec = t2.tv_nsec - t1.tv_nsec;
    }
    return (diff.tv_sec * 1000000000.0 + diff.tv_nsec);
}


void get_big_fibnum(char *buf, long long sz)
{
    char lower[MAX_LENGTH] = {0};
    snprintf(lower, sizeof(lower), "%s", buf);

    long long upper = sz;
    int carry = 0;

    char scale[MAX_LENGTH] = {0};
    snprintf(scale, sizeof(scale), "%s", "18446744073709551616");

    for (int i = 0; i < strlen(lower) / 2; i++) {
        char tmp = lower[i];
        lower[i] = lower[strlen(lower) - 1 - i];
        lower[strlen(lower) - 1 - i] = tmp;
    }

    for (int i = 0; i < strlen(scale) / 2; i++) {
        char tmp = scale[i];
        scale[i] = scale[strlen(scale) - 1 - i];
        scale[strlen(scale) - 1 - i] = tmp;
    }

    for (int i = 0; i < MAX_LENGTH; i++) {
        if ((upper == 0 || scale[i] == '\0') && lower[i] == '\0' &&
            carry == 0) {
            break;
        }
        int x = 0, y = 0;
        if (scale[i] != '\0')
            x = upper * (scale[i] - '0');
        if (lower[i] != '\0')
            y = (lower[i] - '0');
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

void display_big_fibnum(int index, struct BigN res)
{
    // printf("%d = %llu  %llu\n", index, res.upper, res.lower);
    char buf[100] = {0};
    snprintf(buf, sizeof(buf), "%llu", res.lower);
    get_big_fibnum(buf, res.upper);
    printf("%d = %s\n", index, buf);
}

int main()
{
    FILE *fp = fopen("./performance", "w");
    char time_buf[100] = {0};

    for (int i = 0; i <= MAX_LENGTH; i++) {
        struct timespec start, stop;

        /* fib_sequence */
        clock_gettime(CLOCK_MONOTONIC, &start);
        struct BigN res1 = fib_sequence(i);
        // display_big_fibnum(i, res1);
        clock_gettime(CLOCK_MONOTONIC, &stop);
        double t1 = diff_in_ns(start, stop);

        /* fast_fib_sequence_wo_clz */
        clock_gettime(CLOCK_MONOTONIC, &start);
        struct BigN res2 = fast_fib_sequence_wo_clz(i);
        clock_gettime(CLOCK_MONOTONIC, &stop);
        display_big_fibnum(i, res2);
        double t2 = diff_in_ns(start, stop);

        // snprintf(time_buf, sizeof(time_buf), "%d %.10lf\n", i, t1);
        snprintf(time_buf, sizeof(time_buf), "%d %.10lf %.10lf\n", i, t1, t2);
        fputs(time_buf, fp);
    }
    return 0;
}
