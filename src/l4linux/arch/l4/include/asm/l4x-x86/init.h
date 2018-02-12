#ifndef __ASM_L4__L4X_X86__INIT_H__
#define __ASM_L4__L4X_X86__INIT_H__

char *l4x_x86_memory_setup(void);

static unsigned int l4x_io_apic_read(unsigned int apic, unsigned int reg)
{
        return ~0;
}

static void l4x_restore_boot_irq_mode(void)
{}

#endif /* __ASM_L4__L4X_X86__INIT_H__ */
