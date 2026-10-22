#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <stdint.h>

typedef struct {
    size_t s;
    size_t l;
} ln_t;

volatile sig_atomic_t to_flg = 0;

void hdl(int sig) {
    (void)sig;
    to_flg = 1;
}

int get_num(char *buf, size_t cap, int alrm) {
    sigset_t blk, old, wht;
    sigemptyset(&blk);
    sigaddset(&blk, SIGALRM);

    if (sigprocmask(SIG_BLOCK, &blk, &old) == -1)
        return -1;

    wht = old;
    sigdelset(&wht, SIGALRM);

    size_t len = 0;
    int big = 0;
    int res = -1;

    to_flg = 0;
    if (alrm)
        alarm(5);

    while (1) {
        fd_set rdy;
        FD_ZERO(&rdy);
        FD_SET(STDIN_FILENO, &rdy);

        int n = pselect(STDIN_FILENO + 1, &rdy, NULL, NULL, NULL, &wht);

        if (to_flg) {
            res = -2;
            break;
        }

        if (n == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        char c;
        ssize_t got = read(STDIN_FILENO, &c, 1);

        if (got == -1) {
            if (errno == EINTR)
                continue;
            break;
        }

        if (got == 0 || c == '\n') {
            buf[len] = '\0';
            if (big)
                res = 2;
            else if (got == 0 && len == 0)
                res = 0;
            else
                res = 1;
            break;
        }

        if (c == '\0')
            big = 1;

        if (len < cap - 1)
            buf[len++] = c;
        else
            big = 1;
    }

    int e_sv = errno;
    if (alrm)
        alarm(0);

    if (sigprocmask(SIG_SETMASK, &old, NULL) == -1)
        return -1;

    errno = e_sv;
    return res;
}

int out_pt(const char *dat, size_t start, size_t len) {
    if (len == 0)
        return 0;

    if (fwrite(dat + start, 1, len, stdout) != len) {
        fprintf(stderr, "output error\n");
        return 1;
    }

    if (dat[start + len - 1] != '\n') {
        if (putchar('\n') == EOF) {
            fprintf(stderr, "output error\n");
            return 1;
        }
    }

    return 0;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s file\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("open");
        return 1;
    }

    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (!S_ISREG(st.st_mode)) {
        fprintf(stderr, "not a regular file\n");
        close(fd);
        return 1;
    }

    if (st.st_size < 0 || (uintmax_t)st.st_size > SIZE_MAX) {
        fprintf(stderr, "file too large\n");
        close(fd);
        return 1;
    }

    size_t f_sz = (size_t)st.st_size;
    char *dat = NULL;

    if (f_sz > 0) {
        dat = mmap(NULL, f_sz, PROT_READ, MAP_PRIVATE, fd, 0);
        if (dat == MAP_FAILED) {
            perror("mmap");
            close(fd);
            return 1;
        }
    }

    if (close(fd) == -1) {
        perror("close");
        if (f_sz > 0)
            munmap(dat, f_sz);
        return 1;
    }

    struct sigaction act = {0};
    act.sa_handler = hdl;
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGALRM, &act, NULL) == -1) {
        perror("sigaction");
        if (f_sz > 0)
            munmap(dat, f_sz);
        return 1;
    }

    ln_t *tbl = NULL;
    size_t cnt = 0;
    size_t start = 0;
    int st_val = 0;

    for (size_t pos = 0; pos < f_sz; pos++) {
        if (dat[pos] == '\n' || pos == f_sz - 1) {
            if (cnt >= SIZE_MAX / sizeof(ln_t)) {
                fprintf(stderr, "too many lines\n");
                st_val = 1;
                break;
            }

            ln_t *tmp = realloc(tbl, (cnt + 1) * sizeof(ln_t));
            if (tmp == NULL) {
                perror("realloc");
                st_val = 1;
                break;
            }

            tbl = tmp;
            tbl[cnt].s = start;
            tbl[cnt].l = pos - start + 1;
            cnt++;
            start = pos + 1;
        }
    }

    if (st_val == 0) {
        printf("lines: %zu\n", cnt);

        for (size_t i = 0; i < cnt; i++)
            printf("%zu: offset %zu, length %zu\n", i + 1, tbl[i].s, tbl[i].l);

        char in_buf[64];
        int first_in = 1;

        while (1) {
            if (first_in)
                printf("\nline number (5 seconds, 0 to exit): ");
            else
                printf("\nline number (0 to exit): ");

            if (fflush(stdout) == EOF) {
                fprintf(stderr, "output error\n");
                st_val = 1;
                break;
            }

            int res = get_num(in_buf, sizeof(in_buf), first_in);
            first_in = 0;

            if (res == -2) {
                printf("\ntime is up\n");
                break;
            }

            if (res == -1) {
                perror("input");
                st_val = 1;
                break;
            }

            if (res == 0)
                break;

            if (res == 2) {
                printf("invalid number\n");
                continue;
            }

            char *end;
            errno = 0;
            long num = strtol(in_buf, &end, 10);

            if (end == in_buf || errno == ERANGE) {
                printf("invalid number\n");
                continue;
            }

            while (isspace((unsigned char)*end))
                end++;

            if (*end != '\0') {
                printf("invalid number\n");
                continue;
            }

            if (num == 0)
                break;

            if (num < 0 || (uintmax_t)num > (uintmax_t)cnt) {
                printf("line not found\n");
                continue;
            }

            ln_t ln = tbl[num - 1];
            printf("line %ld: ", num);
            st_val = out_pt(dat, ln.s, ln.l);

            if (st_val != 0)
                break;
        }
    }

    free(tbl);

    if (f_sz > 0 && dat != NULL && dat != MAP_FAILED) {
        if (munmap(dat, f_sz) == -1) {
            perror("munmap");
            st_val = 1;
        }
    }

    if (fflush(stdout) == EOF) {
        fprintf(stderr, "output error\n");
    }

    return st_val;
}
