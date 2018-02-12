#ifndef __KVM_L4_H
#define __KVM_L4_H

#include <linux/kvm_host.h>

#include <l4/sys/types.h>

#define L4X_VMCB_LOG2_SIZE 12

int l4x_kvm_create_vm(struct kvm *kvm);
int l4x_kvm_destroy_vm(struct kvm *kvm);

void l4x_kvm_svm_run(struct kvm_vcpu *kvcpu, unsigned long vmcb);
void l4x_kvm_vmx_run(struct kvm_vcpu *kvcpu);

static inline int l4x_kvm_dbg(void)
{
	return 0;
}

#endif
