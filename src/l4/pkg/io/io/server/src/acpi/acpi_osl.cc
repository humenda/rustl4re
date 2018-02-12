/*
 * (c) 2010 Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 *
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU General Public License 2.
 * Please see the COPYING-GPL-2 file for details.
 */
#include <l4/sys/compiler.h>
#include <l4/sigma0/sigma0.h>

#include "debug.h"

__BEGIN_DECLS
#include "acpi.h"
#include "acpiosxf.h"
__END_DECLS

#include "pci.h"
#include "res.h"

ACPI_STATUS
AcpiOsEnterSleep (
    UINT8                   SleepState,
    UINT32                  RegaValue,
    UINT32                  RegbValue)
{
  (void) SleepState;
  (void) RegaValue;
  (void) RegbValue;
  d_printf(DBG_ERR, "error: AcpiOsEnterSleep must never be used in our case...\n");
  return AE_ERROR;
}

#if defined(ARCH_amd64) || defined(ARCH_x86)

#include <l4/util/port_io.h>
#define DEBUG_OSL_PORT_IO 0
/*
 * Platform and hardware-independent I/O interfaces
 */
ACPI_STATUS
AcpiOsReadPort (
        ACPI_IO_ADDRESS                 address,
        UINT32                         *value,
        UINT32                          width)
{
  if (DEBUG_OSL_PORT_IO)
    d_printf(DBG_ALL, "IN: adr=0x%lx, width=%i\n",
             (unsigned long)address, width);

  if (address == 0x80)
    return AE_OK;

  switch (width)
    {
    case 8:
      if (res_get_ioport(address, 0) < 0)
        return AE_BAD_PARAMETER;
      *value = l4util_in8((l4_uint16_t)address);
      break;
    case 16:
      if (res_get_ioport(address, 1) < 0)
        return AE_BAD_PARAMETER;
      *value = l4util_in16((l4_uint16_t)address);
      break;
    case 32:
      if (res_get_ioport(address, 2) < 0)
        return AE_BAD_PARAMETER;
      *value = l4util_in32((l4_uint16_t)address);
      break;
    default :
      return AE_BAD_PARAMETER;
    }
  if (DEBUG_OSL_PORT_IO)
    d_printf(DBG_ALL, "\tport(0x%lx)=>0x%x\n", (unsigned long)address, *value);
  return AE_OK;
}

ACPI_STATUS
AcpiOsWritePort (
        ACPI_IO_ADDRESS                 address,
        UINT32                          value,
        UINT32                          width)
{
  if (DEBUG_OSL_PORT_IO)
    d_printf(DBG_ALL, "\tport(0x%lx)<=0x%x\n", (unsigned long)address, value);

  if (address == 0x80)
    return AE_OK;

  switch (width)
    {
    case 8:
      if (res_get_ioport(address, 0) < 0)
        return AE_BAD_PARAMETER;
      l4util_out8((l4_uint8_t)value,(l4_uint16_t)address);
      break;
    case 16:
      if (res_get_ioport(address, 1) < 0)
        return AE_BAD_PARAMETER;
      l4util_out16((l4_uint16_t)value,(l4_uint16_t)address);
      break;
    case 32:
      if (res_get_ioport(address, 2) < 0)
        return AE_BAD_PARAMETER;
      l4util_out32((l4_uint32_t)value,(l4_uint32_t)address);
      break;
    default :
      return AE_BAD_PARAMETER;
    }
  return AE_OK;
}
#else

ACPI_STATUS
AcpiOsReadPort (
        ACPI_IO_ADDRESS                 /*address*/,
        UINT32                        * /*value*/,
        UINT32                        /*width*/)
{
  return AE_NO_MEMORY;
}

ACPI_STATUS
AcpiOsWritePort (
        ACPI_IO_ADDRESS                 /*address*/,
        UINT32                        /*value*/,
        UINT32                        /*width*/)
{
  return AE_NO_MEMORY;
}
#endif

void *
AcpiOsMapMemory (
        ACPI_PHYSICAL_ADDRESS           where,
        ACPI_SIZE                       length)
{
  void *virt = (void*)res_map_iomem(where, length);

  d_printf(DBG_DEBUG, "%s(%lx, %lx) = %lx\n",
                      __func__, (unsigned long)where, (unsigned long)length,
                      (unsigned long)virt);

  return virt;
}

void
AcpiOsUnmapMemory (
        void                            *logical_address,
        ACPI_SIZE                       size)
{
  (void)logical_address;
  (void)size;
//  l4io_release_iomem((l4_addr_t)logical_address, size);
  return;
}



/******************************************************************************
 *
 * FUNCTION:    AcpiOsReadPciConfiguration
 *
 * PARAMETERS:  PciId               Seg/Bus/Dev
 *              Register            Device Register
 *              Value               Buffer where value is placed
 *              Width               Number of bits
 *
 * RETURN:      Status
 *
 * DESCRIPTION: Read data from PCI configuration space
 *
 *****************************************************************************/

extern inline
l4_uint32_t
pci_conf_addr(l4_uint32_t bus, l4_uint32_t dev, l4_uint32_t fn, l4_uint32_t reg)
{ return 0x80000000 | (bus << 16) | (dev << 11) | (fn << 8) | (reg & ~3); }

ACPI_STATUS
AcpiOsReadPciConfiguration (
        ACPI_PCI_ID             *PciId,
        UINT32                  Register,
        UINT64                  *Value,
        UINT32                  Width)
{
  //printf("%s: ...\n", __func__);
  Hw::Pci::Root_bridge *rb = Hw::Pci::root_bridge(PciId->Segment);
  if (!rb)
    {
      if (Register >= 0x100)
        {
          d_printf(DBG_ERR, "error: PCI config space register out of range\n");
          return AE_BAD_PARAMETER;
        }

      Hw::Pci::Port_root_bridge prb(0, 0, Hw::Pci::Bus::Pci_bus, 0);
      int r = prb.cfg_read(Hw::Pci::Cfg_addr(PciId->Bus, PciId->Device,
                                             PciId->Function, Register),
                           (l4_uint32_t *)Value, Hw::Pci::cfg_w_to_o(Width));

      if (r < 0)
        return AE_BAD_PARAMETER;

      return AE_OK;
    }

  int r = rb->cfg_read(PciId->Bus, (PciId->Device << 16) | PciId->Function,
                       Register, (l4_uint32_t *)Value,
                       Hw::Pci::cfg_w_to_o(Width));

  if (r < 0)
    return AE_BAD_PARAMETER;

  return AE_OK;
}


/******************************************************************************
 *
 * FUNCTION:    AcpiOsWritePciConfiguration
 *
 * PARAMETERS:  PciId               Seg/Bus/Dev
 *              Register            Device Register
 *              Value               Value to be written
 *              Width               Number of bits
 *
 * RETURN:      Status.
 *
 * DESCRIPTION: Write data to PCI configuration space
 *
 *****************************************************************************/

ACPI_STATUS
AcpiOsWritePciConfiguration (
        ACPI_PCI_ID             *PciId,
        UINT32                  Register,
        ACPI_INTEGER            Value,
        UINT32                  Width)
{
  //printf("%s: ...\n", __func__);
  Hw::Pci::Root_bridge *rb = Hw::Pci::root_bridge(PciId->Segment);
  if (!rb)
    {
      if (Register >= 0x100)
        {
          d_printf(DBG_ERR, "error: PCI config space register out of range\n");
          return AE_BAD_PARAMETER;
        }

      Hw::Pci::Port_root_bridge prb(0, 0, Hw::Pci::Bus::Pci_bus, 0);
      int r = prb.cfg_write(Hw::Pci::Cfg_addr(PciId->Bus, PciId->Device,
                                              PciId->Function, Register),
                            Value, Hw::Pci::cfg_w_to_o(Width));

      if (r < 0)
        return AE_BAD_PARAMETER;

      return AE_OK;
    }

  int r = rb->cfg_write(PciId->Bus, (PciId->Device << 16) | PciId->Function,
                        Register, Value, Hw::Pci::cfg_w_to_o(Width));

  if (r < 0)
    return AE_BAD_PARAMETER;

  return AE_OK;

  return (AE_OK);
}

// multi-threading -------------------------------------------------

#include <pthread.h>
#include <semaphore.h>
#include <time.h>

namespace {

// fwd, see below
class Async_work_queue;

/**
 * Asynchronous work item, runs a callback with a data pointer.
 */
class Async_work
{
public:
  /// function pointer for the callback
  typedef void (*Callback)(void *);

  /// create work item, with callback and data
  Async_work(Callback callback, void *data)
  : _d(data), _cb(callback)
  {}

  /// execute the work item, ussually handled by a work queue
  void exec()
  { _cb(_d); }

private:
  // sets _q directly
  friend class Async_work_queue;

  void *_d;
  Callback _cb;
  Async_work_queue *_q = 0;
};

/**
 * Simple asynchronous work queue.
 *
 * This implementation executes all work items independently and
 * possibly in parallel (in any order).
 */
class Async_work_queue
{
public:
  /// flush the work queue (blocks until all work items are finished.
  void flush()
  {
    if (!_num_running)
      return;

    pthread_mutex_lock(&_l);
    while (_num_running)
      pthread_cond_wait(&_c, &_l);
    pthread_mutex_unlock(&_l);
  }

  /// schedule the given work item
  int schedule(cxx::unique_ptr<Async_work> work)
  {
    pthread_t t;
    pthread_mutex_lock(&_l);
    ++_num_running;
    pthread_mutex_unlock(&_l);
    work->_q = this;
    int r = pthread_create(&t, NULL, _exec, work);
    if (r != 0)
      {
        bool signal = false;
        pthread_mutex_lock(&_l);
        signal = (--_num_running == 0);
        pthread_mutex_unlock(&_l);
        if (signal)
          pthread_cond_broadcast(&_c);

        return r;
      }
    work.release();
    pthread_detach(t);
    return 0;
  }

private:
  void work_done()
  {
    bool signal = false;
    pthread_mutex_lock(&_l);
    signal = (--_num_running == 0);
    pthread_mutex_unlock(&_l);
    if (signal)
      pthread_cond_broadcast(&_c);
  }

  static void *_exec(void *work)
  {
    Async_work *w = static_cast<Async_work *>(work);
    w->exec();
    w->_q->work_done();
    delete w;
    return 0;
  }

  pthread_mutex_t _l = PTHREAD_MUTEX_INITIALIZER;
  pthread_cond_t _c = PTHREAD_COND_INITIALIZER;
  unsigned _num_running = 0;
};

/// work queue for AcpiOsExecute
static Async_work_queue acpi_work_queue;

}

ACPI_THREAD_ID
AcpiOsGetThreadId(void)
{
  return (ACPI_THREAD_ID)pthread_self();
}

ACPI_STATUS
AcpiOsExecute(
       ACPI_EXECUTE_TYPE                type,
       ACPI_OSD_EXEC_CALLBACK           function,
       void                            *context)
{
  (void)type;
  int r = acpi_work_queue.schedule(cxx::make_unique<Async_work>(function, context));
  if (r < 0)
    return AE_ERROR;
  return AE_OK;
}

void
AcpiOsWaitEventsComplete()
{
  acpi_work_queue.flush();
}


ACPI_STATUS
AcpiOsCreateSemaphore (
        uint32_t,
        uint32_t                        initial_units,
        ACPI_SEMAPHORE                  *out_handle)
{
  sem_t *sem = new sem_t;
  if (sem_init(sem, 0, initial_units) < 0)
    {
      perror("error: cannot initialize semaphore");
      return AE_ERROR;
    }

  *out_handle = sem;
  return AE_OK;
}

ACPI_STATUS
AcpiOsDeleteSemaphore (
        ACPI_SEMAPHORE                  handle)
{
  sem_t *s = static_cast<sem_t*>(handle);
  sem_destroy(s);
  delete s;
  return AE_OK;
}

inline ACPI_STATUS calc_timeout(uint16_t timeout, struct timespec *to)
{
  if (timeout == 0xffff)
    return AE_OK;

  if (clock_gettime(CLOCK_REALTIME, to) < 0)
    {
      perror("error: cannot get current time");
      return AE_ERROR;
    }

  // check for normalized time (tv_nsec < 1s)
  if (L4_UNLIKELY(to->tv_nsec >= 1000000000))
    return AE_ERROR;

  to->tv_sec += timeout / 1000;
  to->tv_nsec += (timeout % 1000) * 1000000;

  if (to->tv_nsec >= 1000000000)
    {
      ++to->tv_sec;
      to->tv_nsec -= 1000000000;
    }

  return AE_OK;
}

ACPI_STATUS
AcpiOsWaitSemaphore (
        ACPI_SEMAPHORE                  handle,
        uint32_t                        units,
        uint16_t                        timeout)
{
  struct timespec to;
  ACPI_STATUS r = calc_timeout(timeout, &to);
  if (ACPI_FAILURE(r))
    return r;

  sem_t *sem = static_cast<sem_t*>(handle);
  while (units > 0)
    {
      int r;
      if (timeout == 0xffff)
        r = sem_wait(sem);
      else
        r = sem_timedwait(sem, &to);

      if (r < 0)
        {
          switch (errno)
            {
            case EINVAL: return AE_BAD_PARAMETER;
            case EINTR:  continue; // retry after signal
            case EAGAIN: continue;
            case ETIMEDOUT: return AE_TIME;
            default: return AE_ERROR;
            }
        }

      // got the semaphore, do again unit units == 0
      --units;
    }
  return AE_OK;
}


ACPI_STATUS
AcpiOsSignalSemaphore (
        ACPI_SEMAPHORE                  handle,
        uint32_t                        units)
{
  for (; units; --units)
    if (sem_post(static_cast<sem_t*>(handle)) < 0)
      return AE_BAD_PARAMETER;

  return AE_OK;
}

ACPI_STATUS
AcpiOsCreateLock (ACPI_SPINLOCK *out_handle)
{
  pthread_mutex_t *m = new pthread_mutex_t(PTHREAD_MUTEX_INITIALIZER);
  *out_handle = (ACPI_SPINLOCK)m;
  return AE_OK;
}

void
AcpiOsDeleteLock (ACPI_SPINLOCK handle)
{
  delete static_cast<pthread_mutex_t*>(handle);
  return;
}

ACPI_CPU_FLAGS
AcpiOsAcquireLock (ACPI_SPINLOCK handle)
{
  pthread_mutex_lock(static_cast<pthread_mutex_t*>(handle));
  return AE_OK;
}

void
AcpiOsReleaseLock (
        ACPI_SPINLOCK                   handle,
        ACPI_CPU_FLAGS)
{
  pthread_mutex_unlock(static_cast<pthread_mutex_t *>(handle));
  return;
}

