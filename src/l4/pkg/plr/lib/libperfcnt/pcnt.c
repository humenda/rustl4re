#include <l4/sys/types.h>
#include <l4/sys/thread.h>
#include <stdio.h>
#include <l4/re/env.h>
#include <l4/plr/perf.h>

enum {
    THREAD_PERF_OP = 12,
	SETUP_PERF     = 0x5E,
	READ_PERF      = 0xAE,
};

enum {
	/* Cache events */
	L1I_MISSES_U  = 0x02,
	L1I_MISSES_E  = 0x80,
	L3_MISSES_U   = 0x41,
	L3_MISSES_E   = 0x2E,
	L2_MISSES_U   = 0x02,
	L2_MISSES_E   = 0x24,
	
	/* TLB events */
	ITLB_MISSES_U = 0x01,
	ITLB_MISSES_E = 0x85,
	DTLB_MISSES_U = 0x01,
	DTLB_MISSES_E = 0x49,

	/* Runtime events */
	CLK_UNHALTED_THR_U = 0x00,
	CLK_UNHALTED_THR_E = 0x3C,
	CLK_UNHALTED_REF_U = 0x01,
	CLK_UNHALTED_REF_E = 0x3C,

	USR_FLAG      = (1 << 16),
	OS_FLAG       = (1 << 17),
	ENABLE_FLAG   = (1 << 22),
};

static void perfcall(l4_umword_t op, l4_umword_t arg1, l4_umword_t arg2,
                     l4_umword_t arg3, l4_umword_t arg4)
{
	l4_msg_regs_t* mr = l4_utcb_mr_u(l4_utcb());
	mr->mr[1] = THREAD_PERF_OP;
	mr->mr[2] = op;
	mr->mr[3] = arg1;
	mr->mr[4] = arg2;
	mr->mr[5] = arg3;
	mr->mr[6] = arg4;

	l4_thread_stats_time_u(l4re_global_env->main_thread, l4_utcb());
}

void perfconf(void)
{
	l4_umword_t ev1, ev2, ev3, ev4;
	
	ev1  = 0;
	//ev1  = CLK_UNHALTED_REF_E & 0xFF;
	//ev1 |= ((CLK_UNHALTED_REF_U & 0xFF) << 8);
	//ev1 |= USR_FLAG;
	//ev1 |= OS_FLAG;
	//ev1 |= ENABLE_FLAG;
	
	ev2  = 0;
    //ev2  = CLK_UNHALTED_THR_E & 0xFF;
    //ev2 |= ((CLK_UNHALTED_THR_U & 0xFF) << 8);
    //ev2 |= OS_FLAG;
    //ev2 |= ENABLE_FLAG;

	ev3 = 0;
	ev3 = L2_MISSES_E & 0xFF;
	ev3 |= ((L2_MISSES_E & 0xFF) << 8);
	ev3 |= USR_FLAG;
	ev3 |= OS_FLAG;
	ev3 |= ENABLE_FLAG;

	ev4 = 0;
	ev4 = L3_MISSES_E & 0xFF;
	ev4 |= ((L3_MISSES_U & 0xFF) << 8);
	ev4 |= USR_FLAG;
	ev4 |= OS_FLAG;
	ev4 |= ENABLE_FLAG;
	
	perfcall(SETUP_PERF, ev1, ev2, ev3, ev4);
}

void perfread2(l4_uint64_t *v1,l4_uint64_t *v2,l4_uint64_t *v3,l4_uint64_t *v4)
{
	perfcall(READ_PERF, 0, 0, 0, 0);

	l4_msg_regs_t *mr = l4_utcb_mr_u(l4_utcb());
	*v1 = mr->mr[0] | ( (l4_uint64_t)mr->mr[1] << 32 );
	*v2 = mr->mr[2] | ( (l4_uint64_t)mr->mr[3] << 32 );
	*v3 = mr->mr[4] | ( (l4_uint64_t)mr->mr[5] << 32 );
	*v4 = mr->mr[6] | ( (l4_uint64_t)mr->mr[7] << 32 );
}

void perfread(void)
{
	l4_uint64_t v1, v2, v3, v4;
	perfread2(&v1, &v2, &v3, &v4);

	printf("READ %016llx %016llx %016llx %016llx\n", v1, v2, v3, v4);
}

