#ifndef __KVM_L4_SVM_H
#define __KVM_L4_SVM_H

#include <asm/svm.h>
#include <linux/kvm_host.h>

#include <l4/sys/types.h>
#include <l4/sys/vm.h>
#include <l4/sys/factory.h>
#include <l4/re/consts.h>
#include <l4/log/log.h>
#include <asm/generic/user.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/debugger.h>

#include "kvm-l4.h"

#define L4X_VMCB_LOG2_SIZE 12

static inline void l4x_svm_vmcb_seg_dump(struct kvm_vcpu *vcpu, struct vmcb_seg *seg, char *name)
{
	kvm_err("%s: selector = %04x, attrib = %04x limit = %08x, attrib = %016llx\n",
	            name, seg->selector, seg->attrib, seg->limit, seg->base);
}

static inline void l4x_svm_vmcb_dump(struct kvm_vcpu *vcpu)
{
	struct vcpu_svm *svm = to_svm(vcpu);
	struct vmcb_control_area c = svm->vmcb->control;
	struct vmcb_save_area s = svm->vmcb->save;

	kvm_err("************************ vmcb_dump ************************\n");
	kvm_err("****** save ****\n");
	l4x_svm_vmcb_seg_dump(vcpu, &s.es, "es");
	l4x_svm_vmcb_seg_dump(vcpu, &s.cs, "cs");
	l4x_svm_vmcb_seg_dump(vcpu, &s.ss, "ss");
	l4x_svm_vmcb_seg_dump(vcpu, &s.ds, "ds");
	l4x_svm_vmcb_seg_dump(vcpu, &s.fs, "fs");
	l4x_svm_vmcb_seg_dump(vcpu, &s.gs, "gs");
	l4x_svm_vmcb_seg_dump(vcpu, &s.gdtr, "gdtr");
	l4x_svm_vmcb_seg_dump(vcpu, &s.ldtr, "ldtr");
	l4x_svm_vmcb_seg_dump(vcpu, &s.idtr, "idtr");
	l4x_svm_vmcb_seg_dump(vcpu, &s.idtr, "tr");
	kvm_err("cpl = %02x, efer = %016llx\n", s.cpl, s.efer);
	kvm_err("cr0 = %016llx cr2 = %016llx\n", s.cr0, s.cr2);
	kvm_err("cr3 = %016llx cr4 = %016llx\n", s.cr3, s.cr4);
	kvm_err("dr6 = %016llx dr7 = %016llx\n", s.dr6, s.dr7);
	kvm_err("rflags = %016llx, rip = %016llx\n", s.rflags, s.rip);
	kvm_err("rsp = %016llx rax = %016llx\n", s.rsp, s.rax);
	kvm_err("star = %016llx lstar = %016llx\n", s.star, s.lstar);
	kvm_err("cstar = %016llx\n", s.cstar);
	kvm_err("sfmask = %016llx kernel_gs_base = %016llx\n", s.sfmask, s.kernel_gs_base);
	kvm_err("syse_cs = %016llx syse_esp = %016llx\n", s.sysenter_cs, s.sysenter_esp);
	kvm_err("syse_eip = %016llx\n", s.sysenter_eip);
	kvm_err("g_pat = %016llx dbgctl = %016llx\n", s.g_pat, s.dbgctl);
	kvm_err("br_from = %016llx br_to = %016llx\n", s.br_from, s.br_to);
	kvm_err("last_ex_from = %016llx last_ex_to = %016llx\n", s.last_excp_from, s.last_excp_to);
	kvm_err("**** control ***\n");
	kvm_err("inter_cr = %08x\n", c.intercept_cr);
	kvm_err("inter_dr = %08x\n", c.intercept_dr);
	kvm_err("inter_ex = %08x\n", c.intercept_exceptions);
	kvm_err("inter = %016llx\n", c.intercept);
	kvm_err("iopm_base_pa = %016llx msrpm_base_pa = %016llx\n", c.iopm_base_pa, c.msrpm_base_pa);
	kvm_err("tsc_offset = %016llx\n", c.tsc_offset);
	kvm_err("asid = %08x tlb_ctl = %02x\n", c.asid, c.tlb_ctl);
	kvm_err("int_ctl = %08x int_v = %08x int_s = %08x\n", c.int_ctl, c.int_vector, c.int_state);
	kvm_err("exit = %08x exit_hi = %08x\n", c.exit_code, c.exit_code_hi);
	kvm_err("exit_i1 = %016llx exit_i2 = %016llx\n", c.exit_info_1, c.exit_info_2);
	kvm_err("exit_int_i = %08x exit_int_i_err = %08x\n", c.exit_int_info, c.exit_int_info_err);
	kvm_err("nested_ctl = %016llx\n", c.nested_ctl);
	kvm_err("inj = %08x inj_err= %08x\n", c.event_inj, c.event_inj_err);
	kvm_err("ncr3 = %016llx\n", c.nested_cr3);
	kvm_err("virt_ext = %016llx\n", c.virt_ext);
	kvm_err("*********************** end_of_dump ***********************\n");
}

#endif //__KVM_L4_H
