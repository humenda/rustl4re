/*
 * vcpu.cc --
 * 
 * VCPU test for comparing different strategies of emulating
 * write accesses to shared memory.
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 * 				 Adam Lackorzynski <adam@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/thread>
#include <l4/sys/factory>
#include <l4/sys/scheduler>
#include <l4/sys/utcb.h>
#include <l4/sys/kdebug.h>
#include <l4/util/util.h>
#include <l4/util/rdtsc.h>
#include <l4/re/env>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/kumem_alloc>
#include <l4/sys/debugger.h>
#include <l4/vcpu/vcpu>

#include <l4/re/error_helper>

#include <l4/sys/task>
#include <l4/sys/irq>
#include <l4/sys/vcpu.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <iostream>

#include "log"
#include "exceptions"
#include "emulation"

Romain::Log::LogLevel Romain::Log::maxLog = Romain::Log::INFO;
l4_umword_t Romain::Log::logFlags         = Romain::Log::None;
bool Romain::Log::withtime                = true;

using L4Re::chksys;
using L4Re::chkcap;

static char thread_stack[8 << 10];
static char hdl_stack[8 << 10];
static L4::Cap<L4::Task> vcpu_task;
static L4vcpu::Vcpu *vcpu;

const l4_addr_t super_code_map_addr = 0x10000;
extern char     my_super_code[];
static unsigned long fs;
static unsigned long ds;

#define DATASIZE (1 << 19)
asm
(
  ".p2align 12                      \t\n"
  ".global my_super_code            \t\n"
  ".global my_super_code_excp       \t\n"
  ".global my_super_code_excp_after \t\n"
  "my_super_code:                   \t\n"
  "1: movl $0xDEADBEEF, (%eax)      \t\n" // write word 0xdeadbeef to *eax
  "   add $4, %eax                  \t\n" // eax += 4
  "   cmp %eax, %ebx                \t\n" // break on limit
  "   jnz 1b                        \t\n"
 // "   sub $4096, %eax               \t\n"
 // "   loop 1b                       \t\n"
  "   ud2a                          \t\n"
);


static char __attribute__((aligned(4096))) data_area[DATASIZE];


static void setup_user_state_arch(L4vcpu::Vcpu *v)
{
  asm volatile ("mov %%fs, %0" : "=r"(fs));
  asm volatile ("mov %%ds, %0" : "=r"(ds));
  v->r()->ss = ds;
}

static void handler_prolog()
{
  asm volatile ("mov %0, %%es \t\n"
                "mov %0, %%ds \t\n"
                "mov %1, %%fs \t\n"
                : : "r"(ds), "r"(fs));
}

static void pf_info(L4vcpu::Vcpu *v)
{
	std::cout << "PF: ";
	if (v->r()->err & 0x2)
		std::cout << "write ";
	else
		std::cout << "read  ";
	std::cout << "@ " << std::hex << v->r()->pfa;
	std::cout << std::endl;
}

/*
 * Option 1: Direct mapping to vCPU task
 */
#define VAR_DIRECT  0
static inline void map_direct(L4vcpu::Vcpu *v)
{
	std::cout << "Mapping data" << std::endl;
	vcpu_task->map(L4Re::This_task, l4_fpage((l4_addr_t)data_area,
											 L4_PAGESHIFT << 2, L4_FPAGE_RWX),
				   v->r()->pfa);
	v->saved_state()->add(L4_VCPU_F_PAGE_FAULTS);
}

/*
 * Option 2: Trap & Emulate
 */
#define VAR_EMULATE 0

struct MyTranslator : public Romain::AddressTranslator
{
	l4_addr_t translate(l4_addr_t a) const
	{
		DEBUG() << "TRANSLATE: " << std::hex << a;
		if ((a >= super_code_map_addr) &&
			(a < super_code_map_addr + L4_PAGESIZE)) {
			unsigned ip = a - super_code_map_addr;
			return (l4_addr_t)my_super_code + ip;
		} else if ((a >= (l4_addr_t)data_area) &&
				   (a < (l4_addr_t)data_area + DATASIZE)) {
			return a; // data mapped identically
		} else {
			enter_kdebug("strange address");
		}
		return ~0U;
	}
};


static inline void emulate_write(L4vcpu::Vcpu *)
{
	//unsigned ip = v->r()->ip - super_code_map_addr;
	//Romain::InstructionPrinter((l4_addr_t)my_super_code + ip, v->r()->ip);
	static MyTranslator t;
	Romain::WriteEmulator(vcpu, &t).emulate();
}

/*
 * Option 3: Single-step instruction locally
 */
#define VAR_STEP    1
char instruction_buffer[32];
static __attribute__((noinline))
void local_singlestep(L4vcpu::Vcpu *v)
{
	l4_addr_t ip = v->r()->ip;
	ip -= super_code_map_addr;
	ip += (l4_addr_t)my_super_code;

	ud_t ud;
	ud_init(&ud);
	ud_set_mode(&ud, 32);
	ud_set_pc(&ud, v->r()->ip);
	ud_set_input_buffer(&ud, (unsigned char*)ip, 32);
	int num = ud_disassemble(&ud);

	//memset(instruction_buffer, 0x90, 32);
	memcpy(instruction_buffer, v->r()->ip - super_code_map_addr + my_super_code, num);
	instruction_buffer[num] = 0xc3; // RET
	v->r()->ip += num;

#if 0
	for (unsigned i = 0; i < sizeof(instruction_buffer); ++i) {
		printf("%02x ", (unsigned char)instruction_buffer[i]);
	}
	printf("\n");
#endif

	asm volatile ("call *%1"
	              :
	              : "A" (v->r()->ax),
				    "r"(instruction_buffer));
}

static void handle_pf(L4vcpu::Vcpu *v)
{
	if (0) pf_info(v);

 	if (v->r()->err & 0x2) {
		_check(v->r()->pfa < (l4_addr_t)data_area, "outside data area?");
		_check(v->r()->pfa >= ((l4_addr_t)data_area + DATASIZE), "outside data area?");
#if VAR_DIRECT
		map_direct(v);
#elif VAR_EMULATE
		emulate_write(v);
#elif VAR_STEP
		local_singlestep(v);
#endif
	} else if (v->r()->pfa == super_code_map_addr) {
		std::cout << "Mapping code" << std::endl;
		vcpu_task->map(L4Re::This_task, l4_fpage((l4_addr_t)my_super_code,
												 L4_PAGESHIFT, L4_FPAGE_RWX),
					   super_code_map_addr);
		v->saved_state()->add(L4_VCPU_F_PAGE_FAULTS);
	}
}


static struct timeval tv_start;
static struct timeval tv_fault;

static void handler(void)
{
	handler_prolog();

#if 0
	l4_cpu_time_t t = l4_rdtsc();
	std::cout << std::dec << t << std::endl;
#endif

	vcpu->state()->clear(L4_VCPU_F_EXCEPTIONS | L4_VCPU_F_DEBUG_EXC);

	if (0) {
		std::cout << "-------------- VCPU Fault --------------" << std::endl;
		vcpu->print_state();
	}

	/*
	 * We only expect to see page faults here.
	 */
	if (vcpu->is_page_fault_entry()) {
		handle_pf(vcpu);
	} else {
		gettimeofday(&tv_fault, NULL);
		printf("[1] %03ld.%06ld\n", tv_start.tv_sec, tv_start.tv_usec);
		printf("[2] %03ld.%06ld\n", tv_fault.tv_sec, tv_fault.tv_usec);
		enter_kdebug("fault");
	}

#if 0
	t = l4_rdtsc();
	std::cout << std::dec << t << std::endl;
	std::cout << "Resuming @ " << std::hex << vcpu->r()->ip << std::endl;
#endif
	L4::Cap<L4::Thread> self;
	self->vcpu_resume_commit(self->vcpu_resume_start());
	while(1)
		;
}

static void vcpu_thread(void)
{
	printf("Hello vCPU\n");

	memset(hdl_stack, 0, sizeof(hdl_stack));

	setup_user_state_arch(vcpu);
	vcpu->saved_state()->set(L4_VCPU_F_USER_MODE
							 | L4_VCPU_F_EXCEPTIONS
							 | L4_VCPU_F_PAGE_FAULTS
							 | L4_VCPU_F_IRQ);

	/* 
	 * Protocol issue: EAX contains the address to which the remote
	 * vCPU should write. See ASM code above.
	 */
	memset(data_area, 0, sizeof(data_area));
	printf("data %p %d\n", data_area, sizeof(data_area));
	vcpu->r()->ax = (l4_umword_t)data_area;
	vcpu->r()->bx = (l4_umword_t)data_area + DATASIZE;
	vcpu->r()->ip = super_code_map_addr;
	vcpu->r()->sp = 0x40000; // actually doesn't matter, we're not using any
	                         // stack memory in our code

	L4::Cap<L4::Thread> self;

	gettimeofday(&tv_start, NULL);
	printf("IRET\n");

	vcpu->task(vcpu_task);
	self->vcpu_resume_commit(self->vcpu_resume_start());

	printf("IRET: failed!\n");
	while (1)
		;
}

static int run(void)
{
  L4::Cap<L4::Thread> vcpu_cap;

  printf("vCPU example\n");

  l4_debugger_set_object_name(l4re_env()->main_thread, "vcpu_tne");

  // new task
  vcpu_task = chkcap(L4Re::Util::cap_alloc.alloc<L4::Task>(),
                     "Task cap alloc");

  chksys(L4Re::Env::env()->factory()->create_task(vcpu_task,
                                                  l4_fpage_invalid()),
         "create task");
  l4_debugger_set_object_name(vcpu_task.cap(), "vcpu 'user' task");

  /* new thread/vCPU */
  vcpu_cap = chkcap(L4Re::Util::cap_alloc.alloc<L4::Thread>(),
                    "vCPU cap alloc");

  l4_touch_rw(thread_stack, sizeof(thread_stack));

  chksys(L4Re::Env::env()->factory()->create(vcpu_cap), "create thread");
  l4_debugger_set_object_name(vcpu_cap.cap(), "vcpu thread");

  // get memory for vCPU state
  l4_addr_t kumem;
  if (0)
    kumem = (l4_addr_t)l4re_env()->first_free_utcb;
  else
    {
      if (L4Re::Util::kumem_alloc(&kumem, 0))
        exit(1);
    }
  l4_utcb_t *vcpu_utcb = (l4_utcb_t *)kumem;
  vcpu = L4vcpu::Vcpu::cast(kumem + L4_UTCB_OFFSET);
  vcpu->entry_sp((l4_umword_t)hdl_stack + sizeof(hdl_stack));
  vcpu->entry_ip((l4_umword_t)handler);

  printf("VCPU: utcb = %p, vcpu = %p\n", vcpu_utcb, vcpu);

  // Create and start vCPU thread
  L4::Thread::Attr attr;
  attr.pager(L4::cap_reinterpret_cast<L4::Thread>(L4Re::Env::env()->rm()));
  attr.exc_handler(L4Re::Env::env()->main_thread());
  attr.bind(vcpu_utcb, L4Re::This_task);
  chksys(vcpu_cap->control(attr), "control");
  chksys(vcpu_cap->vcpu_control((l4_addr_t)vcpu), "enable VCPU");

  chksys(vcpu_cap->ex_regs((l4_umword_t)vcpu_thread,
                           (l4_umword_t)thread_stack + sizeof(thread_stack),
                           0));

  chksys(L4Re::Env::env()->scheduler()->run_thread(vcpu_cap,
                                                   l4_sched_param(2)));

  l4_sleep_forever();
  return 0;
}

int main()
{
  try { return run(); }
  catch (L4::Runtime_error &e)
    {
      std::cerr << "FATAL uncought exception: " << e.str()
               << "\nterminating...\n";
    }
  catch (...)
    {
      std::cerr << "FATAL uncought exception of unknown type\n"
               << "terminating...\n";
    }
  return 1;
}
