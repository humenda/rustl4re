#include <l4/sys/types.h>
#include <stdio.h>

static void print_cache2(unsigned level, unsigned v)
{
  if (!((v >> 4) & 0xf))
    {
      printf("  MIPS: no L%d-Cache implemented\n", level);
      return;
    }

  printf("  MIPS: L%d-Cache: %d-way %d-set, %d byte per line (u=%x)\n",
         level,
         (v & 0xf) + 1, 64U << ((v >> 8U) & 0xf), 2U << ((v >> 4) & 0xf),
         (v >> 12) & 0xf);
}

static l4_umword_t mips_guest_ctl0()
{
  l4_umword_t v;
  asm volatile ("mfc0 %0, $12, 6" : "=r"(v));
  return v;
}

static void mips_guest_ctl0(l4_umword_t v)
{
  asm volatile ("mtc0 %0, $12, 6" : : "r"(v));
}

static l4_umword_t mips_test_guest_ctl0(l4_umword_t bits)
{
  l4_umword_t x = mips_guest_ctl0();
  mips_guest_ctl0(x | bits);
  l4_umword_t r = mips_guest_ctl0() & bits;
  mips_guest_ctl0(x);
  return r;
}

static l4_umword_t mips_test_guest_ctl1()
{
  l4_umword_t v, r;
  asm volatile ("mfc0 %0, $10, 4" : "=r"(v));
  asm volatile ("mtc0 %0, $10, 4" : : "r"(~0UL));
  asm volatile ("mfc0 %0, $10, 4" : "=r"(r));
  asm volatile ("mtc0 %0, $10, 4" : : "r"(v));
  return r;
}

static void print_vz_info()
{
  l4_umword_t v = mips_guest_ctl0();
  if (v & (1UL << 18))
    printf("  MIPS: VZ: Pending Interrupt Passthrough\n");

  if (mips_test_guest_ctl0(1UL << 24)) // CG
    printf("  MIPS: VZ: Cache Instruction Guest-mode\n");

  char const *const ats[] = {
    "AT=0 <reserved>", "Guest MMU under Root control",
    "AT=2 <reserved>", "Guest MMU under Guest control"
  };

  printf("  MIPS: VZ: %s\n", ats[(v >> 26) & 3]);

  if (v & (1UL << 9))  // RAD
    printf("  MIPS: VZ: RAD (Root ASID Dealias)\n");

  if (mips_test_guest_ctl0(1UL << 8))  // DRG
    printf("  MIPS: VZ: Direct Root to Guest\n");

  if (v & (1UL << 22)) // G1
    {
      printf("  MIPS: VZ: GuestID support\n");
      l4_umword_t g1 = mips_test_guest_ctl1();
      printf("  MIPS: VZ: GuestCtl1: %lx\n", g1);
      printf("  MIPS: VZ: EID=0x%lx\n", (g1 >> 24) & 0xff);
      printf("  MIPS: VZ: GuestIDs=%lu\n", (g1 & 0xff) + 1);
      printf("  MIPS: VZ: GuestRIDs=%lu\n", ((g1 >> 16) & 0xff) + 1);
    }

  if (v & (1UL << 7)) // G2
    printf("  MIPS: VZ: GuestCtl2 supported\n");

  if (v & (1UL << 19)) // G2
    {
      l4_umword_t t;
      asm volatile ("mfc0 %0, $11, 4" : "=r"(t));
      printf("  MIPS: VZ: GuestCtl0Ext supported: val=%lx\n", t);
      char const *const nccs[] = {
        "Guest CCA independent of root CCA",
        "Guest CCA modified by root CCA",
        "2 <reserved>", "3 <reserved>"
      };
      printf("  MIPS: VZ: Nested Cache Coherency: %s\n", nccs[(t >> 6) & 3]);
    }
}


void print_cpu_info();
void print_cpu_info()
{
  l4_umword_t v;
  asm volatile ("mfc0 %0, $15" : "=r"(v));
  printf("  MIPS: processor ID: %08lx\n", v);
  asm volatile ("mfc0 %0, $16" : "=r"(v));
  char const *const ars[] = { "r1", "r2+", "r6", "AR<4>", "AR<5>", "AR<6>", "AR<7>" };
  char const *const ats[] = { "MIPS32", "MIPS64 (32bit compat)", "MIPS64", "AT<3>" };
  printf("  MIPS: Config:  %08lx %s %s %s %sMMU-Type=%lx IMPL=%lx K0=%lx KU=%lx K23=%lx\n",
         v,
         ats[(v >> 13) & 3], ars[(v >> 10) & 7],
         (v & (1UL << 15)) ? "BE" : "LE",
         (v & 8) ? "VI " : "", (v >> 7) & 7,
         (v >> 16) & 0x1ff,
         (v & 0x7), (v >> 25) & 7, (v >> 28) & 7);
  if (!(v & (1UL << 31)))
    return;

  asm volatile ("mfc0 %0, $16, 1" : "=r"(v));
  printf("  MIPS: Config1: %08lx %s%s%s%s%s%s%s\n", v,
         (v & 1) ? "FPU " : "",     (v & 2) ? "EJTAG " : "",
         (v & 4) ? "MIPS16e " : "", (v & 8) ? "WATCH " : "",
         (v & 0x10) ? "PERF " : "", (v & 0x20) ? "MDMX " : "",
         (v & 0x40) ? "COP2 " : "");
  printf("  MIPS: D-cache: %lu-way %d-set %d byte per line\n",
         ((v >> 7) & 7) + 1, 32 * (1U << (((v >> 13) + 1) & 7)),
         1U << (((v >> 10) & 7) + 1));
  printf("  MIPS: I-cache: %lu-way %d-set %d byte per line\n",
         ((v >> 16) & 7) + 1, 32 * (1U << (((v >> 22) + 1) & 7)),
         1U << (((v >> 19) & 7) + 1));
  printf("  MIPS: TLB: %lu entries\n", ((v >> 25) & 0x3f) + 1);
  if (!(v & (1UL << 31)))
    return;

  asm volatile ("mfc0 %0, $16, 2" : "=r"(v));
  printf("  MIPS: Config2: %08lx\n", v);
  print_cache2(2, v);
  print_cache2(3, v >> 16);
  if (!(v & (1UL << 31)))
    return;

  asm volatile ("mfc0 %0, $16, 3" : "=r"(v));
  printf("  MIPS: Config3: %08lx\n", v);
  char const *const features3[] = {
    "TraceLogic", "SmartMIPS", "MT (Multi Threading)",
    "CDMM (Common Device Memory Map)", "SP (1KiB Page)",
    "VInt", "VEIC", "LPA", "ITL (MIPS IFlowtrace)",
    "CTXTC (ContextConfig)", "DSPP (MIPS DSP)", "DSP2P (MIPS DSP r2)",
    "RXI", "ULRI", "microMIPS", "boot microMIPS", "exc microMIPS",
    "MCU"
  };
  char const *const features3_2[] = {
    "VZ", "PW (HW Page Walk)", "SC (Segment Control)", "BadInstr",
    "BadInstrP", "MASP (SIMD)", "CMGCR (Coherency Manager MM)",
    "BPG (Big Pages)",
  };

  printf("  MIPS:");
  for (unsigned i = 0; i < 18; ++i)
    if (v & (1UL << i))
      printf(" %s", features3[i]);

  for (unsigned i = 23; i < 31; ++i)
    if (v & (1UL << i))
      printf(" %s", features3_2[i - 23]);
  printf("\n");

  if (v & (1UL << 23))
    print_vz_info();

  if (!(v & (1UL << 31)))
    return;

  asm volatile ("mfc0 %0, $16, 4" : "=r"(v));
  printf("  MIPS: Config4: %08lx\n", v);
  if (!(v & (1UL << 31)))
    return;

  asm volatile ("mfc0 %0, $16, 5" : "=r"(v));
  printf("  MIPS: Config5: %08lx\n", v);
  if (!(v & (1UL << 31)))
    return;

}

