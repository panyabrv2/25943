#include <stdio.h>
#include <time.h>
#include <stdlib.h>

int main()
{
    time_t now;
    time(&now);

    now -= 8 * 60 * 60;

    struct tm *pst = gmtime(&now);
    struct tm *pdt = gmtime(&pdt_time);

    printf("%02d/%02d/%d %02d:%02d:%02d PST\n",
           pst->tm_mon + 1,
           pst->tm_mday,
           pst->tm_year + 1900,
           pst->tm_hour,
           pst->tm_min,
           pst->tm_sec);

    printf("%02d/%02d/%d %02d:%02d:%02d PDT\n",
           pdt->tm_mon + 1,
           pdt->tm_mday,
           pdt->tm_year + 1900,
           pdt->tm_hour,
           pdt->tm_min,
           pdt->tm_sec);

    return 0;
}
