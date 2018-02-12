#pragma once
/*
 * debugging.h --
 *
 *     Debugging interface
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */


#include "../log"
#include "../manager"
#include "../app_loading"

class Breakpoint
{
	enum {
		BREAK_INSTRUCTION = 0xCC,
	};

	l4_addr_t   _address;
	l4_umword_t _orig_code;
	l4_umword_t   _refcnt;

	public:
		Breakpoint(l4_addr_t addr)
			: _address(addr), _orig_code(0), _refcnt(0)
		{ }


		~Breakpoint()
		{ }


		void activate(Romain::App_instance *i, Romain::App_model *m)
		{
			++_refcnt;
			l4_addr_t local = m->rm()->remote_to_local(address(), i->id());
			DEBUG() << "Setting BP @ 0x" << std::hex << address()
			        << " -> local 0x" << local;
			
			if (*(l4_uint8_t*)local != BREAK_INSTRUCTION) {
				_orig_code = *(l4_umword_t*)local;
				*(l4_uint8_t*)local = BREAK_INSTRUCTION;
				DEBUG() << "addr @ " << std::hex << &_address;
			}
		}


		void deactivate(Romain::App_instance *i, Romain::App_model *m)
		{
			--_refcnt;
			DEBUG() << "Deactivating bp @ " << std::hex << address()
			        << " in instance " << i->id();
			DEBUG() << "(addr == " << &_address << ")";
			l4_addr_t local = m->rm()->remote_to_local(address(), i->id());
			*(l4_umword_t*)local = _orig_code;
		}


		l4_addr_t address() { return _address; }
		l4_umword_t orig()  { return _orig_code; }
		l4_umword_t refs() const { return _refcnt; }

		bool was_hit(Romain::App_thread *t)
		{
			return (t->vcpu()->r()->ip - 1) == _address;
		}
};
