/*
 * gdb_stub/gdb.cc --
 *
 *     GDB stub implemented on top of the Romain framework
 *
 * (c) 2011-2013 Björn Döbel <doebel@os.inf.tu-dresden.de>,
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */

#include <stddef.h>
#include <l4/sys/thread>
#include <l4/util/util.h>
#include <string.h>

#include "../app_loading"
#include "gdbserver"
#include "lwip/inet.h"

#define GDB_BYTE_FORMAT(reg) ntohl((unsigned int)(reg))
#define SAVED_VCPU_REGS _saved_vcpu.r()

#define MSG() DEBUGf(Romain::Log::Gdb)

/*
 * put a reply into the packet buffer
 */
#define REPLY_PACKET(fmt, ...) \
	do { \
		snprintf(_outbuf, sizeof(_outbuf), \
				 "$"fmt, ##__VA_ARGS__); \
		append_checksum(_outbuf+1); \
		MSG() << "\033[36;1m'" << _outbuf << "'\033[0m"; \
	} while (0)

/*
 * For any command not supported by the stub, an empty response (`$#00')
 * should be returned. That way it is possible to extend the protocol.
 * A newer GDB can tell if a packet is supported based on that response.
 */
#define UNSUPPORTED() REPLY_PACKET("")

#define REPLY_OK REPLY_PACKET("OK");

extern struct netif ankh_netif;

void Romain::GDBServerObserver::status() const { }

/*
 * GDB stub thread. Simply waits for commands to come in from the net and
 * handles them. This handling requires synchronization with the other threads,
 * which is done through internal signalling.
 */
static void *gdb_stub_thread(void* data)
{
	INFO() << "GDB Stub thread running.";

	Romain::GDBServerObserver *stub = (Romain::GDBServerObserver*)data;

	stub->connection()->setup_and_wait();
	stub->notify_connect();

	for (;;) {
		memset(stub->buffer(), 0, stub->maxbuffer());
		int num_bytes = stub->connection()->wait_for_cmd(stub->buffer(), stub->maxbuffer());
		if (num_bytes < 0) {
			stub->disconnect();
			stub->connection()->setup_and_wait();
			continue;
		}

		while (stub->handle_cmd() > 0)
			;
	}

	return 0;
}


Romain::GDBServerObserver::GDBServerObserver(Connection* con)
	: _con(con),
	  _connected(false),
	  _ack_mode(true),
	  _await_ack(true), // GDB sends an ACK first!
	  _bufptr(0),
	  _app_model(0),
	  _want_halt(true),
	  _singlestep(false)
{
	int r;
	
	memset(_inbuf, 0, sizeof(_inbuf));
	memset(_outbuf, 0, sizeof(_outbuf));
	memset(_last_cmd, 0, sizeof(_last_cmd));
	memset(&_saved_vcpu, 0, sizeof(_saved_vcpu));

	r = sem_init(&_app_wait_signal, 0, 0);
	_check(r != 0, "sem_init");
	r = sem_init(&_gdb_wait_signal, 0, 0);
	_check(r != 0, "sem_init");

	r = pthread_create(&_thread, 0, gdb_stub_thread, this);
	_check(r != 0, "pthread_create()");
}


Romain::GDBServerObserver::~GDBServerObserver()
{
}


/*
 * Startup notification. We know that the first thread has been started and thus
 * obtain an IP through DHCP and wait for a remote GDB to connect.
 */
void Romain::GDBServerObserver::startup_notify(Romain::App_instance *,
                                               Romain::App_thread *,
                                               Romain::Thread_group *,
                                               Romain::App_model *a)
{
	_app_model = a;

	while (!_connected) {
		INFO() << "waiting for GDB to connect...";
		sem_wait(&_app_wait_signal);
	}
}


/*
 * Trap notification.
 */
Romain::Observer::ObserverReturnVal
Romain::GDBServerObserver::notify(Romain::App_instance *,
                                  Romain::App_thread *t,
                                  Romain::Thread_group *,
                                  Romain::App_model *)
{
#if 0
	MSG() << "want halt: " << _want_halt
	      << " trap: " << t->vcpu()->r()->trapno
	      << " pending: 0x" << std::hex << t->get_pending_trap();
#endif

	/*
	 * Only handle traps 1 & 3. But we use _any_ trap notification
	 * to force the target thread into the debugger, e.g., in the
	 * case of an interrupt.
	 */
	if (!t->events_pending() &&
		(t->vcpu()->r()->trapno != 1) &&
		(t->vcpu()->r()->trapno != 3)) {
		if (_want_halt) {
			t->set_pending_trap(3);
		}
		return Romain::Observer::Ignored;
	}

	save_vcpu(t);
	
	if (_want_halt) {
		_want_halt = false;
	}

	//MSG() << "received HLT from GDB. Waiting for command.";
	notify_and_wait(&_gdb_wait_signal, &_app_wait_signal);

	restore_vcpu(t);

	return Romain::Observer::Continue;
}


/*
 * Verify a GDB packet checksum, which by its definition is
 * a sum of all text characters in the packet modulo 256.
 */
bool Romain::GDBServerObserver::checksum_cmd(char const * const ptr, char const **end)
{
	char const *c = ptr;
	unsigned sum = 0;

	for ( ; *c != '#'; sum += *c++)
		;

	*end = c;
	return (sum & 0xff) == (unsigned)strtol(c+1, 0, 16);
}


/*
 * Append checksum to a GDB packet.
 */
void Romain::GDBServerObserver::append_checksum(char *p)
{
	unsigned csum = 0;
	while (*p != 0)
		csum += *p++;
	snprintf(p, 4, "#%02x", csum & 0xFF);
}


bool Romain::GDBServerObserver::next_cmd()
{
	/* New content in packet buf? */
	if (!_bufptr) {
		_bufptr = _inbuf;
	}

	/* Out of buffer? */
	if (_bufptr - _last_cmd > 1024) { // XXX
		_bufptr = 0;
		return false;
	}

	switch (*_bufptr) {
		case NoCmd: // end of read buffer
			//MSG() << "EOR";
			_bufptr = 0;
			return false;

		case GDBInterrupt:
			break;

		case GDBAck:
			if (_await_ack) {
				_await_ack = false;
				++_bufptr;
			} else {
				MSG() << "Got ACK while not expecting one!";
				++_bufptr;
			}
			return next_cmd();

		case GDBRetry:
			// the last cmd is still in _last_cmd, so just
			// get back to it
			return true;

		case GDBCmdStart:
			_bufptr++; // skip the $ sign
			break;

		default:
			ERROR() << "invalid packet start: " << *_bufptr;
			break;
	}

	char const *eoc = 0;
	if (!checksum_cmd(_bufptr, &eoc)) {
		ERROR() << "Wrong checksum.";
	}

	_check(eoc - _bufptr <= 0, "cmd with 0 or negative size");

	memset(_last_cmd, 0, sizeof(_last_cmd));
	memcpy(_last_cmd, _bufptr, eoc - _bufptr);

	// set bufptr to _after_ the current cmd. this is where
	// we start searching for the next cmd then
	_bufptr = const_cast<char*>(eoc) + 3; // # + 2 bytes checksum == 3

	if (_ack_mode) {
		_check(_con->senddata("+", 2) != 2, "send ack");
	}

	return true;
}


/*
 * GDB command dispatcher.
 */
int Romain::GDBServerObserver::handle_cmd()
{
	if (!next_cmd())
		return 0;

	MSG() << "Need to handle cmd "
	      << "\033[35m" << _last_cmd << "\033[0m";

	char *cmd = _last_cmd;

	switch (*cmd) {
		case 'g': gdb_get_registers(cmd+1); break;
		case 'p': gdb_read_register(cmd+1); break;
		case 'P': gdb_write_register(cmd+1); break;

		case 'm': gdb_dump_mem(cmd+1); break;
		case 'M': gdb_write_mem(cmd+1, false); break;
		case 'H': gdb_select_thread(cmd+1); break;
		case 'k': disconnect(); return -1;
		case 'q': gdb_query(cmd+1); break;
		case 'Q': gdb_settings(cmd+1); break;

		case 'S': gdb_step(cmd+1, true); break;
		case 's': gdb_step(cmd+1, false); break;
		case 'C': gdb_continue(cmd+1, true); break;
		case 'c': gdb_continue(cmd+1, false); break;

		case 'v':
			if (strcmp(cmd, "vCont?") == 0)
				UNSUPPORTED();
			break;

		case 'X': gdb_write_mem(cmd+1, true); break;
		case 'Z': gdb_breakpoint(cmd+1); break;

		/*
		 * Queries the last stop reason. Initially, we return TARGET_SIGNAL_DEFAULT - 0x90
		 * TODO: implement
		 */
		case '?': REPLY_PACKET("S90"); break;
		
		default:
			  INFO() << "Unhandled GDB command: " << cmd;
			  break;
	}

	_await_ack = _ack_mode;
	int err    = _con->senddata(_outbuf, strlen(_outbuf));
	_check(err <= 0, "send reply");

	return 1;
}


/*
 * Handle GDB settings modification.
 *
 * We only support StartNoAckMode so far.
 */
void Romain::GDBServerObserver::gdb_settings(char const * const cmd)
{
	//MSG() << "Q: " << cmd;

	if (strcmp(cmd, "StartNoAckMode") == 0) {
		_ack_mode = false;
		REPLY_OK;
	}
	else {
		MSG() << "Unsupported Q cmd: " << cmd;
		UNSUPPORTED();
	}
}


/*
 * Handle remote query.
 *
 * Currently only returns supported features.
 */
void Romain::GDBServerObserver::gdb_query(char const * const cmd)
{
	//MSG() << "CMD: " << cmd;

	if (strncmp(cmd, "Supported", (size_t)9) == 0) // feature query
		REPLY_PACKET("PacketSize=%d;QStartNoAckMode+;", sizeof(_outbuf));
	else if (strncmp(cmd, "Symbol::", 8) == 0) {
		REPLY_OK;
	}
	else {
		MSG() << "Unsupported query cmd: " << cmd;
		UNSUPPORTED();
	}
}


/*
 * `H c thread-id' : Set thread for subsequent operations (`m', `M', `g', `G', et.al.).
 *    c depends on the operation to be performed: it should be `c' for step and continue
 *    operations, `g' for other operations. The thread designator thread-id has the format
 *    and interpretation described in thread-id syntax.
 *
 * Reply:
 *  `OK'    for success
 *  `E NN'  for an error
 */
void Romain::GDBServerObserver::gdb_select_thread(char const * const cmd)
{
	(void)cmd;
	//MSG() << "CMD: " << cmd;

#if 0
	if (*cmd == 'g') {
	    if (atoi(cmd+1) > 0) {
		ERROR() << "g cmd for thread" << atoi(cmd+1);
	    }
		REPLY_OK;
	}
	else if (*cmd == 'c') {
    	if (atoi(cmd+1) > 0) {
			ERROR() << "c cmd for thread" << atoi(cmd+1);
	    }
	    UNSUPPORTED();
	}
	else {
		ERROR() << "Unsupported H cmd: " << cmd;
		UNSUPPORTED();
	}
#endif
	UNSUPPORTED();
}


/*
 * `g' Read general registers.
 *
 * Reply:
 *
 * `XX...'   Each byte of register data is described by two hex digits. The bytes
 *           with the register are transmitted in target byte order. The size of
 *           each register and their position within the `g' packet are determined
 *           by the gdb internal gdbarch functions DEPRECATED_REGISTER_RAW_SIZE and
 *           gdbarch_register_name. The specification of several standard `g'
 *           packets is specified below.
 *
 * `E NN'    for an error.
 */
void Romain::GDBServerObserver::gdb_get_registers(char const * const)
{
	REPLY_PACKET("%08x%08x%08x%08x%08x%08x%08x%08x"
	             "%08x%08x%08x%08x%08x%08x%08x%08x",
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->ax),     GDB_BYTE_FORMAT(SAVED_VCPU_REGS->cx),
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->dx),     GDB_BYTE_FORMAT(SAVED_VCPU_REGS->bx),
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->sp),     GDB_BYTE_FORMAT(SAVED_VCPU_REGS->bp),
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->si),     GDB_BYTE_FORMAT(SAVED_VCPU_REGS->di),
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->ip),     GDB_BYTE_FORMAT(SAVED_VCPU_REGS->flags),
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->dummy1), GDB_BYTE_FORMAT(SAVED_VCPU_REGS->ss),
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->ds),     GDB_BYTE_FORMAT(SAVED_VCPU_REGS->es),
	             GDB_BYTE_FORMAT(SAVED_VCPU_REGS->fs),     GDB_BYTE_FORMAT(SAVED_VCPU_REGS->gs));
}

/*
 * `m addr,length' Read length bytes of memory starting at address addr. Note that addr
 *                 may not be aligned to any particular boundary. The stub need not use
 *                 any particular size or alignment when gathering data from memory for
 *                 the response; even if addr is word-aligned and length is a multiple of
 *                 the word size, the stub is free to use byte accesses, or not. For this
 *                 reason, this packet may not be suitable for accessing memory-mapped I/O
 *                 devices.
 *
 * Reply:
 *  `XX…' Memory contents; each byte is transmitted as a two-digit hexadecimal number. The
 *        reply may contain fewer bytes than requested if the server was able to read only
 *        part of the region of memory.
 *
 * `E NN' NN is errno
 */
void Romain::GDBServerObserver::gdb_dump_mem(char const * const cmd)
{
	unsigned addr, len;

	if (sscanf(cmd, "%x,%x", &addr, &len) != 2) {
		ERROR() << "Incorrectly matching memory read cmd: " << cmd;
	}

	// XXX: so far we only support the gdb stub for the first running
	//      instance XXX multi-instance support?
	l4_addr_t local_addr = _app_model->rm()->remote_to_local(addr, 0);
	if (local_addr == ~0UL) // nothing mapped?
		REPLY_PACKET("");
	else {

		/*
		 * Validate buffer length. The memory dump may cross a page boundary
		 * and the next page might not be attached at all. To avoid unhandled
		 * page faults here, we check the end address and if it does not resolve
		 * successfully, we cut the length until the end of the first page.
		 *
		 * This assumes that GDB will never try to dump more than a page or even
		 * regions with pages unmapped in between.
		 */
		if (l4_trunc_page(local_addr) != l4_trunc_page(local_addr + len)) {
			// XXX: so far we only support the gdb stub for the first running
			//      instance XXX multi-instance support?
			l4_addr_t end_addr = _app_model->rm()->remote_to_local(local_addr + len, 0);
			if (end_addr == ~0UL) {
				len = l4_round_page(local_addr) - local_addr;
			}
		}

		//MSG() << "local @ " << (void*)local_addr;
		_outbuf[0] = '$';
		// we print 2 characters at a time - representing one byte in hex; as long
		// as the length field tells us
		for (unsigned i = 0; i < len; ++i) {
			char *start = &_outbuf[1];
			start += 2*i;
			//MSG() << (void*)start << " <-> " << std::hex << (int)*(unsigned char*)(local_addr + i);
			snprintf(start, 3, "%02x", *(unsigned char*)(local_addr + i));
		}
		append_checksum(_outbuf+1);
		MSG() << "\033[36;1m'" << _outbuf << "'\033[0m";
	}
}


void Romain::GDBServerObserver::gdb_continue(const char* cmd, bool withSignal)
{
	/* XXX
	 * For the signal case, we simply assume for now that we are continuing
	 * from the last signal, in which case the command behaves the same as with
	 * no signal specified. Later, we should verify this.
	 * XXX */
	if (withSignal || (strlen(cmd) == 0)) {
		// disable single-stepping just in case it was enabled
		_singlestep = false;
		SAVED_VCPU_REGS->flags &= ~(1 << 8);
		notify_and_wait(&_app_wait_signal, &_gdb_wait_signal);

		signal_return(/*_notifyThread*/);
	} else {
		ERROR() << "continue from address? " << cmd;
	}
}


void Romain::GDBServerObserver::gdb_step(const char* cmd, bool withSignal)
{
	_want_halt = false;
	if ((!withSignal && (strlen(cmd) == 0)) || 
		( withSignal && (strcmp(cmd, "90") == 0))) {
		_singlestep = true;
		SAVED_VCPU_REGS->flags |= (1 << 8);

		notify_and_wait(&_app_wait_signal, &_gdb_wait_signal);
		signal_return(/*_notifyThread*/);
	} else {
		printf("step: '%s'\n", cmd);
		ERROR() << "step from address not implemented!";
	}
}


void Romain::GDBServerObserver::gdb_breakpoint(char const* cmd)
{
	(void)cmd;
	// XXX Handle breakpoints
	UNSUPPORTED();
}


void Romain::GDBServerObserver::gdb_write_mem(char const* cmd, bool binaryData)
{
	unsigned addr, len;
	if (sscanf(cmd, "%x,%x", &addr, &len) != 2) {
		ERROR() << "Invalid memory write cmd: " << cmd;
	}

	if (!len) { // nothing to do
		REPLY_OK;
		return;
	}

	// XXX: so far we only support the gdb stub for the first running
	//      instance XXX multi-instance support?
	l4_addr_t local_addr = _app_model->rm()->remote_to_local(addr, 0);
	MSG() << "local: " << std::hex << (void*)local_addr;
	if (local_addr == ~0UL) {// nothing mapped?
		REPLY_PACKET("");
	} else {
		unsigned char const* data_start;
		unsigned char const* data_end;
		unsigned char const *c = (unsigned char const*)cmd;

		MSG() << "local value: " << std::hex << *(unsigned char*)local_addr;
		// find first data byte
		while (*c != ':')
			++c;
		data_start = ++c;

		// find last data byte
		// (note: dispatcher already removed the # before)
		while (*c != 0)
			++c;
		data_end = c-1;

#if 0
		MSG() << "data start @ " << (void*)data_start
		      << " data end @ "  << (void*)data_end
		      << " cmd @ " << (void*)cmd;
#endif

		if (binaryData) {
#if 0
			for (c = data_start; c <= data_end; ++c) {
				printf("%02X ", *c);
			}
			printf("\n");
#endif
			if ((unsigned)(data_end - data_start + 1) > len) {
				ERROR() << "Packet says it contains more data than it really does?";
			}
			
			MSG() << "memcpy(" << (void*)local_addr << ", "
			        << (void*)data_start << ", " << len << ");";
			memcpy((char*)local_addr, data_start, len);
		} else {
		}

		REPLY_OK;
	}
}


void Romain::GDBServerObserver::gdb_read_register(char const* cmd)
{
	(void)cmd;
	UNSUPPORTED();
}


/*
 * Write register n... with value r.... The register number n is in
 * hexadecimal, and r... contains two hex digits for each byte in the register
 * (target byte order).
 *
 * Reply:
 *
 * `OK'     for success
 * `E NN'   for an error
 */
void Romain::GDBServerObserver::gdb_write_register(char const *cmd)
{
	unsigned regno, value;
	MSG() << "write register: " << cmd;
	sscanf(cmd, "%x=%x", &regno, &value);
	MSG() << std::hex << value << " " << ntohl(value);

	switch(regno) {
#define REGCASE(r) case Romain::r: SAVED_VCPU_REGS->r = ntohl(value); break

		REGCASE(ax); REGCASE(bx); REGCASE(cx); REGCASE(dx);
		REGCASE(sp); REGCASE(ip); REGCASE(flags);
		REGCASE(ss); REGCASE(ds); REGCASE(es); REGCASE(fs);
		REGCASE(gs);
#undef REGCASE

		default:
			MSG() << "Unhandled reg number: " << regno;
			break;
	}
	REPLY_PACKET("OK");
}


void Romain::GDBServerObserver::signal_return(Romain::App_thread *t) {
	if (!t) t = _notifyThread;

	if (t->unhandled_pf()) {
		REPLY_PACKET("S0B"); // SEGV
	} else {
		REPLY_PACKET("S05"); // Breakpoint trap
	}
}
