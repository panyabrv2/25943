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

typedef struct
{
    size_t start;
    size_t length;
} Line;

volatile sig_atomic_t timedOut = 0;

void alarmHandler(int signalNumber)
{
    (void)signalNumber;
    timedOut = 1;
}

int readNumber(char *input, size_t capacity)
{
    sigset_t blocked, oldMask, waitMask;

    sigemptyset(&blocked);
    sigaddset(&blocked, SIGALRM);

    if (sigprocmask(SIG_BLOCK, &blocked, &oldMask) == -1)
        return -1;

    waitMask = oldMask;
    sigdelset(&waitMask, SIGALRM);

    size_t length = 0;
    int tooLong = 0;
    int result = -1;

    timedOut = 0;
    alarm(5);

    while (1)
    {
        fd_set ready;
        FD_ZERO(&ready);
        FD_SET(STDIN_FILENO, &ready);

        int n = pselect(STDIN_FILENO + 1, &ready, NULL,
                        NULL, NULL, &waitMask);

        if (timedOut)
        {
            result = -2;
            break;
        }

        if (n == -1)
        {
            if (errno == EINTR)
                continue;

            break;
        }

        char c;
        ssize_t got = read(STDIN_FILENO, &c, 1);

        if (got == -1)
        {
            if (errno == EINTR)
                continue;

            break;
        }

        if (got == 0 || c == '\n')
        {
            input[length] = '\0';

            if (tooLong)
                result = 2;
            else if (got == 0 && length == 0)
                result = 0;
            else
                result = 1;

            break;
        }

        if (c == '\0')
            tooLong = 1;

        if (length < capacity - 1)
            input[length++] = c;
        else
            tooLong = 1;
    }

    int savedErrno = errno;

    alarm(0);

    if (sigprocmask(SIG_SETMASK, &oldMask, NULL) == -1)
        return -1;

    errno = savedErrno;
    return result;
}

int printPart(const char *data, size_t start, size_t length)
{
    if (length == 0)
        return 0;

    if (fwrite(data + start, 1, length, stdout) != length)
    {
        fprintf(stderr, "output error\n");
        return 1;
    }

    if (data[start + length - 1] != '\n')
    {
        if (putchar('\n') == EOF)
        {
            fprintf(stderr, "output error\n");
            return 1;
        }
    }

    return 0;
}

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        printf("usage: %s file\n", argv[0]);
        return 1;
    }

    int fd = open(argv[1], O_RDONLY);

    if (fd == -1)
    {
        perror("open");
        return 1;
    }

    struct stat info;

    if (fstat(fd, &info) == -1)
    {
        perror("fstat");
        close(fd);
        return 1;
    }

    if (!S_ISREG(info.st_mode))
    {
        fprintf(stderr, "not a regular file\n");
        close(fd);
        return 1;
    }

    if (info.st_size < 0 || (uintmax_t)info.st_size > SIZE_MAX)
    {
        fprintf(stderr, "file too large\n");
        close(fd);
        return 1;
    }

    size_t fileSize = (size_t)info.st_size;
    char *data = NULL;

    if (fileSize > 0)
    {
        data = mmap(NULL, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);

        if (data == MAP_FAILED)
        {
            perror("mmap");
            close(fd);
            return 1;
        }
    }

    if (close(fd) == -1)
    {
        perror("close");

        if (fileSize > 0)
            munmap(data, fileSize);

        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = alarmHandler;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGALRM, &action, NULL) == -1)
    {
        perror("sigaction");

        if (fileSize > 0)
            munmap(data, fileSize);

        return 1;
    }

    Line *table = NULL;
    size_t count = 0;
    size_t start = 0;
    int status = 0;

    for (size_t pos = 0; pos < fileSize; pos++)
    {
        if (data[pos] == '\n' || pos == fileSize - 1)
        {
            if (count >= SIZE_MAX / sizeof(Line))
            {
                fprintf(stderr, "too many lines\n");
                status = 1;
                break;
            }

            Line *tmp = realloc(table, (count + 1) * sizeof(Line));

            if (tmp == NULL)
            {
                perror("realloc");
                status = 1;
                break;
            }

            table = tmp;
            table[count].start = start;
            table[count].length = pos - start + 1;
            count++;
            start = pos + 1;
        }
    }

    if (status == 0)
    {
        printf("lines: %zu\n", count);

        for (size_t i = 0; i < count; i++)
            printf("%zu: offset %zu, length %zu\n",
                   i + 1, table[i].start, table[i].length);

        char input[64];

        while (1)
        {
            printf("\nline number (5 seconds, 0 to exit): ");

            if (fflush(stdout) == EOF)
            {
                fprintf(stderr, "output error\n");
                status = 1;
                break;
            }

            int result = readNumber(input, sizeof(input));

            if (result == -2)
            {
                printf("\ntime is up\n");
                status = printPart(data, 0, fileSize);
                break;
            }

            if (result == -1)
            {
                perror("input");
                status = 1;
                break;
            }

            if (result == 0)
                break;

            if (result == 2)
            {
                printf("invalid number\n");
                continue;
            }

            char *end;
            errno = 0;
            long num = strtol(input, &end, 10);

            if (end == input || errno == ERANGE)
            {
                printf("invalid number\n");
                continue;
            }

            while (isspace((unsigned char)*end))
                end++;

            if (*end != '\0')
            {
                printf("invalid number\n");
                continue;
            }

            if (num == 0)
                break;

            if (num < 0 || (uintmax_t)num > (uintmax_t)count)
            {
                printf("line not found\n");
                continue;
            }

            Line line = table[num - 1];

            printf("line %ld: ", num);
            status = printPart(data, line.start, line.length);

            if (status != 0)
                break;
        }
    }

    free(table);

    if (fileSize > 0 && munmap(data, fileSize) == -1)
    {
        perror("munmap");
        status = 1;
    }

    if (fflush(stdout) == EOF)
    {
        fprintf(stderr, "output error\n");
        status = 1;
    }

    return status;
}
