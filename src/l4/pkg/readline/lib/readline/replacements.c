#include <string.h>
#include <unistd.h>
#include "tcap.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef DEBUG
static int _DEBUG = 1;
#else
static int _DEBUG = 0;
#endif

#define LOGd(l, a...) do { if (l) printf(a); } while(0)


int tputs( const char *str, int affcnt, int (*putc)(int))
{
    int i;
    for(i=0; i<strlen(str); i++)
    {
        (*putc)(str[i]);
    }
}

char *tgoto( const char* cap, int col, int row )
{
    char *goto_string;
    sprintf( goto_string, "\033[%d,%dH%s", row, col, cap );
    return goto_string;
}

int tgetflag( char *id )
{
    LOGd(_DEBUG,"tgetflag: %s\n", id);
    return 0;
}

int tcflow( int fd, int action )
{
    LOGd(_DEBUG,"tcflow: %d\n", action);
    return 0;
}

int tgetnum( char *id )
{
    LOGd(_DEBUG,"tgetnum: %s\n", id);
    return 0;
}

char *tgetstr( char *id, char **area )
{
    char *ret;

    LOGd(_DEBUG,"tgetstr: %s\n", id);
    // allocate if necessary
    if (!*area)
        *area = (char *)malloc(2); // 2 byte for a character and its terminating 0

    if (strcmp(id,"le")==0)
        *area = "\b";

    ret = *area;                    // save area`s value
    area++;                         // make area point to next entry

    return strdup(ret);             // finally return ret
}

int tgetent( char *bp, const char *name )
{
    LOGd(_DEBUG,"tgetent: %s, %s\n", bp, name);
    return 0;
}
