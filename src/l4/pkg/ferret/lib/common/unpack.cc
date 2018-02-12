/**
 * \file   ferret/lib/util/unpack.c
 * \brief  Utility functions.
 *
 * \date   29/11/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdarg.h>
#include <string.h>

#include <l4/ferret/util.h>

int ferret_util_unpack(const char *fmt, const void * _buf, ...)
{
    va_list argp;
    int got;

    size_t len = 0;
    int has_count = 0;
    size_t count = 1, i;
    char * ptr;
    unsigned char * buf = const_cast<unsigned char*>(static_cast<const unsigned char *>(_buf));

    va_start(argp, _buf);
    got = 0;
    for (; *fmt; ++fmt)
    {
        // numbers
        if (*fmt >= '0' && *fmt <= '9')
        {
            if (! has_count)
                count = 0;    // first digit ...
            else
                count *= 10;  // ... next digit
            count += *fmt - '0';
            has_count = 1;
            continue;
        }
        // strings
        switch (*fmt)
        {
            case 's':  // c string, 0-terminated
                ptr = static_cast<char *>(va_arg(argp, char *));
                len = strlen(reinterpret_cast<char *>(buf));
                //LOG("%p, %d, %d, %d", ptr, count, len, has_count);
                if (len < count || ! has_count)
                {
                    strncpy(ptr, reinterpret_cast<char *>(buf), len);
                    if (has_count)  // zero out remainder
                    {
                        memset(ptr + len, 0, count - len + 1);
                        //LOG("%p, %d, %d, %d", o + len, count, len, has_count);
                        buf += count + 1;
                        got += count + 1;
                    }
                    else
                    {
                        buf += len + 1;
                        got += len + 1;
                    }
                }
                else
                {
                    strncpy(ptr, reinterpret_cast<char *>(buf), count);
                    *(ptr + count) = '\0';
                    buf += count + 1;
                    got += count + 1;
                }
                count     = 0;
                has_count = 0;
                continue;
            case 'p':  // pascal string, leading length byte
                ptr = static_cast<char *>(va_arg(argp, char *));
                len = static_cast<unsigned char>(*buf);  // read length prefix
                buf += 1;
                got += 1;
                if (count > 255)
                    count = 255;  // truncate string at 255 bytes

                if (len < count || ! has_count)
                {
                    strncpy(ptr, reinterpret_cast<char *>(buf), len);
                    *(ptr + len + 1) = '\0';
                    if (has_count)  // zero out remainder
                    {
                        buf += count;
                        got += count;
                    }
                    else
                    {
                        buf += len;
                        got += len;
                    }
                }
                else
                {
                    strncpy(ptr, reinterpret_cast<char *>(buf), count);
                    *(ptr + count + 1) = '\0';
                    buf += count;
                    got += count;
                }
                count     = 0;
                has_count = 0;
                continue;
        }
        // other stuff
        for (i = 0; i < count; ++i)
        {
            switch (*fmt)
            {
            case 'x':  // padding byte
                buf += 1;
                break;
            case 'b':
            case 'B':  // byte
                *(static_cast<char *>(va_arg(argp, void *)))
				                      = *(reinterpret_cast<char *>(buf));
                buf += 1;
                got += 1;
                break;
            case 'h':
            case 'H':  // short (16 bit)
                *(static_cast<uint16_t *>(va_arg(argp, void *))) 
				                          = *(reinterpret_cast<uint16_t *>(buf));
                buf += 2;
                got += 2;
                break;
            case 'l':
            case 'L':  // long (32 bit)
                *(static_cast<uint32_t *>(va_arg(argp, void *))) 
				                          = *(reinterpret_cast<uint32_t *>(buf));
                buf += 4;
                got += 4;
                break;
            case 'q':
            case 'Q':  // long long (64 bit)
                *(static_cast<uint64_t *>(va_arg(argp, void *))) 
				                          = *(reinterpret_cast<uint64_t *>(buf));
                buf += 8;
                got += 8;
                break;
            default:
                return -1;
            }
        }
        count = 1;
        has_count = 0;
    }
    va_end(argp);
    return got;
}
