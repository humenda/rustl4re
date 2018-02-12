/*
 * MLDE32 -- a tiny instruction length decoder for x86/32
 *
 *      http://www.woodmann.com/collaborative/tools/index.php/Mlde32
 */
.intel_syntax noprefix

.data
O_UNIQUE   =     0
O_PREFIX   =     1
O_IMM8     =     2
O_IMM16    =     3
O_IMM24    =     4
O_IMM32    =     5
O_IMM48    =     6
O_MODRM    =     7
O_MODRM8   =     8
O_MODRM32  =     9
O_EXTENDED =    10
O_WEIRD    =    11
O_ERROR    =    12

.text
.global mlde32
mlde32:
	pushad
	cld

	xor edx,edx
	mov esi, [esp+(8*4)+4]
	mov ebp,esp

	// 256 bytes, index-compressed opcode type table
	push 0x01097f71c
	push 0x0F71C6780
	push 0x017389718
	push 0x0101CB718
	push 0x017302C17
	push 0x018173017
	push 0x0F715F547
	push 0x04C103748
	push 0x0272CE7F7
	push 0x0F7AC6087
	push 0x01C121C52
	push 0x07C10871C
	push 0x0201C701C
	push 0x04767602B
	push 0x020211011
	push 0x040121625
	push 0x082872022
	push 0x047201220
	push 0x013101419
	push 0x018271013
	push 0x028858260
	push 0x015124045
	push 0x05016A0C7
	push 0x028191812
	push 0x0F2401812
	push 0x019154127
	push 0x050F0F011
	mov  ecx, 0x15124710
	push ecx
	push 0x11151247
	push 0x10111512
	push 0x47101115
	mov  eax, 0x12472015
	push eax
	push eax
	push 0x12471A10
	add  cl, 0x10
	push ecx
	sub  cl, 0x20
	push ecx

	xor  ecx, ecx
	dec  ecx

	// code starts
.Lps:
	inc ecx
	mov edi, esp
.Lgo:
	lodsb
	mov bh, al
.Lft:
    mov ah, [edi]
	inc edi
	shr  ah, 4
	sub  al, ah
	jnc  .Lft

	mov  al, [edi-1]
	and  al, 0xf

	cmp  al, O_ERROR
	jnz  .Li7

	pop  edx
	not  edx

.Li7:
	inc edx
	cmp  al, O_UNIQUE
	jz   .Lt_exit

	cmp  al, O_PREFIX
	jz   .Lps

	add  edi, 0x51          /* (.L_ettbl - .L_ttbl) */

	cmp  al, O_EXTENDED
	jz   .Lgo


	mov  edi, [ebp+(8*4)+4]

.Li6:  inc  edx
	cmp  al, O_IMM8
	jz   .Lt_exit
	cmp  al, O_MODRM
	jz   .Lt_modrm
	cmp  al, O_WEIRD
	jz   .Lt_weird

.Li5:  inc  edx
	cmp  al, O_IMM16
	jz   .Lt_exit
	cmp  al, O_MODRM8
	jz   .Lt_modrm

.Li4:  inc  edx
	cmp  al, O_IMM24
	jz   .Lt_exit

.Li3:  inc  edx
.Li2:  inc  edx

	pushad
	mov  al, 0x66
	repnz scasb
	popad
	jnz  .Lc32

.Ld2:  dec  edx
	dec  edx
.Lc32: cmp  al, O_MODRM32
	jz   .Lt_modrm
	sub  al, O_IMM32
	jz   .Lt_imm32

.Li1:  inc  edx

.Lt_exit:
	mov  esp, ebp
	mov  [esp+(7*4)], edx
	popad
	ret

/*********************************
 * PROCESS THE MOD/RM BYTE       *
 *                               *
 *   7    6 5          3 2    0  *
 *   | MOD | Reg/Opcode | R/M |  *
 *                               *
 *********************************
*/
.Lt_modrm:
       lodsb
       mov  ah, al
       shr  al, 7
       jb   .Lprmk
       jz   .Lprm

       add  dl, 4

       pushad
       mov  al, 0x67
       repnz scasb
       popad
       jnz  .Lprm

.Ld3:  sub  dl, 3

       dec  al
.Lprmk:jnz  .Lt_exit
       inc  edx
       inc  eax
.Lprm:
       and  ah, 0x7

       pushad
       mov  al, 0x67
       repnz scasb
       popad
       jz   .Lprm67chk

       cmp  ah, 4
       jz   .Lprmsib

       cmp  ah, 5
       jnz  .Lt_exit

.Lprm5chk:
       dec  al
       jz   .Lt_exit
.Li42: add  dl, 4
       jmp  .Lt_exit

.Lprm67chk:
       cmp  ax, 0x0600
       jnz  .Lt_exit
       inc  edx
       jmp  .Li1

.Lprmsib:
       cmp  al, 0
       jnz  .Li1
       lodsb
       and  al, 0x7
       sub  al, 5
       jnz  .Li1
       inc  edx
       jmp  .Li42

/****************************
 * PROCESS WEIRD OPCODES    *
 *                          *
 * Fucking test (F6h/F7h)   *
 *                          *
 ****************************/
.Lt_weird:
       test byte ptr [esi], 0x38
       jnz  .Lt_modrm

       mov  al, O_MODRM8

       shr  bh, 1
       adc  al, 0
       jmp  .Li5


/*********************************
 * PROCESS SOME OTHER SHIT       *
 *                               *
 * Fucking mov (A0h/A1h/A2h/A3h) *
 *                               *
 *********************************/
.Lt_imm32:
       sub  bh, 0xA0

       cmp  bh, 4
       jae  .Ld2

       pushad
       mov  al, 0x67
       repnz scasb
       popad
       jnz  .Lchk66t

.Ld4:  dec  edx
       dec  edx

.Lchk66t:
       pushad
       mov  al, 0x66
       repnz scasb
       popad
       jz   .Li1
       jnz  .Ld2
