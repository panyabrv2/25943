#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <ctype.h>
#include <signal.h>
#include <errno.h>

int writeAll(int fd, const char *buffer, size_t length)
{
    while (length > 0)
    {
        ssize_t written = write(fd, buffer, length);

        if (written == -1)
        {
            if (errno == EINTR)
                continue;

            perror("write");
            return 1;
        }

        if (written == 0)
        {
            fprintf(stderr, "write: no progress\n");
            return 1;
        }

        buffer += written;
        length -= (size_t)written;
    }

    return 0;
}

int copyText(int input, int output, int uppercase)
{
    char buffer[4096];

    while (1)
    {
        ssize_t length = read(input, buffer, sizeof(buffer));

        if (length == -1)
        {
            if (errno == EINTR)
                continue;

            perror("read");
            return 1;
        }

        if (length == 0)
            return 0;

        if (uppercase)
            for (ssize_t i = 0; i < length; i++)
                buffer[i] = (char)toupper((unsigned char)buffer[i]);

        if (writeAll(output, buffer, (size_t)length) != 0)
            return 1;
    }
}

int main(void)
{
    struct sigaction action = {0};
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);

    if (sigaction(SIGPIPE, &action, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    int channel[2];

    if (pipe(channel) == -1)
    {
        perror("pipe");
        return 1;
    }

    pid_t child = fork();

    if (child == -1)
    {
        perror("fork");
        close(channel[0]);
        close(channel[1]);
        return 1;
    }

    if (child == 0)
    {
        close(channel[1]);
        int status = copyText(channel[0], STDOUT_FILENO, 1);
        close(channel[0]);
        _exit(status);
    }

    close(channel[0]);
    int status = copyText(STDIN_FILENO, channel[1], 0);
    close(channel[1]);

    int childStatus;

    while (waitpid(child, &childStatus, 0) == -1)
    {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return 1;
    }

    if (!WIFEXITED(childStatus) || WEXITSTATUS(childStatus) != 0)
        status = 1;

    return status;
}

