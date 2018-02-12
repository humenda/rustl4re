/**
 * \file   ferret/lib/util/pack.c
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
#include <sys/types.h>

#include <l4/ferret/util.h>

// format modifiers "xbBhHlLqQsp"

int ferret_util_pack(const char *fmt, void * _buf, ...)
{
    unsigned char * buf = static_cast<unsigned char *>(_buf);
    va_list argp;
    int written;

    uint64_t q;
    uint32_t l;
    uint16_t s;
    uint8_t  c;

    size_t len = 0;
    int has_count = 0;
	size_t count = 1, i;
    char * ptr;

    va_start(argp, _buf);
    written = 0;
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
                ptr = static_cast<char*>(va_arg(argp, char *));
                len = strlen(ptr);
                //LOG("%p, %d, %d, %d", ptr, count, len, has_count);
                if (len < count || ! has_count)
                {
                    strncpy(reinterpret_cast<char*>(buf), ptr, len);
                    if (has_count)  // zero out remainder
                    {
                        memset(buf + len, 0, count - len + 1);
                        //LOG("%p, %d, %d, %d", o + len, count, len, has_count);
                        buf     += count + 1;
                        written += count + 1;
                    }
                    else
                    {
                        buf     += len + 1;
                        written += len + 1;
                    }
                }
                else
                {
                    strncpy(reinterpret_cast<char *>(buf), ptr, count);
                    *(buf + count) = '\0';
                    buf     += count + 1;
                    written += count + 1;
                }
                count     = 0;
                has_count = 0;
                continue;
            case 'p':  // pascal string, leading length byte
                ptr = static_cast<char *>(va_arg(argp, char *));
                len = strlen(ptr);
                if (len > 255)
                    len = 255;  // truncate string at 255 bytes
                if (count > 255)
                    count = 255;  // truncate string at 255 bytes

                if (len < count || ! has_count)
                {
                    if (has_count)  // prefix output with length
                        *buf = static_cast<uint8_t>(count);
                    else
                        *buf = static_cast<uint8_t>(len);
                    buf += 1;
                    written += 1;
                    strncpy(reinterpret_cast<char *>(buf), ptr, len);
                    if (has_count)  // zero out remainder
                    {
                        memset(buf + len, 0, count - len);
                        buf     += count;
                        written += count;
                    }
                    else
                    {
                        buf     += len;
                        written += len;
                    }
                }
                else
                {
                    *buf = static_cast<uint8_t>(count);  // prefix output with length
                    buf += 1;
                    written += 1;
                    strncpy(reinterpret_cast<char *>(buf), ptr, count);
                    buf     += count;
                    written += count;
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
                // fixme: we should not do this for performance reasons
                *(buf) = '\0';  // zero out padding stuff
                buf += 1;
                break;
            case 'b':
            case 'B':  // byte
                c = static_cast<uint8_t>(va_arg(argp, int));
                *buf = c;
                buf += 1;
                written += 1;
                break;
            case 'h':
            case 'H':  // short (16 bit)
                s = static_cast<uint16_t>(va_arg(argp, int));
                *((uint16_t *)buf) = s;
                buf += 2;
                written += 2;
                break;
            case 'l':
            case 'L':  // long (32 bit)
                l = static_cast<uint32_t>(va_arg(argp, uint32_t));
                *((uint32_t *)buf) = l;
                buf += 4;
                written += 4;
                break;
            case 'q':
            case 'Q':  // long long (64 bit)
                q = static_cast<uint64_t>(va_arg(argp, uint64_t));
                //LOG("Q:%lld", q);
                *((uint64_t *)buf) = q;
                buf += 8;
                written += 8;
                break;
            default:
                return -1;
            }
        }
        count = 1;
        has_count = 0;
    }
    va_end(argp);
    return written;
}
