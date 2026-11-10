#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <stdint.h>

int printFile(char *path)
{
    struct stat info;

    if (lstat(path, &info) == -1)
    {
        perror(path);
        return 1;
    }

    char mode[] = "----------";
    const mode_t bits[] = {
        S_IRUSR, S_IWUSR, S_IXUSR,
        S_IRGRP, S_IWGRP, S_IXGRP,
        S_IROTH, S_IWOTH, S_IXOTH
    };
    const char permissions[] = "rwxrwxrwx";

    if (S_ISDIR(info.st_mode))
        mode[0] = 'd';
    else if (!S_ISREG(info.st_mode))
        mode[0] = '?';

    for (int i = 0; i < 9; i++)
        if (info.st_mode & bits[i])
            mode[i + 1] = permissions[i];

    char owner[32], group[32], size[32];
    struct passwd *user = getpwuid(info.st_uid);

    if (user != NULL)
        snprintf(owner, sizeof(owner), "%s", user->pw_name);
    else
        snprintf(owner, sizeof(owner), "%lu", (unsigned long)info.st_uid);

    struct group *entry = getgrgid(info.st_gid);

    if (entry != NULL)
        snprintf(group, sizeof(group), "%s", entry->gr_name);
    else
        snprintf(group, sizeof(group), "%lu", (unsigned long)info.st_gid);

    size[0] = '\0';
    if (S_ISREG(info.st_mode))
        snprintf(size, sizeof(size), "%lld", (long long)info.st_size);

    char date[32];
    struct tm *modified = localtime(&info.st_mtime);

    if (modified == NULL ||
        strftime(date, sizeof(date), "%Y-%m-%d %H:%M", modified) == 0)
    {
        fprintf(stderr, "%s: cannot format modification time\n", path);
        return 1;
    }

    size_t length = strlen(path);
    while (length > 1 && path[length - 1] == '/')
        path[--length] = '\0';

    const char *name = strrchr(path, '/');
    name = name == NULL ? path : name + 1;
    if (*name == '\0')
        name = "/";

    printf("%s %4lu %-16s %-16s %12s %s %s\n",
           mode, (unsigned long)info.st_nlink, owner, group, size, date, name);
    return 0;
}

int comparePaths(const void *left, const void *right)
{
    const char *const *a = left;
    const char *const *b = right;
    return strcmp(*a, *b);
}

int printPath(char *path)
{
    struct stat info;

    if (lstat(path, &info) == -1)
    {
        perror(path);
        return 1;
    }

    if (!S_ISDIR(info.st_mode))
        return printFile(path);

    DIR *directory = opendir(path);

    if (directory == NULL)
    {
        perror(path);
        return 1;
    }

    char **entries = NULL;
    size_t count = 0, capacity = 0;
    int status = 0;

    while (1)
    {
        errno = 0;
        struct dirent *entry = readdir(directory);

        if (entry == NULL)
        {
            if (errno != 0)
            {
                perror("readdir");
                status = 1;
            }
            break;
        }

        if (count == capacity)
        {
            if (capacity > SIZE_MAX / 2 / sizeof(*entries))
            {
                fprintf(stderr, "too many directory entries\n");
                status = 1;
                break;
            }

            size_t newCapacity = capacity == 0 ? 16 : capacity * 2;
            char **temporary = realloc(entries, newCapacity * sizeof(*entries));

            if (temporary == NULL)
            {
                perror("realloc");
                status = 1;
                break;
            }

            entries = temporary;
            capacity = newCapacity;
        }

        size_t size = strlen(path) + strlen(entry->d_name) + 2;
        char *fullPath = malloc(size);

        if (fullPath == NULL)
        {
            perror("malloc");
            status = 1;
            break;
        }

        snprintf(fullPath, size, "%s/%s", path, entry->d_name);
        entries[count++] = fullPath;
    }

    if (closedir(directory) == -1)
    {
        perror("closedir");
        status = 1;
    }

    if (count > 1)
        qsort(entries, count, sizeof(*entries), comparePaths);

    for (size_t i = 0; i < count; i++)
    {
        if (printFile(entries[i]) != 0)
            status = 1;

        free(entries[i]);
    }

    free(entries);
    return status;
}

int main(int argc, char *argv[])
{
    if (argc == 1)
    {
        char current[] = ".";
        return printPath(current);
    }

    int status = 0;

    for (int i = 1; i < argc; i++)
    {
        if (argc > 2)
            printf("%s:\n", argv[i]);

        if (printPath(argv[i]) != 0)
            status = 1;

        if (argc > 2 && i < argc - 1)
            putchar('\n');
    }

    return status;
}

