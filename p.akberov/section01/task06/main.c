#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/select.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>

typedef struct
{
    off_t start;
    off_t length;
} Line;

volatile sig_atomic_t timedOut = 0;

void alarmHandler(int signalNumber)
{
    (void)signalNumber;
    timedOut = 1;
}

int readNumber(char *input, size_t capacity, int enableAlarm)
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
    if (enableAlarm)
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

    if (enableAlarm)
        alarm(0);

    if (sigprocmask(SIG_SETMASK, &oldMask, NULL) == -1)
        return -1;

    errno = savedErrno;
    return result;
}

int printPart(int fd, off_t start, off_t length)
{
    if (lseek(fd, start, SEEK_SET) == -1)
    {
        perror("lseek");
        return 1;
    }

    char buffer[4096];
    char last = '\n';

    while (length != 0)
    {
        size_t size = sizeof(buffer);

        if (length > 0 && length < (off_t)size)
            size = (size_t)length;

        ssize_t got = read(fd, buffer, size);

        if (got == -1)
        {
            if (errno == EINTR)
                continue;

            perror("read");
            return 1;
        }

        if (got == 0)
        {
            if (length > 0)
            {
                fprintf(stderr, "unexpected end of file\n");
                return 1;
            }

            break;
        }

        if (fwrite(buffer, 1, (size_t)got, stdout) != (size_t)got)
        {
            fprintf(stderr, "output error\n");
            return 1;
        }

        last = buffer[got - 1];

        if (length > 0)
            length -= got;
    }

    if (last != '\n')
        putchar('\n');

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

    struct sigaction action = {0};
    action.sa_handler = alarmHandler;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGALRM, &action, NULL) == -1)
    {
        perror("sigaction");
        close(fd);
        return 1;
    }

    Line *table = NULL;
    size_t count = 0;
    off_t pos = 0, start = 0;
    char c;
    ssize_t n;
    int status = 0;

    while ((n = read(fd, &c, 1)) != 0)
    {
        if (n == -1)
        {
            if (errno == EINTR)
                continue;

            perror("read");
            status = 1;
            break;
        }

        pos++;

        if (c == '\n')
        {
            Line *tmp = realloc(table, (count + 1) * sizeof(Line));

            if (tmp == NULL)
            {
                perror("realloc");
                status = 1;
                break;
            }

            table = tmp;
            table[count].start = start;
            table[count].length = pos - start;
            count++;
            start = pos;
        }
    }

    if (status == 0 && pos > start)
    {
        Line *tmp = realloc(table, (count + 1) * sizeof(Line));

        if (tmp == NULL)
        {
            perror("realloc");
            status = 1;
        }
        else
        {
            table = tmp;
            table[count].start = start;
            table[count].length = pos - start;
            count++;
        }
    }

    if (status == 0)
    {
        printf("lines: %zu\n", count);

        for (size_t i = 0; i < count; i++)
            printf("%zu: offset %lld, length %lld\n",
                   i + 1,
                   (long long)table[i].start,
                   (long long)table[i].length);

        char input[64];
        int isFirstInput = 1;

        while (1)
        {
            if (isFirstInput)
                printf("\nline number (5 seconds, 0 to exit): ");
            else
                printf("\nline number (0 to exit): ");

            if (fflush(stdout) == EOF)
            {
                fprintf(stderr, "output error\n");
                status = 1;
                break;
            }

            int result = readNumber(input, sizeof(input), isFirstInput);
            isFirstInput = 0;

            if (result == -2)
            {
                printf("\ntime is up\n");
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

            if (num < 0 || (unsigned long)num > count)
            {
                printf("line not found\n");
                continue;
            }

            Line line = table[num - 1];

            printf("line %ld: ", num);
            status = printPart(fd, line.start, line.length);

            if (status != 0)
                break;
        }
    }

    free(table);

    if (close(fd) == -1)
    {
        perror("close");
        status = 1;
    }

    return status;
}
