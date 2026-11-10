#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <ctype.h>
#include <poll.h>

#define WIDTH 40

static struct termios savedTerminal;
static int terminalFd = -1;

static void restoreTerminal(void)
{
    if (terminalFd != -1)
        tcsetattr(terminalFd, TCSAFLUSH, &savedTerminal);
}

static void setupTerminal(void)
{
    terminalFd = open("/dev/tty", O_RDWR);
    if (terminalFd == -1)
    {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (tcgetattr(terminalFd, &savedTerminal) == -1)
    {
        perror("tcgetattr");
        exit(EXIT_FAILURE);
    }

    atexit(restoreTerminal);

    struct termios settings = savedTerminal;
    settings.c_lflag &= ~(ICANON | ECHO | IEXTEN | ISIG);
    settings.c_cc[VMIN] = 1;
    settings.c_cc[VTIME] = 0;

    if (tcsetattr(terminalFd, TCSAFLUSH, &settings) == -1)
    {
        perror("tcsetattr");
        exit(EXIT_FAILURE);
    }
}

static void eraseCharacters(int count)
{
    for (int i = 0; i < count; i++)
        write(terminalFd, "\b \b", 3);
}

static void skipEscape(void)
{
    struct pollfd ready = {terminalFd, POLLIN, 0};
    unsigned char byte;

    while (poll(&ready, 1, 20) > 0)
        if (read(terminalFd, &byte, 1) <= 0)
            break;
}

int main(void)
{
    setupTerminal();

    char line[WIDTH];
    int length = 0;
    unsigned char key;

    printf("Line editor (40 columns)\n");
    printf("ERASE: Backspace, KILL: Ctrl-U\n");
    printf("Ctrl-W: erase word, Ctrl-D: exit\n\n");
    fflush(stdout);

    while (read(terminalFd, &key, 1) == 1)
    {
        int keep = length;

        if (key == 27)
        {
            skipEscape();
            write(terminalFd, "\a", 1);
        }
        else if (key == 4 || key == savedTerminal.c_cc[VEOF])
        {
            if (length == 0)
            {
                write(terminalFd, "\n", 1);
                break;
            }
            write(terminalFd, "\a", 1);
        }
        else if (key == savedTerminal.c_cc[VERASE] ||
                 key == 8 || key == 127)
            keep = length > 0 ? length - 1 : 0;
        else if (key == savedTerminal.c_cc[VKILL])
            keep = 0;
        else if (key == 23 || key == savedTerminal.c_cc[VWERASE])
        {
            while (keep > 0 && line[keep - 1] == ' ')
                keep--;
            while (keep > 0 && line[keep - 1] != ' ')
                keep--;
        }
        else if (key == '\n')
        {
            write(terminalFd, "\n", 1);
            length = 0;
            continue;
        }
        else if (isprint(key))
        {
            if (length == WIDTH)
            {
                int start = length;

                if (key != ' ')
                    while (start > 0 && line[start - 1] != ' ')
                        start--;

                int moved = start > 0 ? length - start : 0;

                eraseCharacters(moved);
                write(terminalFd, "\n", 1);
                memmove(line, line + length - moved, (size_t)moved);
                length = moved;
                write(terminalFd, line, (size_t)length);
            }

            line[length++] = (char)key;
            write(terminalFd, &key, 1);
            continue;
        }
        else
            write(terminalFd, "\a", 1);

        eraseCharacters(length - keep);
        length = keep;
    }

    return 0;
}
