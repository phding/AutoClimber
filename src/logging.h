#ifndef _logging_H
#define _logging_H

#include <stdio.h>
#include <time.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h> 

#define TIME_FORMAT "%Y-%m-%d %H:%M:%S"

extern FILE * logfile;
extern bool console_enable;
char logging_buffer[200];

#define LOGG(level, format, ...)                                                        \
 do {                                                                                   \
        if(logfile != NULL || console_enable) {                                         \
            time_t now = time(NULL);                                                    \
            char timestr[20];                                                           \
            strftime(timestr, 20, TIME_FORMAT, localtime(&now));                        \
            sprintf(logging_buffer, " %s " level ": " format "\n", timestr, ## __VA_ARGS__); \
        }                                                                               \
        if(console_enable == true){                                                     \
            printf("%s", logging_buffer);                                               \
        }                                                                               \
        if(logfile != NULL){                                                            \
            fprintf(logfile, "%s", logging_buffer);                                     \
            fflush(logfile);                                                            \
        }                                                                               \
    }                                                                                   \
    while (0)

#define LOGI(format, ...) LOGG("INFO", format , ## __VA_ARGS__)
#define LOGW(format, ...) LOGG("WARNING", format , ## __VA_ARGS__)
#define LOGE(format, ...) LOGG("ERROR", format , ## __VA_ARGS__)

void ERROR(const char *s);
void FATAL(const char *msg);

#endif 