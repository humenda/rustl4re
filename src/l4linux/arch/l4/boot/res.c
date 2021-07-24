#undef __always_inline

#include <string.h>
#include <l4/sys/compiler.h>

asm(    ".global __l4_external_resolver\n"
	".type __l4_external_resolver, %function\n"
	"__l4_external_resolver: \n"
#ifdef ARCH_arm
	"	stmdb  sp!, {r0 - r12, lr} \n" // 56 bytes onto the stack
	"	ldr r0, [sp, #60] \n" // r0 is the jmptblentry
	"	ldr r1, [sp, #56] \n" // r1 is the funcname pointer
	"	bl __C__l4_external_resolver \n"
	"	str r0, [sp, #60] \n"
	"	ldmia sp!, {r0 - r12, lr}\n"
	"	add sp, sp, #4 \n"
	"	ldmia sp!, {pc} \n"
#elif defined(ARCH_arm64)
	// TODO: Just do the callee saved ones...
	"	sub	sp, sp, #16 * 16\n"
	"	stp	x0, x1, [sp, #16 * 0]\n"
	"	stp	x2, x3, [sp, #16 * 1]\n"
	"	stp	x4, x5, [sp, #16 * 2]\n"
	"	stp	x6, x7, [sp, #16 * 3]\n"
	"	stp	x8, x9, [sp, #16 * 4]\n"
	"	stp	x10, x11, [sp, #16 * 5]\n"
	"	stp	x12, x13, [sp, #16 * 6]\n"
	"	stp	x14, x15, [sp, #16 * 7]\n"
	"	stp	x16, x17, [sp, #16 * 8]\n"
	"	stp	x18, x19, [sp, #16 * 9]\n"
	"	stp	x20, x21, [sp, #16 * 10]\n"
	"	stp	x22, x23, [sp, #16 * 11]\n"
	"	stp	x24, x25, [sp, #16 * 12]\n"
	"	stp	x26, x27, [sp, #16 * 13]\n"
	"	stp	x28, x29, [sp, #16 * 14]\n"
	"	str	x30, [sp, #16 * 15]\n"
	"	ldr	x0, [sp, #16 * 16 + 0]\n" // x0 is the jmptblentry
	"	ldr	x1, [sp, #16 * 16 + 8]\n" // x1 is the funcname pointer
	"	bl	__C__l4_external_resolver\n"
	"	str	x0, [sp, #16 * 16 + 0]\n"
	"	ldp	x0, x1, [sp, #16 * 0]\n"
	"	ldp	x2, x3, [sp, #16 * 1]\n"
	"	ldp	x4, x5, [sp, #16 * 2]\n"
	"	ldp	x6, x7, [sp, #16 * 3]\n"
	"	ldp	x8, x9, [sp, #16 * 4]\n"
	"	ldp	x10, x11, [sp, #16 * 5]\n"
	"	ldp	x12, x13, [sp, #16 * 6]\n"
	"	ldp	x14, x15, [sp, #16 * 7]\n"
	"	ldp	x16, x17, [sp, #16 * 8]\n"
	"	ldp	x18, x19, [sp, #16 * 9]\n"
	"	ldp	x20, x21, [sp, #16 * 10]\n"
	"	ldp	x22, x23, [sp, #16 * 11]\n"
	"	ldp	x24, x25, [sp, #16 * 12]\n"
	"	ldp	x26, x27, [sp, #16 * 13]\n"
	"	ldp	x28, x29, [sp, #16 * 14]\n"
	"	ldr	x30, [sp, #16 * 15]\n"
	"	add	sp, sp, #16 * 16\n"
	"	ldr	x9, [sp] \n"
	"	add	sp, sp, #16             \n"
	"	br	x9\n" // jump to the actually called function
#elif defined(ARCH_x86)
	"	pusha\n"
	"	mov 0x24(%esp), %eax\n" // eax is the jmptblentry
	"	mov 0x20(%esp), %edx\n" // edx is the symtab_ptr
	"	mov     (%edx), %edx\n" // edx is the funcname pointer
	"	call __C__l4_external_resolver \n"
	"	mov %eax, 0x20(%esp) \n"
	"	popa\n"
	"	ret $4\n"
#else
	"	push    %rcx\n"
	"	push    %rdx\n"
	"	push    %rbx\n"
	"	push    %rax\n"
	"	push    %rbp\n"
	"	push    %rsi\n"
	"	push    %rdi\n"
	"	push    %r8\n"
	"	push    %r9\n"
	"	push    %r10\n"
	"	push    %r11\n"
	"	push    %r12\n"
	"	push    %r13\n"
	"	push    %r14\n"
	"	push    %r15\n"
	"	mov     128(%rsp), %rdi\n" // rdi (1st) is the jmptblentry
	"	mov     120(%rsp), %rsi\n" // rsi       is the symtab_ptr
	"	mov     (%rsi), %rsi\n"    // rsi (2nd) is the funcname pointer
	"	call    __C__l4_external_resolver \n"
	"	mov	%rax, 120(%rsp)\n"
	"	pop     %r15\n"
	"	pop     %r14\n"
	"	pop     %r13\n"
	"	pop     %r12\n"
	"	pop     %r11\n"
	"	pop     %r10\n"
	"	pop     %r9\n"
	"	pop     %r8\n"
	"	pop     %rdi\n"
	"	pop     %rsi\n"
	"	pop     %rbp\n"
	"	pop     %rax\n"
	"	pop     %rbx\n"
	"	pop     %rdx\n"
	"	pop     %rcx\n"
	"	ret     $8\n"
#endif
	".size __l4_external_resolver,.-__l4_external_resolver"
);


#define EF(func) \
	void func(void);
#include <func_list.h>

#undef EF
#define EF(func) \
	else if (!strcmp(L4_stringify(func), funcname)) \
             { p = func; }

void do_resolve_error(const char *funcname);

unsigned long
#ifdef ARCH_x86
__attribute__((regparm(3)))
#endif
__C__l4_external_resolver(unsigned long jmptblentry, char *funcname);

unsigned long
#ifdef ARCH_x86
__attribute__((regparm(3)))
#endif
__C__l4_external_resolver(unsigned long jmptblentry, char *funcname)
{
	void *p;

	if (0) {
	}
#include <func_list.h>
	else
		p = 0;

	if (!p)
		do_resolve_error(funcname);

	*(unsigned long *)jmptblentry = (unsigned long)p;
	return (unsigned long)p;
}
