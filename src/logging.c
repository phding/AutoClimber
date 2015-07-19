#include "logging.h"

FILE * logfile;
bool console_enable = true;

void ERROR(const char *s)
{
    char *msg = strerror(errno);
    LOGE("%s: %s", s, msg);

}

void FATAL(const char *msg)
{
    LOGE("%s", msg);
    exit(-1);
}