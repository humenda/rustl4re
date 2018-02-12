#!/usr/bin/python

"""TSAR Event type definitions"""

import struct
import sys

#pylint: disable=C0103,R0903


class Event:
    """Event base class"""

    HEADSIZE = 13
    EVENTSIZE = 32

    SYSCALL_TYPE = 1
    PAGEFAULT_TYPE = 2
    SWIFI_TYPE = 3
    FOO_TYPE = 4
    TRAP_TYPE = 5
    THREAD_START_TYPE = 6
    THREAD_STOP_TYPE = 7
    LOCKING_TYPE = 8
    SHMLOCKING_TYPE = 9
    MAP_TYPE = 10
    BARNES_TYPE = 11

    @staticmethod
    def eventname(event):
        """Get name for event type"""
        syscall_names = ["INV", "SYS", "PF",
                         "SWIFI", "FOO", "TRAP",
                         "START", "STOP", "LOCK", "SHML",
                         "MAP", "BARNES"]
        return syscall_names[event]

    def __init__(self, time=0, typ=0, utcb=0, uid=None):
        self.timestamp = time
        self.type = typ
        self.utcb = utcb
        if uid is None:
            (self.id2, self.id1) = ("", self.utcb)
        else:
            (self.id2, self.id1) = uid.split(":")

    def uid(self):
        """Unique event source ID (NOT unique event ID!)"""
        return "%s:%s" % (self.id1, self.id2)

    def __repr__(self):
        try:
            return "%d [%8x|%5s] (%s,%s) " % (self.timestamp, self.utcb,
                                              Event.eventname(self.type),
                                              str(self.id1),
                                              str(self.id2))
        except TypeError:
            print self.timestamp
            sys.exit(1)

    def pretty(self):
        """Pretty-print event"""
        return " %d" % self.type


class SyscallEvent(Event):
    """Romain system call event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.SYSCALL_TYPE, utcb, uid)
        (self.eip, self.label, ) = struct.unpack_from("II",
                                                      raw[Event.HEADSIZE:])

    def __repr__(self):
        return Event.__repr__(self) + " SYSCALL %08x, ret to %08x" % \
            (self.label, self.eip)

    def pretty(self):
        return ["SYSCALL %08x" % (self.label),
                " ret -> %08x" % (self.eip)]


class PagefaultEvent(Event):
    """Page fault event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.PAGEFAULT_TYPE, utcb, uid)
        (self.writepf, ) = struct.unpack_from("B", raw[Event.HEADSIZE:])
        (self.pfa,
         self.local,
         self.remote) = struct.unpack_from("III", raw[Event.HEADSIZE + 1:])
        #print hex(self.pfa)

    def __repr__(self):
        res = Event.__repr__(self)
        if (self.writepf):
            res += " w"
        res += "r pf @ %08x" % (self.pfa)
        return res

    def pretty(self):
        res = []
        if self.writepf:
            res += ["wr pf @ 0x%x" % self.pfa]
        else:
            res += ["r pf @ 0x%x" % self.pfa]
        #res += ["%x -> %x" % (self.local, self.remote)]
        return res


class SwifiEvent(Event):
    """FI event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.SWIFI_TYPE, utcb, uid)

    def pretty(self):
        return ["SWIFI"]


class FooEvent(Event):
    """Foo test event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.FOO_TYPE, utcb, uid)
        (self.is_start, ) = struct.unpack_from("I", raw[Event.HEADSIZE:])

    def __repr__(self):
        res = Event.__repr__(self)
        if self.is_start == 0:
            res += " STOP"
        else:
            res += " START"
        return res

    def pretty(self):
        return ["FOO"]


class TrapEvent(Event):
    """Generic trap event"""
    counters = {}

    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.TRAP_TYPE, utcb, uid)
        (self.is_start, ) = struct.unpack_from("B", raw[Event.HEADSIZE:])
        (self.trapaddr, self.trapno, ) = \
            struct.unpack_from("II", raw[Event.HEADSIZE + 1:])

        #print "S %d T %d" % (self.is_start, self.trapno)

    def __repr__(self):
        res = Event.__repr__(self)
        if self.is_start == 1:
            res += " start, trapno %x" % self.trapno
        else:
            res += " done"
        return res

    def pretty(self):
        if self.is_start:
            return ["\033[33mTRAP %x @ %08x\033[0m" %
                    (self.trapno, self.trapaddr)]
        else:
            return ["--- TRAP END ---"]


class ThreadStartEvent(Event):
    """Thread start event"""

    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.THREAD_START_TYPE, utcb, uid)
        (self.EIP, ) = struct.unpack_from("L", raw[Event.HEADSIZE:])

    def __repr__(self):
        res = Event.__repr__(self)
        res += "Thread::Start"
        res += str(hex(self.EIP))
        return res

    def pretty(self):
        return ["\033[35mThread::Start\033[0m",
               ("EIP 0x%lx" % self.EIP)]


class ThreadStopEvent(Event):
    """Thread end event"""

    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.THREAD_STOP_TYPE, utcb, uid)

    def __repr__(self):
        res = Event.__repr__(self)
        res += "Thread::Exit"
        return res

    def pretty(self):
        return ["Thread::Exit"]


class LockingEvent(Event):
    """Lock interception event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.LOCKING_TYPE, utcb, uid)
        (self.locktype, self.lockptr) = \
            struct.unpack_from("II", raw[Event.HEADSIZE:])

        self.typenames = ["__lock", "__unlock", "mtx_lock", "mtx_unlock"]

    def __repr__(self):
        res = Event.__repr__(self)
        res += "%s(%lx)" % (self.typenames[self.locktype], self.lockptr)
        return res

    def pretty(self):
        return ["%s(%lx)" % (self.typenames[self.locktype], self.lockptr)]


class SHMLockingEvent(Event):
    """SHM locking event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.SHMLOCKING_TYPE, utcb, uid)
        (self.lockid, self.epoch, self.owner, self.evtype) = \
            struct.unpack_from("IIII", raw[Event.HEADSIZE:])

    def __repr__(self):
        res = Event.__repr__(self)
        res += "SHM %x, %d, %x" % (self.lockid, self.epoch, self.evtype)
        return res

    def pretty(self):

        st1 = ["down1", "down2", "up1", "up2"][self.evtype - 2]
        st1 += "(%x)" % self.lockid

        st2 = "  ID %x, EP %x" % (self.utcb, self.epoch)
        st3 = ""

        if self.evtype in [2, 5]:
            if self.owner == 0xffffffff:
                st3 = "(owner: %x)" % self.owner
            else:
                st3 = "(owner: \033[31;1m%x\033[0m)" % self.owner

        if self.evtype == 4:
            st1 += (" AQ %d" % self.epoch)

        if self.evtype == 2:
            return ["\033[34;1m==========\033[0m", st1, st2, st3]
        if self.evtype == 5:
            return [st1, st3, "\033[34m==========\033[0m"]

        return [st1, st2]


class MapEvent(Event):
    """Memory map/unmap event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.MAP_TYPE, utcb, uid)
        (self.local, self.remote, self.size, self.unmap) = \
            struct.unpack_from("IIIB", raw[Event.HEADSIZE:])

    def __repr__(self):
        res = Event.__repr__(self)
        if (self.unmap != 0):
            res += "un"
        res += "map(%x,%x,%x)" % (self.local, self.remote, self.size)
        return res

    def pretty(self):
        if (self.unmap == 0):
            st1 = "map sz "
        else: 
            st1 = "unmap sz "
        st1 += ("shift %x" % (self.size))
        st2 = "%08x -> %08x" % (self.local, self.remote)

        return [st1, st2]


class BarnesEvent(Event):
    """SPLASH2::Barnes runtime event"""
    def __init__(self, raw, time=0, utcb=0, uid=None):
        Event.__init__(self, time, Event.SHMLOCKING_TYPE, utcb, uid)
        (self.ptr, self.num, self.type) = \
            struct.unpack_from("III", raw[Event.HEADSIZE:])

    def __repr__(self):
        return "Barnes(%d) ptr %x num %d" % (self.type, self.ptr, self.num)

    def pretty(self):
        return ["\033[32mbarnes(%d)\033[0m" % self.type,
                "%x %x\033[0m" % (self.ptr, self.num)]
