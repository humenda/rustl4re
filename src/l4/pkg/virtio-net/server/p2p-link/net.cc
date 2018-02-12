/*
 * Copyright (C) 2012-2018 Kernkonzept GmbH.
 * Author(s): Alexander Warg <alexander.warg@kernkonzept.com>
 *            Andreas Wiese <andreas.wiese@kernkonzept.com>
 *
 * This file is distributed under the terms of the GNU General Public
 * License, version 2.  Please see the COPYING-GPL-2 file for details.
 */

#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/meta>
#include <l4/re/util/object_registry>
#include <l4/re/util/br_manager>

#include <l4/sys/factory>
#include <l4/sys/compiler.h>

#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/cxx/ipc_varg>

#include <l4/cxx/list>
#include <l4/cxx/utils>
#include <l4/cxx/unique_ptr>

#include <l4/l4virtio/server/virtio>
#include <l4/l4virtio/server/l4virtio>
#include <l4/l4virtio/l4virtio>

#include <l4/util/assert.h>

#include <cstring>
#include <climits>
#include <getopt.h>

#include <cstdio>
#include <pthread.h>
#include <debug.h>
#include <checksum.h>

using cxx::access_once;
using L4virtio::Svr::Data_buffer;
using L4virtio::Svr::Request_processor;

// Add some preleminary versions of consumed and finish to allow
// burst commit of buffers
struct Virtqueue : L4virtio::Svr::Virtqueue
{
  void consumed_x(l4_uint16_t n, Head_desc &r, l4_uint32_t len = 0)
  {
    l4_uint16_t i = (_used->idx + n) & _idx_mask;
    _used->ring[i] = Used_elem(r.desc() - _desc, len);
    r = Head_desc();
  }

  template<typename QUEUE_OBSERVER>
  void finish_x(l4_uint16_t n, QUEUE_OBSERVER *o)
  {
    L4virtio::wmb();
    _used->idx += n;
    o->notify_queue(this);
  }

};

//#define CONFIG_STATS 1
//#define CONFIG_BENCHMARK 1

enum
{
  Merge_rx_buffers = true,
  Csum_offload     = true,
  Full_segmentation_offload = false,
};

static struct option options[] =
{
    {"size", 1, 0, 's'},  // size of in/out queue == #buffers in queue
    {0, 0, 0, 0}
};


#ifdef CONFIG_STATS
static void *stats_thread_loop(void *);
#endif

class Virtio_net :
  public L4virtio::Svr::Device,
  public L4::Epiface_t<Virtio_net, L4virtio::Device>
{
public:
  struct Hdr_flags
  {
    l4_uint8_t raw;
    CXX_BITFIELD_MEMBER( 0, 0, need_csum, raw);
    CXX_BITFIELD_MEMBER( 1, 1, data_valid, raw);
  };

  struct Hdr
  {
    Hdr_flags flags;
    l4_uint8_t gso_type;
    l4_uint16_t hdr_len;
    l4_uint16_t gso_size;
    l4_uint16_t csum_start;
    l4_uint16_t csum_offset;
    l4_uint16_t num_buffers;
  };

  struct Features : L4virtio::Svr::Dev_config::Features
  {
    Features() = default;
    Features(l4_uint32_t raw) : L4virtio::Svr::Dev_config::Features(raw) {}

    CXX_BITFIELD_MEMBER( 0,  0, csum, raw);       // host handles partial csum
    CXX_BITFIELD_MEMBER( 1,  1, guest_csum, raw); // guest handles partial csum
    CXX_BITFIELD_MEMBER( 5,  5, mac, raw);        // host has given mac
    CXX_BITFIELD_MEMBER( 6,  6, gso, raw);        // host handles packets /w any GSO
    CXX_BITFIELD_MEMBER( 7,  7, guest_tso4, raw); // guest handles TSOv4 in
    CXX_BITFIELD_MEMBER( 8,  8, guest_tso6, raw); // guest handles TSOv6 in
    CXX_BITFIELD_MEMBER( 9,  9, guest_ecn, raw);  // guest handles TSO[6] with ECN in
    CXX_BITFIELD_MEMBER(10, 10, guest_ufo, raw);  // guest handles UFO in
    CXX_BITFIELD_MEMBER(11, 11, host_tso4, raw);  // host handles TSOv4 in
    CXX_BITFIELD_MEMBER(12, 12, host_tso6, raw);  // host handles TSOv6 in
    CXX_BITFIELD_MEMBER(13, 13, host_ecn, raw);   // host handles TSO[6] with ECN in
    CXX_BITFIELD_MEMBER(14, 14, host_ufo, raw);   // host handles UFO
    CXX_BITFIELD_MEMBER(15, 15, mrg_rxbuf, raw);  // host can merge receive buffers
    CXX_BITFIELD_MEMBER(16, 16, status, raw);     // virtio_net_config.status available
    CXX_BITFIELD_MEMBER(17, 17, ctrl_vq, raw);    // Control channel available
    CXX_BITFIELD_MEMBER(18, 18, ctrl_rx, raw);    // Control channel RX mode support
    CXX_BITFIELD_MEMBER(19, 19, ctrl_vlan, raw);  // Control channel VLAN filtering
    CXX_BITFIELD_MEMBER(20, 20, ctrl_rx_extra, raw); // Extra RX mode control support
    CXX_BITFIELD_MEMBER(21, 21, guest_announce, raw); // Guest can announce device on the network
    CXX_BITFIELD_MEMBER(22, 22, mq, raw);         // Device supports Receive Flow Steering
    CXX_BITFIELD_MEMBER(23, 23, ctrl_mac_addr, raw); // Set MAC address
  };

  enum
  {
    Rx = 0,
    Tx = 1,
  };

#ifdef CONFIG_STATS
  unsigned long num_tx;
  unsigned long num_rx;
  unsigned long num_dropped;
  unsigned long num_irqs;
#endif

  struct Net_config_space
  {
  };

  L4virtio::Svr::Dev_config_t<Net_config_space> _dev_config;

  explicit Virtio_net(unsigned vq_max)
  : L4virtio::Svr::Device(&_dev_config),
    _dev_config(0x44, L4VIRTIO_ID_NET, 2),
    _vq_max(vq_max), _enabled_features(0)
#ifdef CONFIG_STATS
    , num_tx(0), num_rx(0), num_dropped(0), num_irqs(0)
#endif
  {
    Features hf(0);
    hf.ring_indirect_desc() = true;

    hf.csum()       = Csum_offload;
    hf.guest_csum() = Csum_offload;
    hf.mrg_rxbuf()  = Merge_rx_buffers;

    if (Full_segmentation_offload)
      {
        hf.host_tso4() = true;
        hf.host_tso6() = true;
        hf.host_ufo()  = true;
        hf.host_ecn()  = true;

        hf.guest_tso4() = true;
        hf.guest_tso6() = true;
        hf.guest_ufo()  = true;
        hf.guest_ecn()  = true;
      }

    _dev_config.host_features(0) = hf.raw;
    _dev_config.host_features(1) = 1;
    _dev_config.reset_hdr();

    reset_queue_config(0, vq_max);
    reset_queue_config(1, vq_max);
  }

  void register_single_driver_irq()
  {
    kick_guest_irq = L4Re::Util::Unique_cap<L4::Irq>(
       L4Re::chkcap(server_iface()->template rcv_cap<L4::Irq>(0)));

    L4Re::chksys(server_iface()->realloc_rcv_cap(0));
  }

  Server_iface *server_iface() const
  { return L4::Epiface::server_iface(); }

  L4::Cap<L4::Irq> device_notify_irq() const
  { return _host_irq; }

  void reset()
  {
    for (Virtqueue &q: _q)
      q.disable();
  }

  bool available()
  { return !obj_cap(); }

  /**
   * Return the set of features negotiated between host and guest.
   *
   * \retval  The set of negotiated and hence enabled features.
   */
  const Features &enabled_features()
  { return _enabled_features; }

  template<typename T, unsigned N >
  static unsigned array_length(T (&)[N]) { return N; }

  int reconfig_queue(unsigned index)
  {
    if (index >= array_length(_q))
      return -L4_ERANGE;

    if (setup_queue(_q + index, index, _vq_max))
      return 0;

    return -L4_EINVAL;
  }

  bool check_queues()
  {
    for (Virtqueue &q: _q)
      if (!q.ready())
        {
          reset();
          printf("failed to start queues\n");
          return false;
        }

    l4_uint32_t guest_features = _dev_config.guest_features(0);
    l4_uint32_t features = guest_features & _dev_config.host_features(0);

    if (L4_UNLIKELY(guest_features != features))
      {
        Err().printf("error: guest enabled features we did not offer: %x\n",
                     ~features & guest_features);
        return false;
      }

    _enabled_features.raw = features;

    return true;
  }

  Virtqueue *tx_q() { return &_q[Tx]; }
  Virtqueue *rx_q() { return &_q[Rx]; }


  void notify_queue(L4virtio::Virtqueue *queue)
  {
    //printf("%s\n", __func__);
    if (queue->no_notify_guest())
      return;

    // we do not care about this anywhere, so skip
    // _device_config->irq_status |= 1;

    kick_guest_irq->trigger();
#ifdef CONFIG_STATS
    ++num_irqs;
#endif
  }

  char const *name;

  template<typename REG>
  void register_client(REG *registry, L4::Cap<L4::Irq> host_irq,
                       unsigned num_ds)
  {
    init_mem_info(num_ds);
    _host_irq = host_irq;
    L4Re::chkcap(registry->register_obj(this));
    obj_cap()->dec_refcnt(1);
  }

  template<typename REG>
  void unregister_client(REG *registry)
  {
    reset();
    reset_queue_config(0, _vq_max);
    reset_queue_config(1, _vq_max);
    init_mem_info(0);

    registry->unregister_obj(this);
  }

private:
  friend void *stats_thread_loop(void *);

  unsigned _vq_max;
  Virtqueue _q[2];
  L4Re::Util::Unique_cap<L4::Irq> kick_guest_irq;
  L4::Cap<L4::Irq> _host_irq;
  Features _enabled_features;
};

static L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> server;

class Sock_pair : public L4::Epiface_t<Sock_pair, L4::Factory>
{
private:
  struct Host_irq : public L4::Irqep_t<Host_irq>
  {
    explicit Host_irq(Sock_pair *sp) : s(sp) {}
    Sock_pair *s;
    void handle_irq()
    { s->kick(); }
  };

  struct Del_cap_irq : public L4::Irqep_t<Del_cap_irq>
  {
    Del_cap_irq(Virtio_net **clients, unsigned num)
    : _clients(clients),
      _num_clients(num)
    {}

    void handle_irq()
    {
      for (unsigned i = 0; i < _num_clients; ++i)
        {
          if (!_clients[i]->available()
              && !_clients[i]->obj_cap().validate().label())
            {
              printf("Client on port %u has gone. Unregistering.\n", i);
              _clients[i]->unregister_client(server.registry());
            }
        }
    }

    Virtio_net **_clients;
    unsigned _num_clients;
  };

  Host_irq _host_irq;
  Del_cap_irq _del_cap_irq;

public:
  L4::Cap<L4::Irq> host_irq() const
  { return L4::cap_cast<L4::Irq>(_host_irq.obj_cap()); }

  L4::Epiface *irq_object()
  { return &_host_irq; }

  struct Buffer : Data_buffer
  {
    Buffer() = default;
    Buffer(L4virtio::Svr::Driver_mem_region const *r,
           Virtqueue::Desc const &d,
           Request_processor const *)
    {
      pos = static_cast<char *>(r->local(d.addr));
      left = d.len;
    }
  };

  struct End_point : Request_processor
  {
    Virtqueue::Head_desc head;
    Buffer pkt;
    Virtio_net::Hdr *hdr;

    /// Pointer to the device the end point belongs to
    Virtio_net *dev;
    Virtqueue *q;

    enum { merge_rx = Merge_rx_buffers };
    enum { hdr_size = Merge_rx_buffers ? 12 : 10 };

    End_point(Virtio_net *device, Virtqueue *queue) : dev(device), q(queue) {}

    void finish(l4_uint32_t total = 0)
    { q->finish(head, dev, total); }

    bool next()
    { return Request_processor::next(dev->mem_info(), &pkt); }

    bool start_packet(bool read, l4_uint32_t *total = 0, bool merge = false)
    {
      auto r = q->next_avail();

      if (L4_UNLIKELY(!r))
        return false;

      head = start(dev->mem_info(), r, &pkt);
      if (0)
        printf("XX(%s): start new packet: %p (%u)\n",
               read ? "tx" : "rx", pkt.pos, pkt.left);

      if (!merge)
        {
          hdr = (Virtio_net::Hdr *)pkt.pos;

          if (!read)
            *total = 0;

          if (L4_UNLIKELY(pkt.left < hdr_size))
            {
              if (1)
                printf("XX(%s):  abort packet (too small for header)\n",
                       read ? "tx" : "rx");
              finish();
              return false;
            }

          if (pkt.done())
            return L4_LIKELY(next());
        }
      else if (!read)
        {
          if (0)
            printf("XX(rx):  start new rx fragment: %p (%u)\n",
                   pkt.pos, pkt.left);
          *total = 0;
        }

      return true;
    }

  };

  /**
   * An object that does check summing on packets sent between two endpoints.
   *
   * This is the object that encapsulates the calculation of checksums, should
   * this be required. This is the case if the two peers on the link negotiated
   * different checksum offloading capabilities, i.e. one announced support, the
   * other one didn't. In this case the capable peer may send partially
   * checksummed packets which the uncapable peer would drop if simply passed
   * through.
   *
   * This object tracks the state of the checksum calculation, like current
   * offset in the packet in the currently processed descriptor (might be a
   * descriptor chain) and the position in the receive buffer where to
   * eventually store the calculated checksum.
   */
  class Checksum_computer
  {
  private:
    bool _noop; // packet has checksum or peer supports offloading: do nothing
    Net_checksum _csum; // actual checksum

    l4_uint32_t _pos; // current position in the tx packet
    l4_uint32_t _csum_start; // copy of csum_start virtio-net header field
    l4_uint32_t _csum_offset; // copy of csum_offset virtio-net header field

    l4_uint8_t *_rxptr; // pointer to checksum field in rx buffer

    End_point *_tx, *_rx; // the endpoints we are checksumming for

    /**
     * Reset the checksum computer for calculating a new package.
     *
     * This routine gets called if a descriptor containing a virtio-net header
     * is seen, hence a new packet is being processed. Its purpose is resetting
     * the object to cleanly start over.
     */
    void reset()
    {
      _noop = _rx->dev->enabled_features().guest_csum() ||
              !_tx->hdr->flags.need_csum();

      if (_noop)
        return;

      _csum.reset();
      _rxptr = nullptr;
      _pos = 0;

      // Whether those values are out-of-bounds will be detected by finish(),
      // as we will never have found the right spot for writing the checksum
      // while copying/checksumming in this case. Checking them directly here,
      // is not possible without copying the packet first, as a rogue guest
      // might meddle with the descriptor chain between getting its length and
      // actual copying.
      _csum_start = _tx->hdr->csum_start;
      _csum_offset = _tx->hdr->csum_offset;
    }

  public:
    /**
     * Initialize a Checksum_computer for two endpoints.
     *
     * \param tx  The transmitting endpoint.
     * \param rx  The receiving endpoint.
     */
    Checksum_computer(End_point *tx, End_point *rx)
    : _noop(true), _pos(0), _csum_start(0), _csum_offset(0),
      _rxptr(nullptr), _tx(tx), _rx(rx)
    {}

    /**
     * Trigger an update of the checksum calculation.
     *
     * Upon calling this function, the contents of the endpoints' current
     * buffers is examined and added to the checksum.
     */
    void update()
    {
      l4_uint8_t *tx = reinterpret_cast<l4_uint8_t *>(_tx->pkt.pos);
      l4_uint8_t *rx = reinterpret_cast<l4_uint8_t *>(_rx->pkt.pos);
      size_t len = cxx::min(_tx->pkt.left, _rx->pkt.left);

      // The tx queue may contain more than one request in which case we have to
      // detect the beginning of a new descriptor-chain and reset our checksum
      // calculator accordingly. A new request starts with a virtio-net header,
      // which will be reflected in the End_point's member variables:
      if (reinterpret_cast<l4_uint8_t *>(_tx->hdr) == tx) {
        // skip the virtio-net header
        tx  += sizeof(Virtio_net::Hdr);
        rx  += sizeof(Virtio_net::Hdr);
        len -= sizeof(Virtio_net::Hdr);
        reset();
      }

      if (_noop)
        return;

      if (_pos >= _csum_start)
        // whole chunk behind _csum_start: checksum all
        _csum.add(tx, len);
      else if (_pos <= _csum_start && _pos + len > _csum_start)
        // part of chunk behind _csum_start: checksum part
        _csum.add(tx  + _csum_start - _pos, len - _csum_start - _pos);

      // no overflow check on _csum_start + _csum_offset, as these are
      // l4_uint32_t but get their values from l4_uint16_t header fields.
      if (_rxptr == nullptr
          && (_csum_start + _csum_offset)      >= _pos
          && (_csum_start + _csum_offset + 1U) < (_pos + len))
        {
          // save pointer to checksum field in rx buffer

          // In theory, there's uintptr_t and UINTPTR_MAX for this kind of
          // stuff, but unfortunately this is purely optional and we'll have to
          // stick with unsigned long
          if ((unsigned long)rx > 0
              && (_csum_start + _csum_offset) > (ULONG_MAX - (unsigned long)rx))
            Err().printf("error: invalid csum_{start,offset}: would overflow");
          else
            _rxptr = rx + _csum_start + _csum_offset - _pos;
        }

      _pos += len;
    }

    /**
     * Finalise the checksum and write it to the receive buffer.
     *
     * \retval true   NEED_CSUM wasn't set or we successfully wrote the
     *                checksum.
     * \retval false  NEED_CSUM was set but we didn't know where to write it.
     *
     * This function writes the calculated checksum to the correct location in
     * the receive buffer. This location is determined by the `update()`
     * function while processing the packet.
     *
     * After this function returns, the receive buffer should contain a fully
     * checksummed packet and its virtio-net header should be modified to
     * reflect this fact.
     */
    bool finish()
    {
      if (_noop)
        return true;

      if (_rxptr == nullptr)
        return false;

      l4_uint16_t csum = _csum.finalize();
      _rxptr[0] = (csum >> 8) & 0xff;
      _rxptr[1] = csum & 0xff;

      _rx->hdr->flags.need_csum() = 0;
      _rx->hdr->flags.data_valid() = 0;
      // spec says if need_csum==0, driver/device MUST NOT use
      // csum_start/csum_offset, so obviously they may be garbage: keep
      // untouched.
      //_rx->hdr->csum_start = 0;
      //_rx->hdr->csum_offset = 0;

      return true;
    }
  };

  struct Pipe
  {
    l4_uint32_t total;
    End_point tx;
    End_point rx;

    Pipe(Virtio_net *tx_port, Virtio_net *rx_port)
    : tx(tx_port, tx_port->tx_q()),
      rx(rx_port, rx_port->rx_q())
    {}

    bool ready() const
    { return L4_LIKELY(tx.q->ready() && rx.q->ready()); }

    bool work_pending() const
    {
      return L4_LIKELY(ready()) && tx.q->desc_avail()
             && rx.q->desc_avail();
    }

    void disable_notify()
    {
      if (L4_UNLIKELY(!ready()))
        return;

      tx.q->disable_notify();
      rx.q->disable_notify();
    }

    void enable_notify()
    {
      if (L4_UNLIKELY(!ready()))
        return;

      tx.q->enable_notify();
      rx.q->enable_notify();
    }

    bool start_tx_packet()
    {
      return tx.start_packet(true);
    }

    bool start_rx_packet()
    {
      return rx.start_packet(false, &total, nmerge);
    }

    unsigned nmerge;

    bool copy()
    {
      try
        {
          // loop over all chained descriptors (rx and tx)
          if (!tx.head)
            {
              nmerge = 0;
              if (L4_UNLIKELY(!start_tx_packet()))
                return false;
            }

          if (!rx.head && L4_UNLIKELY(!start_rx_packet()))
            return false;

          Checksum_computer csum(&tx, &rx);

          for (;;)
            {
              if (0)
                printf("%p: copy packet %p (%u) -> %p (%u)\n", this,
                       tx.pkt.pos, tx.pkt.left, rx.pkt.pos, rx.pkt.left);

              csum.update();

              total += tx.pkt.copy_to(&rx.pkt);

              if (tx.pkt.done() && !tx.next())
                {
                  if (0)
                    printf("%p: finish packet rx buffers: %u last total %u\n",
                           this, nmerge + 1, total);
                  tx.finish();

                  if (!csum.finish())
                    {
                      printf("%p: bogus csum_start/csum_offset, drop packet\n",
                             this);
                      // XXX: Yeah, but how? finish_x() without consumed_x()?
                    }

                  rx.q->consumed_x(nmerge++, rx.head, total);
                  if (rx.merge_rx)
                    rx.hdr->num_buffers = nmerge;

                  rx.q->finish_x(nmerge, rx.dev);

                  nmerge = 0;
                  if (L4_UNLIKELY(!start_tx_packet()))
                    return false;

                  if (L4_UNLIKELY(!start_rx_packet()))
                    return false;

                  continue;
                }

              if (rx.pkt.done() && !rx.next())
                {
                  if (rx.merge_rx)
                    rx.q->consumed_x(nmerge++, rx.head, total);
                  else
                    {
                      printf("%p: truncated rx packet, drop\n", this);
                      rx.hdr->flags.raw = 0;
                      rx.q->finish_x(nmerge, rx.dev);
                    }

                  if (0)
                    printf("%p: next rx buffer: buffers: %u total %u\n",
                           this, nmerge, total);

                  if (L4_UNLIKELY(!start_rx_packet()))
                    return false;

                  continue;
                }
            }
        }
      catch (L4virtio::Svr::Bad_descriptor const &e)
        {
          if (e.proc == &tx)
            {
              // failed TX queue, be nice to RX part.
              tx.dev->device_error();

              if (rx.q->ready() && rx.head)
                rx.finish(total);

              printf("error: TX queue error: bad descriptor: %d in device %p, queue %p\n",
                     e.error, tx.dev, tx.q);
            }

          if (e.proc == &rx)
            {
              // failed RX queue, send half pkt to TX part.
              rx.dev->device_error();

              if (tx.q->ready() && tx.head)
                tx.finish();

              printf("error: RX queue error: bad descriptor: %d in device %p, queue %p\n",
                     e.error, rx.dev, rx.q);
            }

          return false;
        }
    }
  };

  enum { Npipes = 2, Nports = 2 };
  Virtio_net *port[Nports];
  Pipe *pipe[Npipes];

  /**
   * \brief Create a new virtio Switch
   */
  Sock_pair(unsigned vq_max)
  : _host_irq(this),
    _del_cap_irq(port, Nports)
  {
    for (Virtio_net *&p: port)
      p = new Virtio_net(vq_max);

    pipe[0] = new Pipe(port[0], port[1]);
    pipe[1] = new Pipe(port[1], port[0]);

    auto c = L4Re::chkcap(server.registry()->register_irq_obj(&_del_cap_irq));
    L4Re::chksys(L4Re::Env::env()->main_thread()->register_del_irq(c));
  }

  long op_create(L4::Factory::Rights, L4::Ipc::Cap<void> &res,
                 l4_umword_t type, L4::Ipc::Varg_list_ref va)
  {
    // test for supported object types
    if (type != 0)
      return -L4_EINVAL;

    L4::Ipc::Varg opt = va.next();
    if (!opt.is_of_int())
      return -L4_EINVAL;

    unsigned num_ds = opt.value<l4_mword_t>();
    if (num_ds == 0 || num_ds > 80)
      {
        printf("warning: client requested invalid number of data spaces: 0 < %u <= 80\n", num_ds);
        return -L4_EINVAL;
      }

    for (auto *p: port)
      {
        if (p->available())
          {
            p->register_client(server.registry(), host_irq(), num_ds);
            res = L4::Ipc::make_cap(p->obj_cap(), L4_CAP_FPAGE_RWSD);

            return L4_EOK;
          }
      }

    return -L4_ENOMEM;
  }

  void kick()
  {
    for (;;)
      {
        for (auto p: pipe)
          p->disable_notify();

        for (bool more = true; more; )
          {
            more = false;
            for (auto p: pipe)
              if (L4_LIKELY(p->ready()))
                more |= p->copy();
          }

        for (auto p: pipe)
          p->enable_notify();

        L4virtio::wmb();
        L4virtio::rmb();

        bool work = false;
        for (auto p: pipe)
          if (L4_UNLIKELY((work |= p->work_pending())))
            break;

        if (L4_LIKELY(!work))
          break;

        // seems there is already new work to do ...
      }
  }
};

#ifdef CONFIG_STATS
static void *stats_thread_loop(void *)
{
  for (;;)
    {
      sleep(1);
      for (unsigned i = 0; i < net.Num_ports; ++i)
        {
          Virtio_net *p = &net.port[i];
          printf("%s: tx:%ld rx:%ld drp:%ld irqs:%ld ri:%d:%d  ",
                 p->name, p->num_tx, p->num_rx, p->num_dropped, p->num_irqs,
                 p->_device_config.status().running() ? (unsigned)p->_q[0].avail->idx : -1,
                 (unsigned)p->_q[0].current_avail);
        }
      printf("\n");
    }
  return NULL;
};
#endif

int main(int argc, char *argv[])
{
  Dbg info;
  Dbg warn(Dbg::Warn);

  Dbg::set_level(0xf);

  int opt, index;
  unsigned vq_max_num = 0x100; // default value for data queues

  printf("Hello from l4vio_net_p2p\n");

  while( (opt = getopt_long(argc, argv, "s:", options, &index)) != -1)
    {
      switch (opt)
        {
        case 's':
          vq_max_num = atoi(optarg);
          printf("Max number of buffers in virtqueue: %u\n", vq_max_num);
          break;
        }
    }

  Sock_pair *s = new Sock_pair(vq_max_num);
  L4::Cap<void> cap = server.registry()->register_obj(s, "svr");
  server.registry()->register_irq_obj(s->irq_object());
  if (!cap.is_valid())
    printf("error registering switch\n");

#ifdef CONFIG_STATS
  pthread_t stats_thread;
  pthread_create(&stats_thread, NULL, stats_thread_loop, NULL);
#endif

  server.loop();
  return 0;
}



