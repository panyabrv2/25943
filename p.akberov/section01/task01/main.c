#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ulimit.h>
#include <sys/resource.h>
#include <string.h>

extern char **environ;

void printHelp(const char *name)
{
    printf("usage: %s [options]\n", name);
    printf("options are processed from right to left\n");
    printf("-h             show help\n");
    printf("-i             print uid, euid, gid, egid\n");
    printf("-s             become process group leader\n");
    printf("-p             print pid, ppid, pgid\n");
    printf("-u             print file size limit in 512-byte blocks\n");
    printf("-U value       change file size limit in 512-byte blocks\n");
    printf("-c             print core file size limit in bytes\n");
    printf("-C size        change core file size limit in bytes\n");
    printf("-d             print current directory\n");
    printf("-v             print environment variables\n");
    printf("-V name=value  add or change environment variable\n");
}

int main(int argc, char *argv[])
{
    int option;
    int options[argc];
    char *arguments[argc];
    int count = 0;

    if (argc == 1)
    {
        printHelp(argv[0]);
        return 0;
    }

    while ((option = getopt(argc, argv, "hispuU:cC:dvV:")) != -1)
    {
        if (option == 'h')
        {
            printHelp(argv[0]);
            return 0;
        }

        if (option == '?')
        {
            fprintf(stderr, "use -h for help\n");
            return 1;
        }

        options[count] = option;
        arguments[count] = optarg;
        count++;
    }

    if (optind < argc)
    {
        fprintf(stderr, "unexpected argument: %s\n", argv[optind]);
        fprintf(stderr, "use -h for help\n");
        return 1;
    }

    for (int i = count - 1; i >= 0; i--)
    {
        option = options[i];
        char *argument = arguments[i];

        switch (option)
        {
            case 'i':
                printf("uid: %ld\n", (long)getuid());
                printf("euid: %ld\n", (long)geteuid());
                printf("gid: %ld\n", (long)getgid());
                printf("egid: %ld\n", (long)getegid());
                break;

            case 's':
                if (setpgid(0, 0) == -1)
                    perror("setpgid");
                else
                    printf("process group changed\n");
                break;

            case 'p':
                printf("pid: %ld\n", (long)getpid());
                printf("ppid: %ld\n", (long)getppid());
                printf("pgid: %ld\n", (long)getpgrp());
                break;

            case 'u':
                printf("ulimit: %ld\n", ulimit(UL_GETFSIZE));
                break;

            case 'U':
            {
                char *end;
                long value = strtol(argument, &end, 10);

                if (*end != '\0' || value < 0)
                {
                    printf("invalid value for -U\n");
                    break;
                }

                if (ulimit(UL_SETFSIZE, value) == -1)
                    perror("ulimit");
                else
                    printf("ulimit changed to %ld\n", value);

                break;
            }

            case 'c':
            {
                struct rlimit limit;

                if (getrlimit(RLIMIT_CORE, &limit) == -1)
                {
                    perror("getrlimit");
                    break;
                }

                if (limit.rlim_cur == RLIM_INFINITY)
                    printf("core file size: unlimited\n");
                else
                    printf("core file size: %llu bytes\n",
                           (unsigned long long)limit.rlim_cur);

                break;
            }

            case 'C':
            {
                char *end;
                unsigned long long value;
                struct rlimit limit;

                value = strtoull(argument, &end, 10);

                if (*end != '\0')
                {
                    printf("invalid value for -C\n");
                    break;
                }

                if (getrlimit(RLIMIT_CORE, &limit) == -1)
                {
                    perror("getrlimit");
                    break;
                }

                limit.rlim_cur = (rlim_t)value;

                if (setrlimit(RLIMIT_CORE, &limit) == -1)
                    perror("setrlimit");
                else
                    printf("core file size changed to %llu bytes\n", value);

                break;
            }

            case 'd':
            {
                char path[1024];

                if (getcwd(path, sizeof(path)) == NULL)
                    perror("getcwd");
                else
                    printf("current directory: %s\n", path);

                break;
            }

            case 'v':
            {
                char **env = environ;

                while (*env != NULL)
                {
                    printf("%s\n", *env);
                    env++;
                }

                break;
            }

            case 'V':
            {
                char *equal = strchr(argument, '=');

                if (equal == NULL)
                {
                    printf("use -Vname=value\n");
                    break;
                }

                *equal = '\0';

                if (setenv(argument, equal + 1, 1) == -1)
                    perror("setenv");
                else
                    printf("variable changed: %s=%s\n",
                           argument, equal + 1);

                *equal = '=';
                break;
            }
        }
    }

    return 0;
}
