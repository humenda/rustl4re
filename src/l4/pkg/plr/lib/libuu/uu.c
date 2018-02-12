/**
 * \file   ferret/examples/merge_mon/uu.c
 * \brief  UUencoding functions, mostly from jdb
 *
 * \date   14/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

/* ENC is the basic 1 character encoding function to make a char printing */
#define	ENC(c) ((c) ? ((c) & 077) + ' ': '`')

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>

#include <zlib.h>

#include <l4/plr/uu.h>

#define MIN(a, b) ((a)<(b)?(a):(b))

#define LINE_IN 45

static z_stream strm;

/* Init. zlib data structures, available input data and start address
 */
static void init_z(size_t len, const void *start)
{
    int ret;
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit2(&strm, Z_BEST_COMPRESSION, Z_DEFLATED, 15 + 16, 9,
                       Z_DEFAULT_STRATEGY);
    assert(ret == Z_OK);

    strm.avail_in = len;
    strm.next_in  = (void *)start;  // oh why ...
}

static int get_next_chunk(void *dest, ssize_t n, int flush)
{
    int ret;

    strm.avail_out = n;
    strm.next_out = dest;
    ret = deflate(&strm, flush);
    assert(ret != Z_STREAM_ERROR);
    return n - strm.avail_out;
}

static void update_in_data(const unsigned char * buf, size_t * len)
{
    if (strm.avail_in == 0 && *len != 0)
    {
        strm.avail_in = *len;
        strm.next_in  = (void *)buf;  // oh why ...
        *len = 0;
    }
}

void uu_dumpz(const char * filename, const unsigned char * s_buf, size_t len)
{
    unsigned int ch;
    int n, ret;
    const unsigned char *p_buf;
    int out_i;
    static unsigned char in[LINE_IN + 3];  // some spare zeros for uuencode
    static unsigned char out[70];
    int flush = Z_NO_FLUSH;

    init_z(len, s_buf);

    // uuencode header
    printf("begin 644 %s.gz\n", filename);

    while (1)
    {
        // get next line of data
        n = get_next_chunk(in, LINE_IN, flush);
        if (n < LINE_IN && flush == Z_NO_FLUSH)
        {
            flush = Z_FINISH;  // time to clean up now
            n += get_next_chunk(in + n, LINE_IN - n, flush);
        }
        else if (n == 0 && flush == Z_FINISH)
            break;  // no more data available, leave loop
        memset(in + n, 0, sizeof(in) - n);

        // fill output buffer
        out_i = 0;
        // line length prefix
        ch = ENC(n);
        out[out_i++] = ch;
        for (p_buf = in; n > 0; n -= 3, p_buf += 3)
	{
            ch = *p_buf >> 2;
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = ((*p_buf << 4) & 060) | ((p_buf[1] >> 4) & 017);
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = ((p_buf[1] << 2) & 074) | ((p_buf[2] >> 6) & 03);
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = p_buf[2] & 077;
            ch = ENC(ch);
            out[out_i++] = ch;
	}
        // skip this newline for log output
        //out[out_i++] = '\n';
        out[out_i] = '\0';
        puts((char const*)out);  // write it
    }

    ret = deflateEnd(&strm);
    assert(ret == Z_OK);

    // uuencode footer
    printf("`\nend\n");
}

/* Also works in ring buffers with wrap-around
 */
void uu_dumpz_ringbuffer(const char * filename,
                         const void * _buffer, size_t buffer_len,
                         unsigned start_offset, size_t transfer_size)
{
    int ret;
    unsigned char * buffer = (unsigned char *)_buffer;
    static unsigned char in[LINE_IN + 3];  // some spare zeros for uuencode
    static unsigned char out[70];
    int flush = Z_NO_FLUSH;
    size_t first_len = MIN(transfer_size, buffer_len - start_offset);

    assert((unsigned)start_offset < buffer_len);
    assert(transfer_size <= buffer_len);

    // init. zlib stream for first chunk
    init_z(first_len, buffer + start_offset);
    transfer_size -= first_len;

    // uuencode header
    printf("begin 644 %s.gz\n", filename);

    while (1)
    {
        int out_i, n;
        unsigned int ch;
        const unsigned char * p_buf;

        // get next line of data
        n = get_next_chunk(in, LINE_IN, flush);
        update_in_data(buffer, &transfer_size);
        if (n < LINE_IN && flush == Z_NO_FLUSH)
        {
            flush = Z_FINISH;  // time to clean up now
            n += get_next_chunk(in + n, LINE_IN - n, flush);
            update_in_data(buffer, &transfer_size);
        }
        else if (n == 0 && flush == Z_FINISH)
            break;  // no more data available, leave loop
        memset(in + n, 0, sizeof(in) - n);

        // fill output buffer
        out_i = 0;
        // line length prefix
        ch = ENC(n);
        out[out_i++] = ch;
        for (p_buf = in; n > 0; n -= 3, p_buf += 3)
	{
            ch = *p_buf >> 2;
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = ((*p_buf << 4) & 060) | ((p_buf[1] >> 4) & 017);
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = ((p_buf[1] << 2) & 074) | ((p_buf[2] >> 6) & 03);
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = p_buf[2] & 077;
            ch = ENC(ch);
            out[out_i++] = ch;
	}
        // skip this newline for log output
        //out[out_i++] = '\n';
        out[out_i] = '\0';
        puts((char const *)out);  // write it
    }

    ret = deflateEnd(&strm);
    assert(ret == Z_OK);

    // uuencode footer
    printf("`\nend\n");
}

#if 0
void uu_dump(const char * filename, const buf_t * buf)
{
    unsigned int ch;
    int n;
    const unsigned char *p_buf;
    unsigned char *s_buf = buf->start;
    int out_i;
    static unsigned char in[48];
    static unsigned char out[70];
    size_t len = buf->write_pos - buf->start;

    // debug
    //printf("%hhx %hhx %hhx %hhx\n", ENC(8), ENC(64), ENC(4), ENC(2));

    // uuencode header
    printf("begin 644 %s\n", filename);

    while (len > 0)
    {
        // get next line of data
        if (len >= LINE_IN)
	{
            n = LINE_IN;
	}
        else
	{
            n = len;
	}
        memcpy(in, s_buf, n);
        memset(in + n, 0, sizeof(in) - n);
        len -= n;
        s_buf += n;

        // fill output buffer
        out_i = 0;
        // line length prefix
        ch = ENC(n);
        out[out_i++] = ch;
        for (p_buf = in; n > 0; n -= 3, p_buf += 3)
	{
            ch = *p_buf >> 2;
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = ((*p_buf << 4) & 060) | ((p_buf[1] >> 4) & 017);
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = ((p_buf[1] << 2) & 074) | ((p_buf[2] >> 6) & 03);
            ch = ENC(ch);
            out[out_i++] = ch;
            ch = p_buf[2] & 077;
            ch = ENC(ch);
            out[out_i++] = ch;
	}
        // skip this newline for log output
        //out[out_i++] = '\n';
        out[out_i] = '\0';

        // write it
        puts(out);
    }

    // uuencode footer
    printf("`\nend\n");
}
#endif
