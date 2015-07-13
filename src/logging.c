#include "logging.h"

FILE * logfile;

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