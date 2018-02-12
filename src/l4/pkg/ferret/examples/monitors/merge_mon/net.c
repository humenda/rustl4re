/**
 * \file   ferret/examples/merge_mon/net.c
 * \brief  Network transfer stuff
 *
 * \date   17/01/2006
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2006-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stdio.h>
#include <string.h>

#include <l4/ore/uip-ore.h>
#include <l4/semaphore/semaphore.h>
#include <l4/lock/lock.h>

#include "net.h"
#include "main.h"
#include "buffer.h"

int net_connected = 0;
int conn_port;
l4semaphore_t net_sem = L4SEMAPHORE_UNLOCKED;

static int send_one_packet(int port)
{
    unsigned int len;

    if (buf.dump_pos >= buf.start + buf.size)  // end of whole buffer
        return 1;
    if (buf.dump_pos >= buf.write_pos)         // end of filled buffer
        return 2;

    // fixme: protect with buf_lock, not so easy right now, as buf->lock
    //        is also used to stop filling thread, so we would probably
    //        deadlock here ...
    //l4lock_down(&buf->lock);
    if (buf.write_pos - buf.dump_pos < 1400)
    {
        len = buf.write_pos - buf.dump_pos;
    }
    else
    {
        len = 1400;
    }
    //l4lock_up(&buf->lock);
    uip_ore_send(buf.dump_pos, len, port);
    buf.dump_pos += len;

    return 0;
}

void receive_cb(const void *buf_start, const unsigned size, unsigned port);
void ack_cb(void *addr, unsigned port);
void rexmit_cb(void *addr, unsigned size, unsigned port);
void connected_cb(const struct in_addr ip, unsigned port);
void timeout_cb(unsigned port);
void abrt_cb(unsigned port);
void close_cb(unsigned port);

void receive_cb(const void *buf_start, const unsigned size, unsigned port)
{
    printf("net: received something!\n");
}

void ack_cb(void *addr, unsigned port)
{
    int ret;

    printf("net: ack!\n");

    ret = send_one_packet(port);
    if (ret)
    {
        uip_ore_close(port);
    }
}

void rexmit_cb(void *addr, unsigned size, unsigned port)
{
    printf("net: retransmit!\n");
    uip_ore_send(addr, size, port);
}

void connected_cb(const struct in_addr ip, unsigned port)
{
    int ret;
    printf("net: connected!\n");

    net_connected = 1;
    conn_port = port;
    buf.dump_pos = buf.start;

    ret = send_one_packet(port);
    if (ret)
    {
        uip_ore_close(port);
    }
}

void timeout_cb(unsigned port)
{
    printf("net: timeout!\n");
    net_connected = 0;

    l4semaphore_up(&net_sem);
}

void abrt_cb(unsigned port)
{
    printf("net: abort!\n");
    net_connected = 0;

    l4semaphore_up(&net_sem);
}

void close_cb(unsigned port)
{
    printf("net: closed!\n");
    net_connected = 0;

    l4semaphore_up(&net_sem);
}

int net_init(const char * ip)
{
    uip_ore_config c;

    memset(&c, 0, sizeof(c));

    strcpy(c.ip, ip);
//    c.port_nr = port;

    c.recv_callback    = receive_cb;
    c.ack_callback     = ack_cb;
    c.rexmit_callback  = rexmit_cb;
    c.connect_callback = connected_cb;
    c.abort_callback   = abrt_cb;
    c.timeout_callback = timeout_cb;
    c.close_callback   = close_cb;

    uip_ore_initialize(&c);
    uip_thread = l4thread_create(uip_ore_thread, NULL, L4THREAD_CREATE_SYNC);

    return 0;
}
