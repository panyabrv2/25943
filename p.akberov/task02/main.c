#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main()
{
    time_t now;
    time(&now);

    now -= 8 * 60 * 60;

    struct tm *sp = gmtime(&now);

    printf("%02d/%02d/%d %02d:%02d:%02d PST\n",
           sp->tm_mon + 1,
           sp->tm_mday,
           sp->tm_year + 1900,
           sp->tm_hour,
           sp->tm_min,
           sp->tm_sec);

    return 0;
}
