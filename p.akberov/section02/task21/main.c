#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

volatile sig_atomic_t count = 0;
volatile sig_atomic_t finished = 0;

void signalHandler(int signalNumber)
{
    int savedErrno = errno;

    if (signalNumber == SIGINT)
    {
        if (count < SIG_ATOMIC_MAX)
            count++;

        const char bell = '\a';
        (void)write(STDOUT_FILENO, &bell, 1);
    }
    else
        finished = 1;

    errno = savedErrno;
}

int main(void)
{
    sigset_t blocked, oldMask, waitMask;
    sigemptyset(&blocked);
    sigaddset(&blocked, SIGINT);
    sigaddset(&blocked, SIGQUIT);

    if (sigprocmask(SIG_BLOCK, &blocked, &oldMask) == -1)
    {
        perror("sigprocmask");
        return 1;
    }

    struct sigaction action = {0};
    action.sa_handler = signalHandler;
    action.sa_mask = blocked;

    if (sigaction(SIGINT, &action, NULL) == -1 ||
        sigaction(SIGQUIT, &action, NULL) == -1)
    {
        perror("sigaction");
        return 1;
    }

    waitMask = oldMask;
    sigdelset(&waitMask, SIGINT);
    sigdelset(&waitMask, SIGQUIT);

    printf("Ctrl-C: beep, Ctrl-\\: quit\n");
    fflush(stdout);

    while (!finished)
        sigsuspend(&waitMask);

    printf("\nbeeps: %ld\n", (long)count);
    return 0;
}
