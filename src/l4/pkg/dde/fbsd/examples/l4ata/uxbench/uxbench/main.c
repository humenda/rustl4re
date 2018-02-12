#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ALIGN 512

#define BLOCK_SIZE         512
#define STARTBLOCKCOUNT      1  // start with 1 block per request
                                //   doubled per step
#define STARTREQUESTS  16*1024  // number of requests per pass
#define STEPS               13  // number of steps
#define BEGINHALVING         5  // halve number of requests beginning at step 5
#define PASSES             200  // get 200 samples

// scaler for my Athlon 500
//const unsigned int l4_scaler_tsc_to_ns=268958300; // +-1000
// scaler for os Celeron 900
//const unsigned int l4_scaler_tsc_to_ns=148792300; // +-800
// scaler for my Athlon 2000
//const unsigned int l4_scaler_tsc_to_ns=67109400; // +-300
// scaler for ch12 Celeron 1700
const unsigned int l4_scaler_tsc_to_ns=78772200; // +-800

typedef unsigned long long l4_cpu_time_t;
typedef unsigned int l4_uint32_t;

typedef struct pass {
	unsigned int blockcount;
	unsigned int requests;
	l4_uint32_t ps, pns, ts, tns;
} pass_t;

pass_t results[STEPS*PASSES];

static inline l4_cpu_time_t l4_rdtsc(void) {
    l4_cpu_time_t v;

    __asm__ __volatile__
	("				\n\t"
	 ".byte 0x0f, 0x31		\n\t"
	/*"rdtsc\n\t"*/
	:
	"=A" (v)
	: /* no inputs */
	);

    return v;
}

static inline l4_cpu_time_t l4_rdpmc(int nr) {
    l4_cpu_time_t v;

    __asm__ __volatile__ (
	 "rdpmc				\n\t"
	:
	"=A" (v)
	: "c" (nr)
	);

    return v;
}

static inline void l4_tsc_to_s_and_ns(l4_cpu_time_t tsc, l4_uint32_t *s, l4_uint32_t *ns) {
    l4_uint32_t dummy;
    __asm__
	("				\n\t"
	 "movl  %%edx, %%ecx		\n\t"
	 "mull	%4			\n\t"
	 "movl	%%ecx, %%eax		\n\t"
	 "movl	%%edx, %%ecx		\n\t"
	 "mull	%4			\n\t"
	 "addl	%%ecx, %%eax		\n\t"
	 "adcl	$0, %%edx		\n\t"
	 "movl  $1000000000, %%ecx	\n\t"
	 "shld	$5, %%eax, %%edx	\n\t"
	 "shll	$5, %%eax		\n\t"
	 "divl  %%ecx			\n\t"
	:"=a" (*s), "=d" (*ns), "=&c" (dummy)
	: "A" (tsc), "g" (l4_scaler_tsc_to_ns)
	);
}

static int checkintegrity(l4_cpu_time_t tsc1, l4_cpu_time_t tsc2, l4_cpu_time_t pmc1, l4_cpu_time_t pmc2) {
	l4_uint32_t ts, tns, ps, pns;

	if ( (tsc2 < tsc1) || (pmc2 < pmc1) ) return 1;

	l4_tsc_to_s_and_ns(tsc2-tsc1, &ts, &tns);
	l4_tsc_to_s_and_ns(pmc2-pmc1, &ps, &pns);

	if ( (ts > 1200) || (ps>ts) )
		return 2;

	return 0;
}

static void info_loop(void *arg) __attribute((unused));
static void info_loop(void *arg) {
	l4_cpu_time_t tsc1, pmc1, tsc2, pmc2;
	l4_uint32_t ts, tns, ps, pns;

	tsc2 = l4_rdtsc();
	pmc2 = l4_rdpmc(0);
	while (1) {
		tsc1 = tsc2;
		pmc1 = pmc2;
		sleep(10);
		tsc2 = l4_rdtsc();
		pmc2 = l4_rdpmc(0);
		if (checkintegrity(tsc1, tsc2, pmc1, pmc2)) {
			fprintf(stderr, "# timestamp- or performance counter overrun; repeating last pass\n");
			continue;
		}
		l4_tsc_to_s_and_ns(tsc2-tsc1, &ts, &tns);
		l4_tsc_to_s_and_ns(pmc2-pmc1, &ps, &pns);
		fprintf(stderr,
				"%8u" "\t"
				"%4u.%03u" "\t"
				"%4u.%03u" "\t"
				"%8.3f" "\t"
				"%8.3f" "\n",
				0,
				ps, pns/1000000,
				ts, tns/1000000,
				100.0*(pmc2-pmc1)/(tsc2-tsc1),
				0.0
		);
		if (arg) {
			if (! --arg) break;
		}
	}
}

int main(int argc, char **argv) {
	int disk;
	void *bufp;
	unsigned buf;

	char *para_disk = NULL;
	int   para_idle = 1;
	int   scan_err  = 0;

	{
		int arg;
		for (arg=1; arg < argc; arg++) {
			if (argv[arg][0] == '-') {
				if (argv[arg][0] == '-') {
					if (! strcmp(argv[arg], "--noidle"))
						para_idle = 0;
					else
						goto have_scan_err;
				} else
					goto have_scan_err;
			} else {
				if (para_disk) {
					fprintf(stderr, "more than one disk parameter!\n");
					scan_err = 1;
					break;
				} else
					para_disk = argv[arg];
			}
			continue;
have_scan_err:
			fprintf(stderr, "unknown parameter: %s\n", argv[arg]);
			scan_err = 1;
			break;
		}
	}

	if (! para_disk) {
		fprintf(stderr, "missing disk parameter!\n");
		scan_err = 1;
	}

	if (scan_err) {
		fprintf(stderr, "\n");
		fprintf(stderr, "  uxbench [--noidle] <special file>\n");
		fprintf(stderr, "\n");
		fprintf(stderr, "  eg. uxbench /dev/hdc\n");
		fprintf(stderr, "\n");
		return -1;
	}
	
	disk = open(para_disk, O_RDONLY|O_DIRECT|O_SYNC);
	if (disk<0) {
		fprintf(stderr, "error opening \"%s\"\n", para_disk);
		return -1;
	}

	bufp = malloc((STARTBLOCKCOUNT<<STEPS)*BLOCK_SIZE + ALIGN-1);
	buf = (((unsigned)bufp + ALIGN-1) / ALIGN) * ALIGN;
	bufp = (void*) buf;
	if (! bufp) {
		fprintf(stderr, "error allocating buffer\n");
		return -1;
	}

	fprintf(stderr, "# l4_scaler_tsc_to_ns = %d\n", l4_scaler_tsc_to_ns);
	fprintf(stderr, "# block size = %d Byte\n", BLOCK_SIZE);
	fprintf(stderr, "#\n");
	fprintf(stderr, "# blocks\t   cpu s\t  real s\t usage %%\t    MB/s\n");

	if (para_idle) {
		fprintf(stderr, "# %d time(s) about 10 secs idle\n", 1);
		info_loop((void*) 1);
		fprintf(stderr, "#\n");
	} else
		fprintf(stderr, "# skipping idle pass(es) (--noidle)\n");

	int bpr = STARTBLOCKCOUNT;
	unsigned int requests = STARTREQUESTS;
	int step;
	int residx = 0;
	for (step=0; step < STEPS; step++,bpr<<=1) {
		long long amount;

		if (step >= BEGINHALVING)
			requests = requests >> 1;

		amount = 1ll * requests * bpr * BLOCK_SIZE;

		fprintf(stderr,
				"# step %d: %d pass(es) w/ %d reqs of %d block(s) = %0.1f MB\n",
				step+1, PASSES, requests, bpr, 1.0*amount/(1<<20)
		);

		int passesleft;
		for (passesleft=PASSES; passesleft; passesleft--) {
			l4_cpu_time_t tsc2, pmc2;
			l4_uint32_t ts, tns, ps, pns;
			l4_cpu_time_t tsc1, pmc1;

			results[residx].requests   = requests;
			results[residx].blockcount = bpr;

			// print status sometimes
			if (! (passesleft & 0x0F))
				fprintf(stderr, "# step %d: %d/%d passes remaining\n", step+1, passesleft, PASSES);

//			sleep(1);
			tsc1 = l4_rdtsc();
			pmc1 = l4_rdpmc(0);

			int reqnum;
			for (reqnum=requests; reqnum; reqnum--) {
				ssize_t count;

				lseek(disk, 0L, SEEK_SET);
				count = read(disk, bufp, bpr * BLOCK_SIZE);
				if (count != bpr * BLOCK_SIZE) {
					perror("error reading");
					return -1;
				}
			}

			tsc2 = l4_rdtsc();
			pmc2 = l4_rdpmc(0);
			if (checkintegrity(tsc1, tsc2, pmc1, pmc2)) {
				fprintf(stderr, "# timestamp- or performance counter overrun; repeating last pass\n");
				passesleft++;
				continue;
			}
			l4_tsc_to_s_and_ns(tsc2-tsc1, &ts, &tns);
			l4_tsc_to_s_and_ns(pmc2-pmc1, &ps, &pns);

			results[residx].ts  = ts;
			results[residx].tns = tns;
			results[residx].ps  = ps;
			results[residx].pns = pns;

			fprintf(stderr,
					"%8u" "\t"
					"%4u.%03u" "\t"
					"%4u.%03u" "\t"
					"%8.3f" "\t"
					"%8.3f" "\n",
					bpr,
					ps, pns/1000000,
					ts, tns/1000000,
					100.0*(pmc2-pmc1)/(tsc2-tsc1),
					1.0*amount/(1.0*ts+1.0*tns/1000/1000/1000)/(1<<20)
			);
			residx++;
		} // next round
	} // next buffer size

	close(disk);

	for (residx=0; residx < STEPS*PASSES; residx++) {
		pass_t *p = &results[residx];
		long long amount;
		amount = 1ll * p->requests * p->blockcount * BLOCK_SIZE;
		printf(
				"%8u" "\t"
				"%4u.%03u" "\t"
				"%4u.%03u" "\t"
				"%5u" "\t"
				"%8.3f" "\t"
				"%8.3f" "\n",
				p->blockcount,
				p->ps, p->pns/1000000,
				p->ts, p->tns/1000000,
				p->requests,
				(100.0*p->ps+1.0*p->pns/1000/1000/10) /
					(1.0*p->ts+1.0*p->tns/1000/1000/1000)/(1<<20),
				1.0*amount/(1.0*p->ts+1.0*p->tns/1000/1000/1000)/(1<<20)
		);
	}

	return 0;
}
