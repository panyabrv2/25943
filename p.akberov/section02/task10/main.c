#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: %s command [arguments...]\n", argv[0]);
        return 1;
    }

    pid_t child = fork();

    if (child == -1)
    {
        perror("fork");
        return 1;
    }

    if (child == 0)
    {
        execvp(argv[1], argv + 1);
        int error = errno;
        perror("execvp");
        _exit(error == ENOENT ? 127 : 126);
    }

    int status;

    while (waitpid(child, &status, 0) == -1)
    {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return 1;
    }

    if (WIFEXITED(status))
    {
        printf("exit code: %d\n", WEXITSTATUS(status));
        return WEXITSTATUS(status);
    }

    if (WIFSIGNALED(status))
    {
        printf("terminated by signal: %d\n", WTERMSIG(status));
        return 128 + WTERMSIG(status);
    }

    return 1;
}
