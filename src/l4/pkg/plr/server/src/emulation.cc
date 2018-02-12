/*
 * emulation.cc --
 *
 *     Implementation of the write instruction emulator.
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "log"
#include "exceptions"
#include "memory"
#include "emulation"

#include <string.h>
#include <l4/sys/kdebug.h>

#define MSG() DEBUGf(Romain::Log::Emulator)

/*
 * Debugging: get human-readable operand type
 */
static char const *operand_type_string(ud_operand_t *op)
{
	switch (op->type) {
		case UD_OP_REG: return "register";
		case UD_OP_MEM: return "memory";
		case UD_OP_PTR: return "pointer";
		case UD_OP_IMM: return "immediate";
		case UD_OP_JIMM: return "immediate jmp target";
		case UD_OP_CONST: return "constant";
		default: return "invalid";
	}
}


void Romain::Emulator_base::init_ud()
{
	ud_init(&_ud);
	ud_set_mode(&_ud, 32);
	ud_set_syntax(&_ud, UD_SYN_INTEL);

	ud_set_pc(&_ud, ip());
	ud_set_input_buffer(&_ud, (l4_uint8_t*)_local_ip, 32);

	l4_mword_t num_bytes = ud_disassemble(&_ud);
	(void)num_bytes;
#if 0
	MSG() << "print_instruction "
	                          << num_bytes << " byte"
	                          << (num_bytes > 1 ? "s" : "") << ".";
#endif
}

/*
 * Romain::Emulator constructor
 *
 * Nothing fancy -- use of udis86 should be hidden behind another
 * property. XXX
 */
Romain::Emulator_base::Emulator_base(L4vcpu::Vcpu *vcpu,
                                     Romain::AddressTranslator const *trans)
	: _vcpu(vcpu), _translator(trans)
{
	_local_ip = _translator->translate(ip());
	//init_ud();
}


/*
 * Get register value from VCPU
 *
 * Returns an MWord even if the real operand is only 16 or 8 bit.
 */
l4_umword_t Romain::Emulator_base::register_to_value(ud_type op)
{
	l4_umword_t  val = ~0;

#define REG(udis_name, vcpu_name, target, ...) \
	case UD_R_##udis_name: target = _vcpu->r()->vcpu_name __VA_ARGS__; break

	switch(op) {
		REG(AL, ax, val, & 0xFF); REG(CL, cx, val, & 0xFF); REG(DL, dx, val, & 0xFF);
		REG(BL, bx, val, & 0xFF); REG(SPL, sp, val, & 0xFF); REG(BPL, bp, val, & 0xFF);
		REG(SIL, si, val, & 0xFF); REG(DIL, di, val, & 0xFF);
		
#if 0
		REG(AH, ax, val, & 0xFF00); REG(CH, cx, val, & 0xFF00); REG(DH, dx, val, & 0xFF00);
		REG(BH, bx, val, & 0xFF00);
#endif

		REG(AX, ax, val, & 0xFFFF); REG(CX, cx, val, & 0xFFFF); REG(DX, dx, val, & 0xFFFF);
		REG(BX, bx, val, & 0xFFFF); REG(SP, sp, val, & 0xFFFF); REG(BP, bp, val, & 0xFFFF);
		REG(SI, si, val, & 0xFFFF); REG(DI, di, val, & 0xFFFF);

		REG(EAX, ax, val); REG(ECX, cx, val); REG(EDX, dx, val);
		REG(EBX, bx, val); REG(ESP, sp, val); REG(EBP, bp, val);
		REG(ESI, si, val); REG(EDI, di, val);

		default: 
			MSG() << "target register: " << std::hex << op;
			enter_kdebug("unhandled register target");
			break;
	}
#undef REG
	return val;
}


void Romain::Emulator_base::value_to_register(l4_umword_t val, ud_type op)
{
#define REG(udis_name, vcpu_name, ...) \
	case UD_R_##udis_name: _vcpu->r()->vcpu_name = val __VA_ARGS__; break;

	switch(op) {
		REG(AL, ax, & 0xFF); REG(CL, cx, & 0xFF); REG(DL, dx, & 0xFF);
		REG(BL, bx, & 0xFF); REG(SPL, sp, & 0xFF); REG(BPL, bp, & 0xFF);
		REG(SIL, si, & 0xFF); REG(DIL, di, & 0xFF);

		REG(AX, ax, & 0xFFFF); REG(CX, cx, & 0xFFFF); REG(DX, dx, & 0xFFFF);
		REG(BX, bx, & 0xFFFF); REG(SP, sp, & 0xFFFF); REG(BP, bp, & 0xFFFF);
		REG(SI, si, & 0xFFFF); REG(DI, di, & 0xFFFF);

		REG(EAX, ax); REG(ECX, cx); REG(EDX, dx);
		REG(EBX, bx); REG(ESP, sp); REG(EBP, bp);
		REG(ESI, si); REG(EDI, di);
		
		default:
			MSG() << "target register: " << std::hex << op;
			enter_kdebug("unhandled register target");
			break;
	}

#undef REG
}


/*
 * Calculate the value for an operand
 *
 * Note, this always returns an MWord. Users need to check op->size
 * to determine what to do with the value.
 */
l4_umword_t Romain::Emulator_base::operand_to_value(ud_operand_t *op)
{
	bool valid = false;
	l4_umword_t val = ~0;

	switch(op->type) {
		// Operand is a register. The specific register is contained in
		// base in the form of an enumerated constant, enum ud_type.
		case UD_OP_REG:
			{
				char buf[80];
				snprintf(buf, 80, "reg b %02x idx %02x scale %02x offs %02x",
				         op->base, op->index, op->scale, op->offset);
				MSG() << buf;

				// addr: = base + index * scale + offset

				_check(op->scale != 0, "!! implement register scaling");
				_check(op->offset != 0, "!! implement register offset");

				l4_umword_t idx = op->index ? register_to_value(op->index) : 0;
				l4_umword_t bas = op->base ? register_to_value(op->base) : 0;

				val = bas + idx * op->scale;
				if (op->offset)
					val += op->lval.sdword;

				MSG() << "val = " << std::hex << val;

				valid = true;
			}
			break;

		// Immediate operand. Value available in lval.
		case UD_OP_IMM:
			{
				MSG() << "op sz " << (int)op->size
				        << "op val " << std::hex << op->lval.uqword;
				val = op->lval.udword;

				switch (op->size) {
					case 8: val &= 0xFF; break;
					case 16: val &= 0xFFFF; break;
					default: MSG() << "strange op size: " << op->size;
					case 32: break;
				}

				valid = true;
			}
			break;

		// Memory operand. The intermediate form normalizes all memory
		// address equations to the scale-index-base form. The address
		// equation is availabe in base, index, and scale. If the offset
		// field has a non-zero value (one of 8, 16, 32, and 64), lval
		// will contain the memory offset. Note that base and index fields
		// contain the base and index register of the address equation,
		// in the form of an enumerated constant enum ud_type. scale
		// contains an integer value that the index register must be
		// scaled by.
		case UD_OP_MEM:
			{
				char buf[80];
				long long offset = 0;
				snprintf(buf, 80, "mem b %02x idx %02x scale %02x offs %02x",
				         op->base, op->index, op->scale, op->offset);

				MSG() << buf;

				_check(op->scale != 0, "!! implement register scaling");

				l4_umword_t addr = register_to_value(op->base);
				MSG() << "    reg " << std::hex << addr;

				switch(op->offset) {
					case 0:  offset = 0; break;
					case 8:  offset = op->lval.sbyte; break;
					case 16: offset = op->lval.sword; break;
					case 32: offset = op->lval.sdword; break;
					case 64: offset = op->lval.sqword; enter_kdebug("64bit offset"); break;
					default: enter_kdebug("unknown offset??");
				}

				MSG() << std::hex << addr << " + " << offset << " = " << addr + offset;
				addr += offset;

				// reading a full mword here is okay, because users of the
				// results returned from this function need to check the real
				// operand size anyway
				val = *(l4_umword_t*)_translator->translate(addr);

				MSG() << std::hex << addr << " := " << val;

				valid = true;
			}

			break;
		default:
			MSG() << "Need to handle " << operand_type_string(op);
			enter_kdebug("unhandled src operand type");
			break;
	}

	MSG() << std::hex << "v " << val
	        << (valid ? " (ok)" : " \033[31;1m(INV!)\033[0m")
	        << " ilen " << ilen();

	if (!valid)
		enter_kdebug("unhandled operand type");
	return val;
}


/*
 * Extract the offset encoded in an operand.
 *
 * This incorporates looking at the operand's size to figure out
 * the right masking. Plus, the result is _SIGNED_!
 */
l4_mword_t Romain::Emulator_base::offset_from_operand(ud_operand_t *op)
{
	uint8_t offs    = op->offset;
	long long value = 0;
	bool neg        = false;
	
	if (!offs) return op->lval.sword;

	/*
	 * Mask only the lower N bits
	 */
	value = op->lval.sdword & ((1LL << offs) - 1);
	neg   = value & (1LL << (offs-1));
	if (neg) {
		value = -((1LL << offs) - value);
	}

	// XXX: so far, we don't support 64bit offsets...
	return (int)value;
}


/*
 * Given a value, write it to whatever target is described by the
 * operand.
 *
 * So far, only memory targets are needed as we don't get to see writes to
 * other stuff, such as registers.
 */
void Romain::Emulator_base::value_to_operand(l4_umword_t val, ud_operand_t *op)
{
	switch(op->type) {
		// Memory operand. The intermediate form normalizes all memory
		// address equations to the scale-index-base form. The address
		// equation is availabe in base, index, and scale. If the offset
		// field has a non-zero value (one of 8, 16, 32, and 64), lval
		// will contain the memory offset. Note that base and index fields
		// contain the base and index register of the address equation,
		// in the form of an enumerated constant enum ud_type. scale
		// contains an integer value that the index register must be
		// scaled by.
		//
		// addr: = base + index * scale + offset
		case UD_OP_MEM:
			{
				char buf[80];
				snprintf(buf, 80, "b %02x idx %02x scale %02x offs %02x",
				         op->base, op->index, op->scale, op->offset);
				MSG() << buf;

				// no base reg, 32 bit size -> this is an address
				if (!op->base && op->offset) {
					l4_addr_t target = _translator->translate(op->lval.sdword);
					MSG() << std::hex
					        << "writing to address: (r " << op->lval.sdword
					        << " l " << target << ") := " << val;
					*(l4_umword_t*)target = val;
				}
				else if (op->base) { // else there must be at least a base addr
					l4_umword_t b_addr = register_to_value(op->base);
					MSG() << "BASE: " << std::hex << b_addr;
					l4_umword_t i_addr = op->index ? register_to_value(op->index) : 0;
					l4_umword_t scale  = op->scale;

					MSG() << std::hex << b_addr << " + (" << i_addr << " << " << scale << ") + "
					        << op->lval.sword << " = " << b_addr + (i_addr << scale) + offset_from_operand(op);
					b_addr = b_addr + (i_addr << scale) + offset_from_operand(op);

					l4_addr_t target = _translator->translate(b_addr);
					MSG() << "target: " << std::hex << target;
					// XXX: error check
					MSG() << std::hex
					        << "writing to address: (r " << b_addr
					        << " l " << target << ") := " << val;
					write_target(target, val, op->size);
				}
				else {
					MSG() << "strange mem encoding??";
					enter_kdebug("!");
				}
			}
			break;
		default:
			MSG() << "Need to handle " << operand_type_string(op);
			enter_kdebug("unhandled target operand");
			break;
	}
}


/*
 * Handle PUSH instruction
 */
void Romain::WriteEmulator::handle_push()
{
	l4_umword_t val = ~0;
	ud_operand_t *op = &_ud.operand[0];

	val = operand_to_value(op);

	Romain::Stack(_translator->translate(_vcpu->r()->sp)).push(val);

	_vcpu->r()->sp -= sizeof(l4_umword_t);
	_vcpu->r()->ip += ilen();
}


/*
 * Emulate a CALL instruction
 */
void Romain::WriteEmulator::handle_call()
{
	// push return address
	_vcpu->r()->ip += ilen();
	Romain::Stack(_translator->translate(_vcpu->r()->sp)).push(ip());

	// adapt EIP
	ud_operand_t *op = &_ud.operand[0];

	// XXX: check later, if this can be moved into operand_to_value(), too
	switch(op->type) {
		case UD_OP_JIMM:
			_check(op->size != 32, "!! immediate jmp offset not an mword");
			MSG() << std::hex << op->lval.sdword
			      << " " << _vcpu->r()->ip + op->lval.sdword;
			_vcpu->r()->ip += op->lval.sdword;
			break;

		case UD_OP_MEM: // fallthrough
		case UD_OP_REG:
			{
				l4_umword_t v = operand_to_value(op);
				MSG() << std::hex << v;
				_vcpu->r()->ip = v;
			}
			break;

		default:
			MSG() << "Unhandled: " << operand_type_string(op);
			enter_kdebug("unhandled target");
			break;
	}

	/* We must not touch the SP _before_ looking at the immediate value,
	 * because otherwise offset calculations might be wrong.
	 */
	_vcpu->r()->sp -= sizeof(l4_umword_t);
}


/*
 * Handle MOV instruction
 */
void Romain::WriteEmulator::handle_mov()
{
	ud_operand_t *op1 = &_ud.operand[0];
	ud_operand_t *op2 = &_ud.operand[1];

	l4_umword_t val = operand_to_value(op2);
	value_to_operand(val, op1);

	_vcpu->r()->ip += ilen();
}


/*
 * Handle (REP:)STOS instruction
 */
void Romain::WriteEmulator::handle_stos()
{
	_check(_ud.mnemonic != UD_Istosd, "non-word string copy");

	l4_umword_t count = _ud.pfx_rep != UD_NONE ? _vcpu->r()->cx : 1;

	MSG() << std::hex << "rep = 0x" << (int)_ud.pfx_rep;
	MSG() << "iterations: " << count;

	l4_addr_t base = _vcpu->r()->di;
	base = _translator->translate(base);

	for (l4_umword_t idx = 0; idx < count; ++idx) {
		*(l4_umword_t*)base = _vcpu->r()->ax;

		// XXX: Handle
		// 1) other stos-sizes than 4
		// 2) direction flag
		base += 4;
	}

	if (_ud.pfx_rep != UD_NONE) // we're done rep'ing
		_vcpu->r()->cx = 0;

	_vcpu->r()->di += count * sizeof(l4_umword_t);

	_vcpu->r()->ip += ilen();
}


/*
 * Handle MOVSD instruction
 */
void Romain::WriteEmulator::handle_movsd()
{
	_check(_ud.mnemonic != UD_Imovsd, "non-word memcopy");

	l4_umword_t count = _ud.pfx_rep != UD_NONE ? _vcpu->r()->cx : 1;
	MSG() << std::hex << "rep = 0x" << (int)_ud.pfx_rep;
	MSG() << "iterations: " << count;

	l4_addr_t src = _translator->translate(_vcpu->r()->si);
	l4_addr_t dst = _translator->translate(_vcpu->r()->di);

	for (l4_umword_t idx = 0; idx < count; ++idx) {
		*(l4_umword_t*)dst = *(l4_umword_t*)src;
		dst += 4;
		src += 4;
	}

	if (_ud.pfx_rep != UD_NONE) // we're done rep'ing
		_vcpu->r()->cx = 0;
	_vcpu->r()->si += count * sizeof(l4_umword_t);
	_vcpu->r()->di += count * sizeof(l4_umword_t);

	_vcpu->r()->ip += ilen();
}


/*
 * Handle MOVSB instruction
 */
void Romain::WriteEmulator::handle_movsb()
{
	_check(_ud.mnemonic != UD_Imovsb, "non-byte memcopy");

	l4_umword_t count = _ud.pfx_rep != UD_NONE ? _vcpu->r()->cx : 1;
	MSG() << std::hex << "rep = 0x" << (int)_ud.pfx_rep;
	MSG() << "iterations: " << count;

	l4_addr_t src = _translator->translate(_vcpu->r()->si);
	l4_addr_t dst = _translator->translate(_vcpu->r()->di);

	for (l4_umword_t idx = 0; idx < count; ++idx) {
		*(l4_uint8_t*)dst = *(l4_uint8_t*)src;
		dst++;
		src++;
	}

	if (_ud.pfx_rep != UD_NONE) // we're done rep'ing
		_vcpu->r()->cx = 0;
	_vcpu->r()->si += count * sizeof(l4_uint8_t);
	_vcpu->r()->di += count * sizeof(l4_uint8_t);

	_vcpu->r()->ip += ilen();
}


/*
 * Handle arithmetic instructions
 *
 * Arithmetics modify EFLAGS, too...
 */
void Romain::WriteEmulator::handle_arithmetics(ArithmeticOperations op)
{
	static char const *opstr[] = {"+", "-", "*", "/", "%", "--"};

	ud_operand_t *op1  = &_ud.operand[0];
	ud_operand_t *op2  = NULL; //  = &_ud.operand[1];
	l4_umword_t orig   = operand_to_value(op1);
	l4_umword_t arval  = 0; // operand_to_value(op2);
	l4_umword_t flags  = 0;

	switch(op) {
		case ADD:
		case SUB:
		case MULT:
		case DIV:
		case MOD:
			op2   = &_ud.operand[1];
			arval = operand_to_value(op2);
			break;
		case DEC:
			arval = 1;
			op = SUB;
	}

	MSG() << "value: " << std::hex << orig
	                          << opstr[op] << arval << " = ";

	switch (op) {
		case ADD:  orig += arval; break;
		case SUB:  orig -= arval; break;
		case MULT: orig *= arval; break;
		case DIV:  orig /= arval; break;
		case MOD:  orig %= arval; break;
		default: enter_kdebug("unknown arith op"); break;
	}

	/*
	 * Now obtain the flags and insert them into the
	 * already set flags.
	 */
	asm volatile ("pushf\n\t"
	              "pop %0"
	              : "=r" (flags));
	/* First, we plain copy the lowest 8 bits. */
	_vcpu->r()->flags = (_vcpu->r()->flags & 0xFFFFFF00) | (flags & 0xFF);
	/* The next three would be IF, TF, DF, which we don't want to touch.
	 * The remaining OF needs to be put in, though.
	 */
	if (flags & OverflowFlag) {
		_vcpu->r()->flags |= OverflowFlag;
	} else {
		_vcpu->r()->flags &= ~OverflowFlag;
	}

	MSG() << std::hex << "flags " << flags << " result " << orig;

	value_to_operand(orig, op1);

	_vcpu->r()->ip += ilen();
	MSG() << std::hex << "vcpu.eflags = " << _vcpu->r()->flags;
	//enter_kdebug("arith");
}

/*
 * Emulation entry point
 */
void Romain::WriteEmulator::emulate()
{
	//print_instruction(); // debugging
#define HANDLE(val, fn) \
	case val: fn(); break;
#define HANDLE_1(val, fn, arg) \
	case val: fn(arg); break;

	switch(_ud.mnemonic) {
		HANDLE(UD_Icall,  handle_call);
		HANDLE(UD_Imov,   handle_mov);
		HANDLE(UD_Ipush,  handle_push);
		HANDLE(UD_Istosd, handle_stos);
		HANDLE(UD_Imovsd, handle_movsd);
		HANDLE(UD_Imovsb, handle_movsb); // XXX merge with movsd
		HANDLE_1(UD_Isub, handle_arithmetics, SUB);
		HANDLE_1(UD_Idec, handle_arithmetics, DEC);
		default:
			MSG() << _ud.mnemonic;
			enter_kdebug("unhandled mnemonic");
			break;
	}
#undef HANDLE
#undef HANDLE_1
}


void Romain::Emulator_base::print_instruction()
{
	INFO() << "INSTR(" << std::setw(16) << ud_insn_hex(&_ud) << ") "
	       << std::setw(20) << ud_insn_asm(&_ud);
}

static unsigned long long rdtsc1()
{
	unsigned long long ret = 0;
	unsigned long hi, lo;
	asm volatile ("cpuid\t\n"
				  "rdtsc\t\n"
				  "mov %%edx, %0\n\t"
				  "mov %%eax, %1\n\t"
				  : "=r"(hi), "=r"(lo)
				  :
				  : "eax", "ebx", "ecx", "edx");
	ret = hi;
	ret <<= 32;
	ret |= lo;
	return ret;
}


static unsigned long long rdtsc2()
{
	unsigned long long ret = 0;
	unsigned long hi, lo;
	asm volatile ("rdtscp\n\t"
				  "mov %%edx, %0\n\t"
				  "mov %%eax, %1\n\t"
				  "cpuid\n\t"
				  : "=r"(hi), "=r"(lo)
				  :
				  : "eax", "ebx", "ecx", "edx");
	ret = hi;
	ret <<= 32;
	ret |= lo;
	//printf("%lx %lx %llx\n", hi, lo, ret);
	return ret;
}


#if 1
static unsigned long long t = 0;
static unsigned count = 0;
#endif

#include "instruction_length.h"

void Romain::CopyAndExecute::emulate(Romain::AddressTranslator *at)
{
	unsigned long long t1, t2;
#if 0
	MSG() << "CopyAndExecute::emulate() called @ " << std::hex << _vcpu->r()->ip
	      << "\n   local IP @ " << _local_ip << "\n   ilen " << _ilen;
#endif

	// XXX: need rewrite support for rep:movs, because this instruction would
	//      potentially use a second address that is not a shared mem address
	_local_ip = at->translate(_vcpu->r()->ip);
	_ilen     = mlde32((void*)_local_ip);

	//static char instbuf[32];
	//memset(_instbuf, 0x90, 32); // NOP
	t1 = rdtsc1();
	for (unsigned inc = 0; inc <= _ilen; inc += 4) {
		*(unsigned*)(_instbuf + inc) = *(unsigned*)(_local_ip + inc);
	}
	t2 = rdtsc2();
	//memcpy((void*)_instbuf, (void*)_local_ip, _ilen); // THE instruction
	_instbuf[_ilen] = 0xC3; // RET

	asm volatile(
				 "call *%5\n\t"
				 : "=a" (_vcpu->r()->ax),
				   "=c" (_vcpu->r()->cx),
				   "=d" (_vcpu->r()->dx),
				   "=S" (_vcpu->r()->si),
				   "=D" (_vcpu->r()->di)
				 : "r" (_instbuf), "a" (_vcpu->r()->ax),
				   "d" (_vcpu->r()->dx), "D" (_vcpu->r()->di),
				   "c" (_vcpu->r()->cx), "S" (_vcpu->r()->si)
				 : "cc", "memory"
	);


#if 1
	t += (t2-t1);

	count++;

	if (count >= 100000) {
		printf("DT: %lld %p %p\n", t / count, this, _instbuf);
		count = 0;
		t = 0;
	}
#endif
	_vcpu->r()->ip += _ilen;
}
