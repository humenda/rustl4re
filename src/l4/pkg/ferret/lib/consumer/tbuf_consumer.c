/**
 * \file   ferret/lib/sensor/tbuf_consumer.c
 * \brief  Tracebuffer consumer functions.
 *
 * \date   06/12/2005
 * \author Martin Pohlack  <mp26@os.inf.tu-dresden.de>
 */
/*
 * (c) 2005-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <stdlib.h>

// fixme: debug
//#include <l4/log/l4log.h>


#include <l4/ferret/types.h>
#include <l4/ferret/sensors/tbuf.h>
#include <l4/ferret/sensors/tbuf_consumer.h>

int ferret_tbuf_get(ferret_tbuf_moni_t * tbuf, l4_tracebuffer_entry_t * el)
{
    /* 1. compute index from next_read
     * 2. compare count in element + version with next_read
     * 3. copy element
     * 4. check count again
     */

    unsigned int index, ec;
    uint64_t ecl;
    uint64_t version, version2, version3;

    while (1)
    {
        index = ((uint32_t)(tbuf->next_read - 1)) & tbuf->mask;

        if (index < tbuf->count2)
            version = tbuf->status->version0;
        else
            version = tbuf->status->version1;

        // fixme: memory barrier

        ec = tbuf->status->tracebuffer0[index].number;

        if (tbuf->status->tracebuffer0[index].type == l4_ktrace_tbuf_unused)
        {
            //LOG("XXXu %p, %d", tbuf->status->tracebuffer0, index);
            return -1;  // unused event, no more data available
        }

        //LOG("version %llu, number %u", version, ec);
        ecl = ((ec - 1) | ((version >> tbuf->offset) << (sizeof(ec) * 8)));
        if (ecl >= (version + 1) * tbuf->count)
            ecl -= 1LL << (sizeof(ec) * 8);  // 2 ^ 32
        ++ecl;
        //LOG("next %llu, ecl %llu", tbuf->next_read, ecl);

        if (index < tbuf->count2)
            version2 = tbuf->status->version0;
        else
            version2 = tbuf->status->version1;

        if (version != version2)
            continue;  // found inconsistent values, retry

        if (ecl < tbuf->next_read)
        {
            //LOG("XXXl %p, %d", tbuf->status->tracebuffer0, index);
            return -1;  // we have reached the limit -> no new data available
        }
        else if (ecl > tbuf->next_read)
        {
            //LOG("XXXlost");
            // fixme: find better way to compute new element
            tbuf->lost += ecl - tbuf->next_read;
            tbuf->next_read = ecl;
            continue;
        }
        memcpy(el, &tbuf->status->tracebuffer0[index],
               sizeof(l4_tracebuffer_entry_t));

        // fixme: enforce new memory access
        if (index < tbuf->count2)
            version3 = tbuf->status->version0;
        else
            version3 = tbuf->status->version1;
        if (ec != tbuf->status->tracebuffer0[index].number ||
            version2 != version3)
        {
            continue;  // we might have lost this event -> retry
        }
        ++(tbuf->next_read);
        return 0;  // everything ok, got data
    }
}

void ferret_tbuf_init_consumer(void ** addr)
{
    ferret_tbuf_moni_t * temp;
    int ld, count;

    temp = malloc(sizeof(ferret_tbuf_moni_t));

    // setup some pointers
    temp->status = *addr;  // store pointer to global structure

    // copy header
    temp->header.major    = FERRET_TBUF_MAJOR;
    temp->header.minor    = FERRET_TBUF_MINOR;
    temp->header.instance = FERRET_TBUF_INSTANCE;
    temp->header.type     = FERRET_TBUF;

    // init. local information
    temp->lost      = 0;
    temp->next_read = 1;  // fixme: set this to current element
    temp->size      = temp->status->size0 + temp->status->size1;
    temp->count     = temp->size / sizeof(l4_tracebuffer_entry_t);
    temp->count2    = temp->count / 2;
    temp->mask      = temp->count - 1;

    // compute log2(count)
    for (ld = 0, count = temp->count; count > 1; ++ld)
        count = count / 2;

    temp->offset    = sizeof(unsigned int) * 8 - ld;

    *addr = temp;
}

void ferret_tbuf_free_consumer(void ** addr)
{
    void * temp = ((ferret_tbuf_moni_t *)(*addr))->status;

    free(*addr);

    *addr = temp;
}
