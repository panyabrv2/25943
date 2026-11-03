#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        fprintf(stderr, "usage: %s file\n", argv[0]);
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
        execlp("cat", "cat", argv[1], (char *)NULL);
        perror("execlp");
        _exit(127);
    }

    printf("parent: waiting for child\n");
    fflush(stdout);

    int status;

    while (waitpid(child, &status, 0) == -1)
    {
        if (errno == EINTR)
            continue;

        perror("waitpid");
        return 1;
    }

    printf("parent: child finished\n");
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : 1;
}
