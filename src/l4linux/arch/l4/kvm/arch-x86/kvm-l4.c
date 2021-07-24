

#include <linux/module.h>
#include <linux/kvm_host.h>

#include <asm/l4lxapi/task.h>
#include <asm/generic/vcpu.h>
#include <asm/generic/jdb.h>

#include <l4/sys/factory.h>
#include <l4/sys/debugger.h>
#include <l4/sys/vm.h>
#include <l4/re/env.h>
#include <l4/vcpu/vcpu.h>

#include "kvm-l4.h"

int l4x_kvm_create_vm(struct kvm *kvm)
{
	l4_utcb_t *u = l4_utcb();
	int r;

	kvm->arch.l4vmcap = L4_INVALID_CAP;

	if (l4lx_task_get_new_task(L4_INVALID_CAP, &kvm->arch.l4vmcap)) {
		pr_err("l4x-kvm: could not allocate task cap\n");
		return -ENOENT;
	}

	r = L4XV_FN_e(l4_factory_create_vm_u(l4re_env()->factory,
	                                     kvm->arch.l4vmcap, u));
	if (unlikely(r)) {
		pr_err("l4x-kvm: kvm task creation failed cap=%08lx: %d\n",
		       kvm->arch.l4vmcap, r);
		l4lx_task_number_free(kvm->arch.l4vmcap);
		return -ENOENT;
	}

	L4XV_FN_v(l4x_dbg_set_object_name(kvm->arch.l4vmcap, "kvmVM"));

	return 0;
}
EXPORT_SYMBOL(l4x_kvm_create_vm);

static inline void l4x_kvm_l4vcpu_to_kvm(l4_vcpu_state_t *l4vcpu,
                                         struct kvm_vcpu *kvcpu)
{
	l4_vcpu_regs_t *r = &l4vcpu->r;
	kvcpu->arch.regs[VCPU_REGS_RAX] = r->ax;
	kvcpu->arch.regs[VCPU_REGS_RDX] = r->dx;
	kvcpu->arch.regs[VCPU_REGS_RCX] = r->cx;
	kvcpu->arch.regs[VCPU_REGS_RBX] = r->bx;
	kvcpu->arch.regs[VCPU_REGS_RBP] = r->bp;
	kvcpu->arch.regs[VCPU_REGS_RSI] = r->si;
	kvcpu->arch.regs[VCPU_REGS_RDI] = r->di;
#ifdef CONFIG_X86_64
	kvcpu->arch.regs[VCPU_REGS_R8]  = r->r8;
	kvcpu->arch.regs[VCPU_REGS_R9]  = r->r9;
	kvcpu->arch.regs[VCPU_REGS_R10] = r->r10;
	kvcpu->arch.regs[VCPU_REGS_R11] = r->r11;
	kvcpu->arch.regs[VCPU_REGS_R12] = r->r12;
	kvcpu->arch.regs[VCPU_REGS_R13] = r->r13;
	kvcpu->arch.regs[VCPU_REGS_R14] = r->r14;
	kvcpu->arch.regs[VCPU_REGS_R15] = r->r15;
#endif
#if 0
	current->thread.debugreg0 = r->dr0;
	current->thread.debugreg1 = r->dr1;
	current->thread.debugreg2 = r->dr2;
	current->thread.debugreg3 = r->dr3;
#endif
}

static inline void l4x_kvm_kvm_to_l4vcpu(struct kvm_vcpu *kvcpu,
                                         l4_vcpu_state_t *l4vcpu)
{
	l4_vcpu_regs_t *r = &l4vcpu->r;
	r->ax = kvcpu->arch.regs[VCPU_REGS_RAX];
	r->dx = kvcpu->arch.regs[VCPU_REGS_RDX];
	r->cx = kvcpu->arch.regs[VCPU_REGS_RCX];
	r->bx = kvcpu->arch.regs[VCPU_REGS_RBX];
	r->bp = kvcpu->arch.regs[VCPU_REGS_RBP];
	r->si = kvcpu->arch.regs[VCPU_REGS_RSI];
	r->di = kvcpu->arch.regs[VCPU_REGS_RDI];
#ifdef CONFIG_X86_64
	r->r8 = kvcpu->arch.regs[VCPU_REGS_R8];
	r->r9 = kvcpu->arch.regs[VCPU_REGS_R9];
	r->r10= kvcpu->arch.regs[VCPU_REGS_R10];
	r->r11= kvcpu->arch.regs[VCPU_REGS_R11];
	r->r12= kvcpu->arch.regs[VCPU_REGS_R12];
	r->r13= kvcpu->arch.regs[VCPU_REGS_R13];
	r->r14= kvcpu->arch.regs[VCPU_REGS_R14];
	r->r15= kvcpu->arch.regs[VCPU_REGS_R15];
#endif
#if 0
	r->dr0 = current->thread.debugreg0;
	r->dr1 = current->thread.debugreg1;
	r->dr2 = current->thread.debugreg2;
	r->dr3 = current->thread.debugreg3;
#endif
}

#ifdef CONFIG_KVM_AMD
void l4x_kvm_svm_run(struct kvm_vcpu *kvcpu, unsigned long vmcb)
{
	l4_msgtag_t tag;
	unsigned cpu;
	unsigned long orig_state, orig_saved_state;
	l4_vcpu_state_t *vcpu;
	L4XV_V(f);

	L4XV_L(f);
	cpu = smp_processor_id();
	vcpu = l4x_vcpu_ptr[cpu];

	orig_state        = vcpu->state;
	vcpu->state       = L4_VCPU_F_FPU_ENABLED;

	orig_saved_state  = vcpu->saved_state;
	vcpu->saved_state = L4_VCPU_F_USER_MODE | L4_VCPU_F_FPU_ENABLED;

	vcpu->user_task   = kvcpu->kvm->arch.l4vmcap;

	l4x_kvm_kvm_to_l4vcpu(kvcpu, vcpu);
	memcpy((char *)vcpu + L4_VCPU_OFFSET_EXT_STATE, (void *)vmcb,
	       L4_PAGESIZE - L4_VCPU_OFFSET_EXT_STATE);

	tag = l4_thread_vcpu_resume_start();
	tag = l4_thread_vcpu_resume_commit(L4_INVALID_CAP, tag);

	l4x_kvm_l4vcpu_to_kvm(vcpu, kvcpu);
	memcpy((void *)vmcb, (char *)vcpu + L4_VCPU_OFFSET_EXT_STATE,
	       L4_PAGESIZE - L4_VCPU_OFFSET_EXT_STATE);

	vcpu->user_task   = L4_INVALID_CAP;
	vcpu->state       = orig_state;
	vcpu->saved_state = orig_saved_state;
	L4XV_U(f);

	if (l4_error(tag) < 0)
		pr_err("l4x-kvm: VM run failed with %ld\n", l4_error(tag));
}
EXPORT_SYMBOL(l4x_kvm_svm_run);
#endif

#ifdef CONFIG_KVM_INTEL
void l4x_kvm_vmx_run(struct kvm_vcpu *kvcpu)
{
	l4_msgtag_t tag;
	unsigned cpu;
	unsigned long orig_state, orig_saved_state;
	l4_vcpu_state_t *vcpu;
	void *vmcs;
	L4XV_V(f);

	L4XV_L(f);
	cpu = smp_processor_id();
	vcpu = l4x_vcpu_ptr[cpu];
	vmcs = ((void*)vcpu) + L4_VCPU_OFFSET_EXT_STATE;

	orig_state        = vcpu->state;
	vcpu->state       = L4_VCPU_F_FPU_ENABLED;

	orig_saved_state  = vcpu->saved_state;
	vcpu->saved_state = L4_VCPU_F_USER_MODE | L4_VCPU_F_FPU_ENABLED;

	vcpu->user_task   = kvcpu->kvm->arch.l4vmcap;

	l4x_kvm_kvm_to_l4vcpu(kvcpu, vcpu);
	l4_vm_vmx_write_nat(vmcs, L4_VM_VMX_VMCS_CR2, kvcpu->arch.cr2);

	tag = l4_thread_vcpu_resume_start();
	tag = l4_thread_vcpu_resume_commit(L4_INVALID_CAP, tag);

	l4x_kvm_l4vcpu_to_kvm(vcpu, kvcpu);
	kvcpu->arch.cr2   = l4_vm_vmx_read_nat(vmcs, L4_VM_VMX_VMCS_CR2);

	vcpu->user_task   = current->mm->context.task;
	vcpu->state       = orig_state;
	vcpu->saved_state = orig_saved_state;
	L4XV_U(f);

	if (l4_error(tag) < 0)
		pr_err("l4x-kvm: VM run failed with %ld\n", l4_error(tag));
}
EXPORT_SYMBOL(l4x_kvm_vmx_run);
#endif

int l4x_kvm_destroy_vm(struct kvm *kvm)
{
	if (l4lx_task_delete_task(kvm->arch.l4vmcap)) {
		pr_err("l4x-kvm: kvm task destruction failed cap=%08lx\n",
		       kvm->arch.l4vmcap);
		l4lx_task_number_free(kvm->arch.l4vmcap);
		return -ENOENT;
	}

	l4lx_task_number_free(kvm->arch.l4vmcap);
	return 0;
}
EXPORT_SYMBOL(l4x_kvm_destroy_vm);
