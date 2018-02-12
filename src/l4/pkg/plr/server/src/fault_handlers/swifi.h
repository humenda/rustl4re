#pragma once

/*
 * swifi.h --
 *
 *     Fault observer for fault injection experiments
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include "../asmjit/Assembler.h"
#include "../asmjit/MemoryManager.h"
#include "observers.h"
#include "debugging.h"
#include "../emulation"
#include "../app_loading"

/***************************************************************************
 * ASMJit instruction wrappers. Generate an ASMJit instruction using the   *
 * input operands from a given UDIS86 disassembled instruction.            *
 ***************************************************************************/


namespace Romain
{
/*
 * SEU simulation base class
 *
 * The flips are based on a common model:
 *   - place an INT3 on the instruction to target
 *   - upon the debug interrupt, revert the breakpoint and prepare the injection
 *     -> flip()
 *   - single-step or continue until next INT3
 *   - upon next INT1/INT3 perform post-exec operations
 *     -> revert()
 */
class Flipper : public Emulator_base
{
	protected:
		l4_addr_t       _flip_eip; // stores the original EIP that was flipped
		Romain::App_model *_am;
		l4_umword_t        _when;		// XXX
		l4_umword_t        _hitcount;	// XXX
		l4_umword_t        _interval;	// XXX
		bool            _repeat;	// XXX

	public:
		Flipper(L4vcpu::Vcpu *vcpu,
                Romain::App_model *am,
                Romain::App_instance *inst)
		: Emulator_base(vcpu, new Romain::AppModelAddressTranslator(am, inst)),
		      _flip_eip(vcpu->r()->ip), _am(am),
		      _when(0), _hitcount(0), _interval(1), _repeat(false)
		{
			_vcpu       = vcpu;
			_local_ip   = _translator->translate(ip());
			print_instruction();
		}

		virtual ~Flipper()
		{
			delete _translator;
		}

		l4_addr_t prev_eip() { return _flip_eip; }
		l4_addr_t next_eip() { return _flip_eip + ilen(); }

		/*
		 * Prepare flipped instruction
		 *
		 * Returns: true if we want to single-step afterwards, false if not.
		 */
		virtual bool flip()   = 0;
		virtual void revert() = 0;
};


class AsmJitUser
{
	public:
	static AsmJit::GPReg ud_to_jit_reg(ud_type t)
	{
#define MAP(x, y) case UD_R_##x: return AsmJit::y;
		switch(t) {
			MAP(EAX, eax); MAP(EBX, ebx); MAP(ECX, ecx); MAP(EDX, edx);
			MAP(ESI, esi); MAP(EDI, edi); MAP(ESP, esp);
			MAP(AX, ax); MAP(BX, bx); MAP(CX, cx); MAP(DX, dx);
			MAP(SI, si); MAP(DI, di); MAP(SP, sp);
			MAP(AH, ah); MAP(BH, bh); MAP(CH, ch); MAP(DH, dh);
			MAP(AL, al); MAP(BL, bl); MAP(CL, cl); MAP(DL, dl);
			default:
				ERROR() << "I don't know a mapping for " << (l4_umword_t)t << " yet.\n";
				break;
		}
		return AsmJit::no_reg;
#undef MAP
	}

	void disassemble_mem(l4_addr_t start, l4_addr_t remote, l4_umword_t size)
	{
		l4_umword_t count = 0;
		while (count < size)
			count += Romain::InstructionPrinter(start + count, remote + count).ilen();
	}

	void flip_bit_in_register(AsmJit::Assembler& as, AsmJit::GPReg& reg)
	{
		l4_umword_t i = random() % 32;
		as.xor_(reg, (1 << i));
	}

	void commit_asm(AsmJit::Assembler& as, Romain::App_model *_am,
	                L4vcpu::Vcpu *_vcpu)
	{
		as.int3(); // last instruction is an INT3 which will trigger the replica
		           // to return to the master
		void *ptr    = as.make();
		l4_umword_t len = as.getOffset();
		MSG() << "Created asm code @ " << ptr << ", size is " << len << " bytes";
		_check(!ptr, "code generation failed");
		Romain::dump_mem(ptr, len);

		memcpy((void*)_am->trampoline_local(), ptr, len);
		_vcpu->r()->ip = _am->trampoline_remote();
		disassemble_mem(_am->trampoline_local(), _am->trampoline_remote(), len);

		AsmJit::MemoryManager::getGlobal()->free(ptr);
	}
};

#define FUNC1(name, op) \
	struct name { \
		void operator() (AsmJit::Assembler& as, AsmJit::GPReg& r) \
		{ as.op(r); } };

FUNC1(Decrement, dec);
FUNC1(Increment, inc);
FUNC1(Negate, neg);

#define FUNC2(name, op) \
	struct name { \
		void operator() (AsmJit::Assembler& as, AsmJit::GPReg& op1, \
		                 AsmJit::GPReg& op2) { \
			as.op(op1, op2); } \
		void operator()  (AsmJit::Assembler& as, AsmJit::GPReg& op1, \
						  l4_umword_t imm) { \
			as.op(op1, imm); } \
	};

FUNC2(Addition, add);
FUNC2(Subtraction, sub);
FUNC2(And, and_);
FUNC2(Or, or_);
FUNC2(Xor, xor_);
FUNC2(ShiftLeft, shl);
FUNC2(ShiftRight, shr);

template <typename FN>
void UnaryOperation(ud_t ud, AsmJit::Assembler& as)
{
	_check(ud.operand[0].scale != 0, "scale?");
	AsmJit::GPReg op = AsmJitUser::ud_to_jit_reg(ud.operand[0].base);
	FN()(as, op);
}


template <typename FN>
void BinaryOperation(ud_t ud, AsmJit::Assembler& as)
{
	AsmJit::GPReg op1, op2;
	l4_mword_t imm = 0;
	MSG() << "BinaryOp " << (l4_umword_t)ud.operand[0].type
		<< " " << (l4_umword_t)ud.operand[1].type
		<< " " << (l4_umword_t)UD_OP_CONST;

	_check(ud.operand[0].type != UD_OP_REG, "1st op must be reg");
	_check(ud.operand[0].scale != 0, "scaling in reg??");
	_check((ud.operand[1].type != UD_OP_REG) &&
	       (ud.operand[1].type != UD_OP_CONST) &&
	       (ud.operand[1].type != UD_OP_IMM), "2nd must be reg or immediate");

	op1 =  AsmJitUser::ud_to_jit_reg(ud.operand[0].base);

	if (ud.operand[1].type == UD_OP_REG) {
		_check(ud.operand[1].scale != 0, "scaling in reg??");
		op2 = AsmJitUser::ud_to_jit_reg(ud.operand[1].base);
		FN()(as, op1, op2);
	} else if ((ud.operand[1].type == UD_OP_IMM) ||
	           (ud.operand[1].type == UD_OP_CONST)) {
		MSG() << "op size " << (l4_mword_t)ud.operand[1].size;
		switch(ud.operand[1].size) {
			case  8: imm = ud.operand[1].lval.sbyte; break;
			case 16: imm = ud.operand[1].lval.sword; break;
			case 32: imm = ud.operand[1].lval.sdword; break;
			case 64: abort(); break;
		}
		// XXX: sign extension bug -> some of the opcodes
		//      have immediates that are sign extended...
		MSG() << "Immediate: " << std::hex << imm;
		FN()(as, op1, imm);
	}
	MSG() << "code...";
}


/*
 * Emulate bit flips within general-purpose registers
 */
class GPRFlipEmulator : public Flipper
{
	/*
	 * The registers
	 */
	enum targetRegs {
		ax, bx, cx, dx, flags,
		si, di, bp, ip, sp,
		MAX_REG,
	};

	char const* reg_to_str(targetRegs r)
	{
#define CASE(reg) case reg: return #reg;
		switch(r) {
			CASE(ax); CASE(bx); CASE(cx); CASE(dx); CASE(flags);
			CASE(si); CASE(di); CASE(bp); CASE(ip); CASE(sp)
			CASE(MAX_REG);
		}
		return "???";
#undef CASE
	};

	targetRegs str_to_reg(char const *r)
	{
#define CASE(reg) if (strcmp(r, #reg) == 0) return reg;
		CASE(ax); CASE(bx); CASE(cx); CASE(dx); CASE(flags);
		CASE(si); CASE(di); CASE(bp); CASE(ip); CASE(sp);
#undef CASE

		return MAX_REG;
	}
 
 	l4_umword_t _target_reg;
 	l4_umword_t _target_bit;

	public:
		GPRFlipEmulator(L4vcpu::Vcpu *vcpu,
                        Romain::App_model *am,
                        Romain::App_instance *inst)
			: Flipper(vcpu, am, inst)
		{
			char const *reg = ConfigStringValue("swifi:register", "none");
			_target_reg = str_to_reg(reg);
			_target_bit = ConfigIntValue("swifi:bit", -1);

			DEBUG() << "REG: " << reg << "(" << _target_reg
					<< ") BIT: " << _target_bit;
		}

		void flip_reg(l4_umword_t reg, l4_umword_t bit)
		{
        #define FLIP(pos) \
			case pos: _vcpu->r()->pos ^= (1 << bit); break;

			switch(reg) {
				FLIP(ax); FLIP(bx); FLIP(cx); FLIP(dx); FLIP(flags);
				FLIP(si); FLIP(di); FLIP(bp); FLIP(ip); FLIP(sp);
			}
        #undef FLIP
		}

		virtual bool flip()
		{
			l4_umword_t reg = (_target_reg == MAX_REG) ?
				random() % MAX_REG : _target_reg;
			l4_umword_t bit = (_target_bit == -1) ?
				random() % 32 : _target_bit;

			MSG() << "selected (register, bit): (" << reg_to_str((targetRegs)reg)
			      << ", " << bit << ")";

			flip_reg(reg, bit);

			return false;
		}

	virtual void revert() { }
};


/*
* Instruction emulator that is used to flip a bit in instruction code
* (thereby emulating an SEU in the instruction decoder) and later on
* to revert it.
*/
class InstrFlipEmulator : public Flipper
{
	l4_addr_t _flip_addr;
	l4_addr_t _flip_bit;

public:
	InstrFlipEmulator(L4vcpu::Vcpu *vcpu,
	                  Romain::App_model *am,
	                  Romain::App_instance *inst)
		: Flipper(vcpu, am, inst),
		  _flip_addr(0), _flip_bit(0)
	{
	}

	virtual bool flip()
		{
			l4_umword_t len = ilen();
			len *= 8; // bits

			_flip_bit  = random() % len;
			_flip_addr = local();
			_flip_eip  = ip();

			flip_mem();

			InstructionPrinter(_flip_addr, _flip_eip);
			return true;
		}

		void virtual revert()
		{
			flip_mem();
			InstructionPrinter(_flip_addr, _flip_eip);
		}

		void flip_mem()
		{
			l4_addr_t start = _flip_addr;
			l4_addr_t bit   = _flip_bit;
			MSG() << std::hex << "-1- addr: " << start;
			MSG() << std::hex << "-2- bit:  " << bit;
			l4_umword_t byte = bit >> 3;

			start += byte;
			bit &= 0x7;

			MSG() << std::hex << "-3- flip byte: " << start;
			MSG() << std::hex << "-4- flip bit: " << bit;
			*(l4_uint8_t*)start ^= (1 << bit);
		}
};


/*
 * Inject ALU bitflips
 *
 * This class randomly choses from three potential errors:
 *   - instruction SEU -> the ALU performs an operation different
 *                        from what it is supposed to do
 *   - input SEU       -> one of the input operands encounters
 *                        a bit flip while being processed
 *   - output SEU      -> the output encounters a bit flip
 *                        before being written back to the target
 */
class ALUFlipEmulator : public Flipper,
                        public Romain::AsmJitUser
{
	enum ALUError {
		e_flip_instr,
		e_flip_input,
		e_flip_output,
		max_alu_error
	};

	ALUError      _mode;   // the selected ALU error mode
	ud_operand_t  _target; // flip output: determined target

	bool mnemonic_supported(l4_umword_t mnemonic)
	{
		switch(mnemonic) {
			case UD_Iinc: case UD_Idec: case UD_Iadd: case UD_Isub:
			case UD_Ishr: case UD_Ishl: case UD_Iand: case UD_Ior:
			case UD_Isar: case UD_Ixor: case UD_Ineg:
				return true;
			default:
				return false;
		}
	}

	public:
	ALUFlipEmulator(L4vcpu::Vcpu *vcpu,
	                Romain::App_model *am,
	                Romain::App_instance *inst)
	        : Flipper(vcpu, am, inst)
	{
		_mode = e_flip_instr; //(ALUError)(random() % 3);

		if (!mnemonic_supported(_ud.mnemonic)) {
			ERROR() << "Mnemonic " << _ud.mnemonic << " not yet supported.\n";
		}
	}


	virtual bool flip()
	{
		switch(_mode)
		{
			case e_flip_instr:  flip_instruction(); MSG() << "ALU::flip_instr()"; return false;
			case e_flip_input:  flip_input();       MSG() << "ALU::flip_in()"; return false;
			case e_flip_output: flip_output();      MSG() << "ALU::flip_out()"; return true;
			default: break;
		}
		return true;
	}


	/*
	 * Reverting only happens in the flip_output case. Otherwise,
	 * we leave the trampoline as is and the caller is responsible
	 * for setting the vCPU's EIP to the next valid instruction.
	 */
	virtual void revert()
	{
		l4_umword_t bit = random() % 32;

		if (_mode == e_flip_output) {
			MSG() << "flipping " << bit;
			_vcpu->print_state();
			switch(_target.base) {
				case UD_R_AX: case UD_R_AL: case UD_R_AH: case UD_R_EAX:
					_vcpu->r()->ax ^= (1 << bit); break;
				case UD_R_BX: case UD_R_BL: case UD_R_BH: case UD_R_EBX:
					_vcpu->r()->bx ^= (1 << bit); break;
				case UD_R_CX: case UD_R_CL: case UD_R_CH: case UD_R_ECX:
					_vcpu->r()->cx ^= (1 << bit); break;
				case UD_R_DX: case UD_R_DL: case UD_R_DH: case UD_R_EDX:
					_vcpu->r()->dx ^= (1 << bit); break;
				case UD_R_ESP:
					_vcpu->r()->sp ^= (1 << bit); break;
				case UD_R_ESI:
					_vcpu->r()->si ^= (1 << bit); break;
				case UD_R_EDI:
					_vcpu->r()->di ^= (1 << bit); break;
				default:
					ERROR() << "unhandled flip reg: " << _target.base << "\n";
			}
		}
	}


	/*
	 * Flip an instruction.
	 *
	 * Look at the original instruction and randomly select an alternative
	 * one using the same input parameters. This maps unary operations to
	 * unary operations and binary ops to binary ops again.
	 */
	
	void flip_instruction()
	{
		l4_mword_t r;
		AsmJit::Assembler as;

		if (_ud.operand[1].type == UD_NONE) { // single operand
			r = random() % 3;
			switch(r)
			{
#define CASE1(num, _class) case num: { UnaryOperation<_class>(_ud, as); } break;
				CASE1(0, Decrement); CASE1(1, Increment); CASE1(2, Negate);
#undef CASE1
			}
		} else {
			r = random() % 7;
			switch(r) {
#define CASE2(num, _class) case num: { BinaryOperation<_class>(_ud, as); } break;
				CASE2(0, Addition); CASE2(1, Subtraction);
				CASE2(2, ShiftLeft); CASE2(3, ShiftRight);
				CASE2(4, Or); CASE2(5, And); CASE2(6, Xor);
			}
#undef CASE2
		}
		commit_asm(as, _am, _vcpu);
	}


	/*
	 * Flip one input value.
	 *
	 * Selects one of the input values and generates code to flip a random bit
	 * within this value.
	 */
	void flip_input()
	{
		ud_operand_t o, p;
		AsmJit::Assembler as;
		l4_umword_t bit = random() % 32;

		/*
		 * Step 1: Determine operand.
		 */
		if (_ud.operand[1].type == UD_NONE) { // single operand
			o     = _ud.operand[0];
		} else {
			l4_mword_t i = random() % 2;
			o     = _ud.operand[i];
			p     = _ud.operand[1-i]; // the other reg
		}

		/*
		 * Step 2: generate XOR() operation to flip bit.
		 */
		if (o.type == UD_OP_REG) { // easy: simply xor with constant
			_check(o.index != UD_NONE, "indexing...");
			as.xor_(AsmJitUser::ud_to_jit_reg(o.base), (1 << bit));
		} else {
			/* If the target is not a register, we need to move it to one
			 * for XORing. For that purpose we use EAX unless it is the second
			 * operand, in which case we use EBX.
			 */
			AsmJit::GPReg scratch;
			switch (p.base) {
				case UD_R_AL: case UD_R_AH: case UD_R_AX: case UD_R_EAX:
					scratch = AsmJit::ebx;
				default: scratch = AsmJit::eax; break;
			}

			l4_umword_t value = o.lval.udword;
			as.push(scratch);       // push scratch to stack
			as.mov(scratch, value); // mov to scratch register
			                        // now calculate
			// XXX: shouldn't this be: XOR scratch register AND then move the
			//      result back to original memory address???
			as.xor_(AsmJitUser::ud_to_jit_reg(p.base), (1 << bit));
			as.pop(scratch);        // pop previously stored scratch register
		}

		commit_asm(as, _am, _vcpu);

		/*
		 * Step 3: We only generated code to flip the bit. We still need to execute
		 *         the original instruction. If we decrement the _flip_eip here by
		 *         the original instruction length, then a future restart will continue
		 *         at the original EIP:
		 */
		_flip_eip -= ilen();
	}
	

	/*
	 * Prepare output flip by determining the target register.
	 */
	void flip_output()
	{
		_check(_ud.operand[0].scale != UD_NONE, "scaling in target reg?");
		_target = _ud.operand[0];
	}
};


/*
 * Emulate SEU in the register allocation table which maps physical
 * registers to GPRs.
 *
 * When hitting an instruction, we determine which operand (if more than 1) is
 * going to be affected:
 *
 * 1) Input operand:
 *    - store current value
 *    - overwrite with randomized value
 *    - single-step
 *    - restore current value
 *
 * 2) Output operand:
 *    - store current value
 *    - overwrite with randomized value
 *    - single-step
 *    - write to randomized target
 *    - restore current value
 *
 * Randomization works as follows: We simulate a physical register file that is
 * 10x as large as the needed register file (this is the case on Intel's
 * Sandybridge architecture, which has 160 integer registers for 16 GPRs).
 * Thus, with a chance of 10% we pick a random other register from the available
 * GPRs, and with a chance of 90% we use a random input and ignore the output
 * value.
 */
class RATFlipEmulator : public Flipper
{
	ud_type  which;   // which operand is flipped
	l4_umword_t scratch; // temp storage
	l4_umword_t target;  // which target reg to use

	l4_umword_t randomize()
	{
		l4_umword_t x = random() % 10;
		if (x == 1) { // 10% chose real register
			x = random() % 8 + 1;
			switch(x) {
				case 1:  target = UD_R_EAX; break;
				case 2:  target = UD_R_EBX; break;
				case 3:  target = UD_R_ECX; break;
				case 4:  target = UD_R_EDX; break;
				case 5:  target = UD_R_ESP; break;
				case 6:  target = UD_R_EBP; break;
				case 7:  target = UD_R_ESI; break;
				case 8:  target = UD_R_EDI; break;
			}
			return register_to_value((ud_type)target);
		} else {
			target = 0;
			return random();
		}
	}

	public:
	RATFlipEmulator(L4vcpu::Vcpu *vcpu,
                    Romain::App_model *am,
                    Romain::App_instance *inst)
		: Flipper(vcpu, am, inst),
	      which(UD_NONE), scratch(0), target(0)
	{
		l4_umword_t opcount     = 0;
		l4_umword_t operands[4] = { ~0U, ~0U, ~0U, ~0U };
		enum { 
			RAT_IDX_MASK   = 0x0FF,
			RAT_IDX_OFFSET = 0x100
		};

		for (l4_umword_t i = 0; i < UD_MAX_OPERANDS; ++i) {
			/*
			 * Case 1: operand is a register
			 */
			if (_ud.operand[i].type == UD_OP_REG) {
				operands[opcount++] = i;
			} else if (_ud.operand[i].type == UD_OP_MEM) {
				/*
				 * Case 2: operand is memory op.
				 *
				 * In this case, we may have 2 registers involved for the
				 * index-scale address calculation.
				 */
#if 0
				MSG() << "mem " << _ud.operand[i].base << " "
				      << _ud.operand[i].index;
#endif
				if (_ud.operand[i].base != 0)  // 0 if hard-wired mem operand
					operands[opcount++] = i;
				if (_ud.operand[i].index != 0)
					operands[opcount++] = i + RAT_IDX_OFFSET;
			}
		}

		l4_umword_t rnd = opcount ? random() % opcount : 0;
		
		if (operands[rnd] > RAT_IDX_OFFSET) {
			which = _ud.operand[operands[rnd] - RAT_IDX_OFFSET].index;
		} else {
			which = _ud.operand[operands[rnd]].base;
		}
		MSG() << "operands " << opcount << " which " << which;
	}


	virtual bool flip()
	{
		/*
		 * In every case we need to get the original register value
		 * and restore it later on, because we emulate the real 
		 * register not being touched at all.
		 */
		scratch = register_to_value(which);
		value_to_register(randomize(), which);
		MSG() << "RAT: target " << std::hex << which
		      << " val " << scratch << " -> "
		      << register_to_value(which);
		return true;
	}


	virtual void revert()
	{
		MSG() << "RAT: revert " << which << " flip target " << target;
		if (which == _ud.operand[0].base) { // we work on an output reg
			switch(target) {
				case 0:   // output goes to nirvana, ignore
					break;
				default:  // output goes to another reg
				          // -> read from orig operand and write to target
				{
					l4_umword_t val = register_to_value(which);
					value_to_register(val, (ud_type)target);
				}
			}
		}

		value_to_register(scratch, which);
	}
};

class MemFlipEmulator : public Flipper,
                        public AsmJitUser
{
	public:
	MemFlipEmulator(L4vcpu::Vcpu *vcpu,
	                Romain::App_model *am,
	                Romain::App_instance *inst)
		: Flipper(vcpu, am, inst)
	{ }

	virtual bool flip()
	{
		l4_umword_t i = 0;
		for ( ; i < UD_MAX_OPERANDS; ++i) {
			if (_ud.operand[i].type == UD_OP_MEM)
				break;
		}

		MSG() << "Mem operand is #" << i;
		MSG() << "     base " << std::setw(2) << _ud.operand[i].base;
		MSG() << "    index " << std::setw(2) << _ud.operand[i].index;
		MSG() << "    scale " << std::setw(2) << (l4_mword_t)_ud.operand[i].scale;
		MSG() << "     offs " << std::setw(2) << (l4_mword_t)_ud.operand[i].offset;
		MSG() << "     lval " << std::setw(8) << std::hex << (l4_mword_t)_ud.operand[i].lval.sdword;

		l4_umword_t addr = 0;
		if (_ud.operand[i].base != 0)
			addr += register_to_value(_ud.operand[i].base);
		if (_ud.operand[i].index != 0)
			addr += (register_to_value(_ud.operand[i].index) << (l4_umword_t)_ud.operand[i].scale);
		switch(_ud.operand[i].offset) {
			case 0:  break;
			case 8:  addr += _ud.operand[i].lval.sbyte; break;
			case 16: addr += _ud.operand[i].lval.sword; break;
			case 32: addr += _ud.operand[i].lval.sdword; break;
			default: abort(); break;
		}

		MSG() << "target addr: " << std::hex << addr << " ("
		      << *(l4_umword_t*)_translator->translate(addr) << ")";

		AsmJit::Assembler as;
		ud_operand_t &udreg = _ud.operand[0];
		MSG() << "target op: " << udreg.base << "(" << register_to_value(udreg.base) << ")";
		AsmJit::GPReg tmp = udreg.base == UD_R_EAX ? AsmJit::ebx : AsmJit::eax;
		as.push(tmp);

		if (i == 0) { // output operand
			ud_operand_t &reg2 = _ud.operand[1];
			switch(reg2.type) {
				case UD_OP_CONST:
				case UD_OP_IMM:
					MSG() << "const: " << std::hex << _ud.operand[1].lval.sdword;
					as.push(AsmJit::ecx);
					as.mov(AsmJit::ecx, AsmJit::imm(reg2.lval.sdword));
					as.mov(tmp, AsmJit::imm(addr));
					
					flip_bit_in_register(as, tmp);
					
					as.mov(AsmJit::dword_ptr(tmp), AsmJit::ecx);
					as.pop(AsmJit::ecx);
					break;
				default:
					MSG() << _ud.operand[1].type;
					break;
			}
		} else { // input operand
			_check(_ud.operand[0].index  != 0, "indexing...");
			_check(_ud.operand[0].scale  != 0, "scaling...");
			_check(_ud.operand[0].offset != 0, "offsetting...");

			as.mov(tmp, AsmJit::imm(addr));

			flip_bit_in_register(as, tmp);

			as.mov(AsmJitUser::ud_to_jit_reg(udreg.base), AsmJit::dword_ptr(tmp));
		}

		as.pop(tmp);
		commit_asm(as, _am, _vcpu);

		return false;
	}

	virtual void revert()
	{ }
};

class SWIFIPriv : public Romain::SWIFIObserver
{
	enum targetFlags {
		None,
		GPR,    // register bit flip
		INSTR,  // instruction decoding bit flip
		ALU,    // ALU fault injection
		RAT,    // RAT fault injection
		MEM,    // flip in mem address
	};

	Breakpoint * _breakpoint;
	targetFlags  _flags;

	Flipper    * _flipper;

	l4_umword_t     _alu_mode;   // 0 - flip instr, 1 - flip input, 2 - flip output

	void flipper_trap1(Romain::App_thread* t);
	void flipper_trap3(Romain::App_thread* t, bool want_stepping = true);

	void remove_breakpoint(Romain::App_thread *t)
	{
		MSG() << "bp: " << std::hex << _breakpoint;
		if (_breakpoint) {
			delete _breakpoint;
			_breakpoint = 0;
			t->vcpu()->r()->ip--;
		}
	}

	l4_umword_t decode_insn(l4_addr_t local, l4_addr_t remote, ud_t *ud);

	public:
		SWIFIPriv();
		DECLARE_OBSERVER("swifi");
};
};
