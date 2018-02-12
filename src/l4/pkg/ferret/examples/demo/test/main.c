/**
 * \file   ferret/examples/test/main.c
 * \brief  Tests some functions
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
#include <stdio.h>

//#include <l4/log/l4log.h>
//#include <l4/util/l4_macros.h>
//#include <l4/util/util.h>
//#include <l4/sys/syscalls.h>
//#include <l4/sys/kdebug.h>
#include <l4/sys/ktrace.h>
#include <l4/sys/ktrace_events.h>

#include <l4/ferret/util.h>

char LOG_tag[9] = "FerTest";

int main(int argc, char* argv[])
{
    char buf[1000];
    int ret, i;

    l4_tracebuffer_status_t * tbuf;
    l4_tracebuffer_entry_t test;

    tbuf = fiasco_tbuf_get_status();
    printf("%p, 0:(%p, %lu, %llu), 1:(%p, %lu, %llu)\n", tbuf,
           (void *)tbuf->tracebuffer0, tbuf->size0, tbuf->version0,
           (void *)tbuf->tracebuffer1, tbuf->size1, tbuf->version1);

    printf("%s test\n", "testing");

    ret = ferret_util_pack("LLLLxxhxbQ00010s10p", buf, 1, 2, 3, 4, 5, 6, 7LL,
                            "abcd", "xyzqw");
    printf("ret = %d\n", ret);

    for (i = 0; i < ret; ++i)
    {
        printf("%hhu, ", buf[i]);
    }
    puts("\n");

    printf("sizeof %d\n", sizeof(l4_tracebuffer_entry_t));
    printf("type offset %d\n", ((char *)&test.type) - ((char *)&test));
    printf("sizeof %d\n", sizeof(test.m.pf));
    printf("sizeof %d\n", sizeof(test.m.ipc));
    printf("sizeof %d\n", sizeof(test.m.ipc_res));
    printf("sizeof %d\n", sizeof(test.m.ipc_trace));
    printf("sizeof %d\n", sizeof(test.m.ke));
    printf("sizeof %d\n", sizeof(test.m.ke_reg));
    printf("sizeof %d\n", sizeof(test.m.unmap));
    printf("sizeof %d\n", sizeof(test.m.shortcut_failed));
    printf("sizeof %d\n", sizeof(test.m.shortcut_succeeded));
    printf("sizeof %d\n", sizeof(test.m.context_switch));
    printf("sizeof %d\n", sizeof(test.m.exregs));
    printf("sizeof %d\n", sizeof(test.m.breakpoint));
    printf("sizeof %d\n", sizeof(test.m.trap));
    printf("sizeof %d\n", sizeof(test.m.pf_res));
    printf("sizeof %d\n", sizeof(test.m.sched));
    printf("sizeof %d\n", sizeof(test.m.preemption));
    printf("sizeof %d\n", sizeof(test.m.id_nearest));
    printf("sizeof %d\n", sizeof(test.m.jean1));
    printf("sizeof %d\n", sizeof(test.m.task_new));
    printf("sizeof %d\n", sizeof(test.m.fit));
    for (i = 0;
         i < (tbuf->size0 + tbuf->size1) / sizeof(l4_tracebuffer_entry_t) &&
             i < 100;
         ++i)
    {
        printf("i = %d, %d, type = %hhd, %p\n", i,
               tbuf->tracebuffer0[i].number,
               tbuf->tracebuffer0[i].type,
               &tbuf->tracebuffer0[i]);
    }

    return 0;
}
