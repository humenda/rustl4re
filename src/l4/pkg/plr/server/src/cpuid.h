#pragma once

#include <l4/sys/types.h>
#include "constants.h"

struct CPU_id
{
    l4_umword_t fiasco_id;
    l4_umword_t apic_id;

    CPU_id(l4_umword_t a=0xf00, l4_umword_t b=0xf00)
        : fiasco_id(a), apic_id(b)
    {}

    static bool comp_apic(CPU_id const& i1, CPU_id const& i2)
    { return i1.apic_id < i2.apic_id; }
};

class CPUID
{
public:

    enum {
        MAXID     = 0x00,
        FMS_flags = 0x01,
        CACHES_o  = 0x02,
        PSN       = 0x03,
        CACHES_n  = 0x04,
        MON       = 0x05,
        POWER     = 0x06,
        FLAGS     = 0x07,
        DCA       = 0x08,
        PEMO      = 0x0A,
        TOPOLOGY  = 0x0B,
        XSTATE    = 0x0D,
    };

    /* Perform CPUID */
    static l4_umword_t cpuid(l4_umword_t code, l4_umword_t* a, l4_umword_t* b,
                          l4_umword_t *c, l4_umword_t *d)
    {
        asm volatile ("cpuid"
         : "=a"(*a), "=b"(*b), "=c"(*c), "=d"(*d)
         : "a" (code), "b" (*b), "c" (*c), "d" (*d));
        return *a;
    }


    /*
     * Check if CPU supports CPUID.
     *
     * From http://wiki.osdev.org/CPUID
     */
     static bool have_cpuid()
     {
        l4_umword_t ret;
        asm volatile (
            "pushf\t\n"
            "pop %%eax\t\n"
            "mov %%eax, %%ecx\t\n"
            "xor $0x200000, %%eax\t\n"
            "push %%eax\t\n"
            "popf\t\n"
            "pushf\t\n"
            "pop %%eax\t\n"
            "xor %%ecx, %%eax\t\n"
            "shr $21, %%eax\t\n"
            "and $1, %%eax\t\n"
            "push %%ecx\t\n"
            "popf\t\n"
            : "=a" (ret));

        return (ret == 1);
    }


    static CPU_id current_apicid(l4_umword_t fiascoID)
    {
        CPU_id ret;
        l4_umword_t a,b,c,d;
        a = 0; b = 0; c = 0; d = 0;
        a = CPUID::cpuid(CPUID::TOPOLOGY, &a, &b, &c, &d);
        ret.fiasco_id = fiascoID;
        ret.apic_id   = d;
        return ret;
    }


    static l4_umword_t num_cpus()
    {
        if (CPUID::max_cpuid() < CPUID::TOPOLOGY) {
            return Romain::MAX_CPUS;
        }
        l4_umword_t a,b,c,d;
        a = 0; b = 0; c = 1; d = 0;
        a = CPUID::cpuid(CPUID::TOPOLOGY, &a, &b, &c, &d);
        return b;
    }



    static l4_umword_t max_cpuid()
    {
        l4_umword_t a, b, c, d;
        a = b = c = d = 0;
        return CPUID::cpuid(CPUID::MAXID, &a, &b, &c, &d);
    }
};
