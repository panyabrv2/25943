#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <ulimit.h>
#include <sys/resource.h>
#include <string.h>

extern char **environ;

int main(int argc, char *argv[])
{
    int option;

    while ((option = getopt(argc, argv, "ispuU:cC:dvV:")) != -1)
    {
        switch (option)
        {
            case 'i':
                printf("UID: %ld\n", (long)getuid());
                printf("EUID: %ld\n", (long)geteuid());
                printf("GID: %ld\n", (long)getgid());
                printf("EGID: %ld\n", (long)getegid());
                break;


            case 's':
                if (setpgid(0, 0) == -1)
                    perror("setpgid");
                else
                    printf("Process became group leader\n");
                break;


            case 'p':
                printf("PID: %ld\n", (long)getpid());
                printf("PPID: %ld\n", (long)getppid());
                printf("PGID: %ld\n", (long)getpgrp());
                break;


            case 'u':
                printf("ulimit: %ld\n", ulimit(UL_GETFSIZE));
                break;


            case 'U':
            {
                char *end;
                long value = strtol(optarg, &end, 10);

                if (*end != '\0' || value < 0)
                {
                    printf("Invalid value for -U\n");
                    break;
                }

                if (ulimit(UL_SETFSIZE, value) == -1)
                    perror("ulimit");
                else
                    printf("New ulimit: %ld\n", value);

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
                    printf("Core file size: unlimited\n");
                else
                    printf("Core file size: %llu bytes\n",
                           (unsigned long long)limit.rlim_cur);

                break;
            }


            case 'C':
            {
                char *end;
                unsigned long long value;

                struct rlimit limit;

                value = strtoull(optarg, &end, 10);

                if (*end != '\0')
                {
                    printf("Invalid value for -C\n");
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
                    printf("New core file size: %llu bytes\n", value);

                break;
            }


            case 'd':
            {
                char path[1024];

                if (getcwd(path, sizeof(path)) == NULL)
                    perror("getcwd");
                else
                    printf("Current directory: %s\n", path);

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
                char *equal = strchr(optarg, '=');

                if (equal == NULL)
                {
                    printf("Use: -Vname=value\n");
                    break;
                }

                *equal = '\0';

                if (setenv(optarg, equal + 1, 1) == -1)
                    perror("setenv");
                else
                    printf("%s=%s\n", optarg, equal + 1);

                *equal = '=';

                break;
            }


            case '?':
                printf("Unknown option or missing argument\n");
                break;
        }
    }

    return 0;
}
