
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <errno.h>

typedef struct
{
    off_t start;
    off_t length;
} Line;

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

    Line *table = NULL;
    size_t count = 0;
    off_t pos = 0, start = 0;
    char c;
    ssize_t n;
    int status = 0;

    while ((n = read(fd, &c, 1)) == 1)
    {
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

    if (n == -1)
    {
        perror("read");
        status = 1;
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
        char buffer[4096];

        while (1)
        {
            printf("\nline number: ");
            fflush(stdout);

            if (fgets(input, sizeof(input), stdin) == NULL)
            {
                if (ferror(stdin))
                    perror("input");
                break;
            }

            size_t len = 0;
            while (input[len] != '\0' && input[len] != '\n')
                len++;

            if (input[len] != '\n' && !feof(stdin))
            {
                int ch = getchar();
                if (ch != '\n' && ch != EOF)
                {
                    while (ch != '\n' && ch != EOF)
                        ch = getchar();
                    printf("invalid number\n");
                    continue;
                }
            }

            char *end;
            errno = 0;
            long num = strtol(input, &end, 10);

            while (*end == ' ' || *end == '\t')
                end++;

            if (end == input || errno == ERANGE ||
                (*end != '\0' && *end != '\n'))
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

            if (lseek(fd, line.start, SEEK_SET) == -1)
            {
                perror("lseek");
                status = 1;
                break;
            }

            off_t remaining = line.length;
            char last = '\n';

            printf("line %ld: ", num);

            while (remaining > 0)
            {
                size_t size = remaining > (off_t)sizeof(buffer)
                            ? sizeof(buffer) : (size_t)remaining;

                ssize_t got = read(fd, buffer, size);

                if (got <= 0)
                {
                    if (got < 0)
                        perror("read");
                    else
                        fprintf(stderr, "unexpected end of file\n");

                    status = 1;
                    break;
                }

                last = buffer[got - 1];
                fwrite(buffer, 1, got, stdout);
                remaining -= got;
            }

            if (status != 0)
                break;

            if (last != '\n')
                putchar('\n');
        }
    }

    free(table);
    close(fd);
    return status;
}

