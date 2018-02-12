#define LWIP_PREFIX_BYTEORDER_FUNCS 1

#include <l4/l4re_vfs/vfs.h>
#include <l4/l4re_vfs/backend>
#include <l4/cxx/ref_ptr>
#include <errno.h>
#include <cstring>

#include "lwip/opt.h"
#include "lwip/ip_addr.h"
#include "lwip/api.h"
#include "lwip/sys.h"
#include "lwip/igmp.h"
#include "lwip/tcpip.h"
#include "lwip/pbuf.h"

#include <mutex>
#include <sys/socket.h>
#include <netinet/in.h>

namespace {

/** A struct sockaddr replacement that has the same alignment as sockaddr_in/
 *  sockaddr_in6 if instantiated.
 */
union sockaddr_aligned
{
   sockaddr sa;
#if LWIP_IPV6
   sockaddr_in6 sin6;
#endif /* LWIP_IPV6 */
   sockaddr_in sin;
};

/** Table to quickly map an lwIP error (err_t) to a socket error
  * by using -err as an index */
static const int err_to_errno_table[] = {
  0,             /* ERR_OK          0      No error, everything OK. */
  ENOMEM,        /* ERR_MEM        -1      Out of memory error.     */
  ENOBUFS,       /* ERR_BUF        -2      Buffer error.            */
  EWOULDBLOCK,   /* ERR_TIMEOUT    -3      Timeout                  */
  EHOSTUNREACH,  /* ERR_RTE        -4      Routing problem.         */
  EINPROGRESS,   /* ERR_INPROGRESS -5      Operation in progress    */
  EINVAL,        /* ERR_VAL        -6      Illegal value.           */
  EWOULDBLOCK,   /* ERR_WOULDBLOCK -7      Operation would block.   */
  EADDRINUSE,    /* ERR_USE        -8      Address in use.          */
  EALREADY,      /* ERR_ISCONN     -9      Already connected.       */
  ECONNABORTED,  /* ERR_ABRT       -10     Connection aborted.      */
  ECONNRESET,    /* ERR_RST        -11     Connection reset.        */
  ENOTCONN,      /* ERR_CLSD       -12     Connection closed.       */
  ENOTCONN,      /* ERR_CONN       -13     Not connected.           */
  EIO,           /* ERR_ARG        -14     Illegal argument.        */
  -1,            /* ERR_IF         -15     Low-level netif error    */
};

inline int err_to_errno(int err)
{
  if (-err < (int)(sizeof(err_to_errno_table)/sizeof(err_to_errno_table[0])))
    return err_to_errno_table[-err];
  return EIO;
}

class Socket_file : public L4Re::Vfs::Be_file
{
private:
  netconn *_conn;
  std::mutex _lock;

  int _rcvevent;
  unsigned _sendevent;
  unsigned _errevent;

  void *_lastdata;
  unsigned _lastoffset;

  static void event_callback(netconn *conn, netconn_evt evt, u16_t len) throw();
public:
  Socket_file(netconn *conn = 0) throw()
  : _conn(conn), _rcvevent(0), _sendevent(0), _errevent(0),
    _lastdata(NULL), _lastoffset(0)
  {}

  ~Socket_file() throw()
  {
    if (_conn)
      netconn_delete(_conn);
  }

  static cxx::Ref_ptr<Socket_file> socket(int domain, int type, int protocol) throw();
  int bind(sockaddr const *, socklen_t) throw();
  int connect(sockaddr const *, socklen_t) throw();
  ssize_t send(void const *, size_t, int) throw();
  ssize_t recv(void *, size_t, int) throw();
  ssize_t sendto(void const *, size_t, int, sockaddr const *, socklen_t) throw();
  ssize_t recvfrom(void *, size_t, int, sockaddr *, socklen_t *) throw();
#if 0
  ssize_t sendmsg(msghdr const *, int) throw();
  ssize_t recvmsg(msghdr *, int) throw();
  int getsockopt(int level, int opt, void *, socklen_t *) throw();
  int setsockopt(int level, int opt, void const *, socklen_t) throw();
#endif
  int listen(int) throw();
  int accept(sockaddr *addr, socklen_t *) throw();
#if 0
  int shutdown(int) throw();

  int getsockname(sockaddr *, socklen_t *) throw();
  int getpeername(sockaddr *, socklen_t *) throw();
#endif
  ssize_t readv(const struct iovec *vec, int iovcnt) throw();
  ssize_t writev(const struct iovec *vec, int iovcnt) throw();

private:
  bool match_connection_type(sockaddr const *addr)
  {
    if (NETCONNTYPE_ISIPV6(_conn->type) && addr->sa_family == AF_INET6)
      return true;
    if (! NETCONNTYPE_ISIPV6(_conn->type) && addr->sa_family == AF_INET)
      return true;
    return false;
  }
};

inline netconn_type domain_to_netconn_type(int domain, int type)
{
  if (domain == AF_INET)
    return static_cast<netconn_type>(type);
  else
    return static_cast<netconn_type>(type | NETCONN_TYPE_IPV6);
}

inline socklen_t
ip4_addr_port_to_sockaddr(ip_addr_t const &addr, uint16_t port, sockaddr_in *saddr)
{
  saddr->sin_family = AF_INET;
  saddr->sin_port = htons(port);
  saddr->sin_addr.s_addr = addr.u_addr.ip4.addr;
  //memset(saddr->sin_zero, 0, SIN_ZERO_LEN);
  return sizeof(sockaddr_in);
}

inline socklen_t
ip6_addr_port_to_sockaddr(ip_addr_t const &addr, uint16_t port, sockaddr_in6 *saddr)
{
  saddr->sin6_family = AF_INET6;
  saddr->sin6_port = htons((port));
  saddr->sin6_flowinfo = 0;
  memcpy(&saddr->sin6_addr, &addr.u_addr.ip6, sizeof(saddr->sin6_addr));
  return sizeof(sockaddr_in6);
}

inline socklen_t
ip_addr_port_to_sockaddr(bool ipv6, ip_addr_t const &addr, uint16_t port, sockaddr *saddr)
{
  if (ipv6)
    return ip6_addr_port_to_sockaddr(addr, port, reinterpret_cast<sockaddr_in6 *>(saddr));
  else
    return ip4_addr_port_to_sockaddr(addr, port, reinterpret_cast<sockaddr_in *>(saddr));
}

inline void
sockaddr_to_ip4_addr_port(ip_addr_t *addr, uint16_t *port, sockaddr_in const *saddr)
{
  *port = ntohs(saddr->sin_port);
  addr->u_addr.ip4.addr = saddr->sin_addr.s_addr;
  //memset(saddr->sin_zero, 0, SIN_ZERO_LEN);
}

inline void
sockaddr_to_ip6_addr_port(ip_addr_t *addr, uint16_t *port, sockaddr_in6 const *saddr)
{
  *port = ntohs(saddr->sin6_port);
  memcpy(&addr->u_addr.ip6, &saddr->sin6_addr, sizeof(saddr->sin6_addr));
}

inline void
sockaddr_to_ip_addr_port(ip_addr_t *addr, uint16_t *port, sockaddr const *saddr)
{
  if (saddr->sa_family == AF_INET6)
    sockaddr_to_ip6_addr_port(addr, port, reinterpret_cast<sockaddr_in6 const *>(saddr));
  else
    sockaddr_to_ip4_addr_port(addr, port, reinterpret_cast<sockaddr_in const *>(saddr));
}

inline bool
sockaddr_len_valid(socklen_t len)
{
  switch (len)
    {
    case sizeof(sockaddr_in):
    case sizeof(sockaddr_in6): return true;
    default:                   return false;
    }
}

inline bool
sockaddr_type_valid(sockaddr const *addr)
{
  switch (addr->sa_family)
    {
    case AF_INET:
    case AF_INET6: return true;
    default:       return false;
    }
}

inline bool
sockaddr_aligned(sockaddr const *addr)
{ return ((unsigned long) addr & 3) == 0; }


static std::mutex conn_lock;

/**
 * Callback registered in the netconn layer for each socket-netconn.
 * Processes recvevent (data available) and wakes up tasks waiting for select.
 */
void
Socket_file::event_callback(netconn *conn, netconn_evt evt, u16_t len) throw()
{
  (void)len;
  using cxx::Ref_ptr;

  Socket_file *sock;

  /* Get socket */
  if (!conn)
    return;

  if (!conn->priv)
    {
      std::unique_lock<std::mutex> guard(conn_lock);
      if (!conn->priv)
        {
          Socket_file *sock = new Socket_file(conn);
          conn->priv = sock;
        }
    }

  sock = static_cast<Socket_file*>(conn->priv);
  if (!sock)
    return;


  std::unique_lock<decltype(sock->_lock)> guard(sock->_lock);

  /* Set event as required */
  switch (evt) {
    case NETCONN_EVT_RCVPLUS:
      ++sock->_rcvevent;
      break;
    case NETCONN_EVT_RCVMINUS:
      --sock->_rcvevent;
      break;
    case NETCONN_EVT_SENDPLUS:
      sock->_sendevent = 1;
      break;
    case NETCONN_EVT_SENDMINUS:
      sock->_sendevent = 0;
      break;
    case NETCONN_EVT_ERROR:
      sock->_errevent = 1;
      break;
    default:
      LWIP_ASSERT("unknown event", 0);
      break;
  }
}


cxx::Ref_ptr<Socket_file>
Socket_file::socket(int domain, int type, int protocol) throw()
{
  using cxx::Ref_ptr;

  netconn *conn = 0;
  /* create a netconn */
  switch (type)
    {
    case SOCK_RAW:
      conn = netconn_new_with_proto_and_callback(domain_to_netconn_type(domain, NETCONN_RAW),
          (u8_t)protocol, event_callback);
      break;
    case SOCK_DGRAM:
      conn = netconn_new_with_callback(domain_to_netconn_type(domain, NETCONN_UDP),
          event_callback);
      break;
    case SOCK_STREAM:
      conn = netconn_new_with_callback(domain_to_netconn_type(domain, NETCONN_TCP), event_callback);
      if (conn != NULL)
          /* Prevent automatic window updates, we do this on our own! */
          netconn_set_noautorecved(conn, 1);
      break;
    default:
      errno = EINVAL;
      return Ref_ptr<>::Nil;
    }

  if (!conn)
    {
      errno = ENOBUFS;
      return Ref_ptr<>::Nil;
    }

  Ref_ptr<Socket_file> s(new Socket_file(conn));
  if (!s)
    {
      netconn_delete(conn);
      errno = ENOBUFS;
      return Ref_ptr<>::Nil;
    }

  return s;
}

int
Socket_file::accept(sockaddr *addr, socklen_t *len) throw()
{
  if (netconn_is_nonblocking(_conn) && (_rcvevent <= 0))
    return -EWOULDBLOCK;

  netconn *newconn;
  err_t err = netconn_accept(_conn, &newconn);
  if (err != ERR_OK)
    {
      if (NETCONNTYPE_GROUP(netconn_type(_conn)) != NETCONN_TCP)
        return -EOPNOTSUPP;

      return -err_to_errno(err);
    }

  cxx::Ref_ptr<Socket_file> newsock;
  if (!newconn->priv)
    {
      std::unique_lock<std::mutex> guard(conn_lock);
      if (!newconn->priv)
        {
          Socket_file *sock = new Socket_file(newconn);
          newconn->priv = sock;
        }
    }

  newsock = cxx::ref_ptr(static_cast<Socket_file*>(newconn->priv));


  /* Prevent automatic window updates, we do this on our own! */
  netconn_set_noautorecved(newconn, 1);

  /* Note that POSIX only requires us to check addr is non-NULL. addrlen must
   * not be NULL if addr is valid.
   */
  if (addr)
    {
      sockaddr tempaddr;
      ip_addr_t naddr;
      u16_t port;
     /* get the IP address and port of the remote host */
      err = netconn_peer(newconn, &naddr, &port);
      if (err != ERR_OK)
        return -err_to_errno(err);

      socklen_t l = ip_addr_port_to_sockaddr(NETCONNTYPE_ISIPV6(newconn->type), naddr, port, &tempaddr);
      memcpy(addr, &tempaddr, l);
      *len = l;
    }

  int fd = L4Re::Vfs::vfs_ops->alloc_fd(newsock);
  if (fd < 0)
    return -ENFILE;

  return fd;
}

int
Socket_file::bind(sockaddr const *addr, socklen_t len) throw()
{
  if (!match_connection_type(addr))
    return -EINVAL;

  if (   !sockaddr_len_valid(len) || !sockaddr_type_valid(addr)
      || !sockaddr_aligned(addr))
    return -EIO;

  ip_addr_t local_addr;
  u16_t local_port;

  sockaddr_to_ip_addr_port(&local_addr, &local_port, addr);

  err_t err = netconn_bind(_conn, &local_addr, local_port);
  if (err != ERR_OK)
    return -err_to_errno(err);

  return 0;
}

int
Socket_file::connect(sockaddr const *addr, socklen_t len) throw()
{
  if (!(addr->sa_family == AF_UNSPEC) && !match_connection_type(addr))
    return -EINVAL;

  if (!sockaddr_len_valid(len) || !sockaddr_aligned(addr))
    return -EIO;

  err_t err;

  if (addr->sa_family == AF_UNSPEC)
    err = netconn_disconnect(_conn);
  else
    {
      ip_addr_t remote_addr;
      u16_t remote_port;
      sockaddr_to_ip_addr_port(&remote_addr, &remote_port, addr);
      err = netconn_connect(_conn, &remote_addr, remote_port);
    }

  if (err != ERR_OK)
    return -err_to_errno(err);

  return 0;
}

int
Socket_file::listen(int backlog) throw()
{
  /* limit the "backlog" parameter to fit in an u8_t */
  backlog = std::min(std::max(backlog, 0), 0xff);

  err_t err = netconn_listen_with_backlog(_conn, backlog);

  if (err == ERR_OK)
    return 0;

  if (NETCONNTYPE_GROUP(netconn_type(_conn)) != NETCONN_TCP)
    return -EOPNOTSUPP;

  return -err_to_errno(err);
}

ssize_t
Socket_file::recvfrom(void *_buf, size_t len, int flags, sockaddr *from, socklen_t *fromlen) throw()
{
  void *buf = NULL;
  pbuf *p;
  unsigned buflen, copylen;
  int off = 0;
  bool done = false;
  err_t err;

  bool is_tcp = NETCONNTYPE_GROUP(netconn_type(_conn)) == NETCONN_TCP;

  do
    {
      /* Check if there is data left from the last recv operation. */
      if (_lastdata)
        buf = _lastdata;
      else
        {
          /* If this is non-blocking call, then check first */
          if (   ((flags & MSG_DONTWAIT) || netconn_is_nonblocking(_conn))
              && (_rcvevent <= 0))
            {
              if (off > 0)
                {
                  /* update receive window */
                  netconn_recved(_conn, (u32_t)off);
                  return off;
                }
              return -EWOULDBLOCK;
            }

          /* No data was left from the previous operation, so we try to get
             some from the network. */
          if (is_tcp)
            err = netconn_recv_tcp_pbuf(_conn, (pbuf **)&buf);
          else
            err = netconn_recv(_conn, (netbuf **)&buf);

          if (err != ERR_OK)
            {
              if (off > 0)
                {
                  /* update receive window */
                  netconn_recved(_conn, (u32_t)off);
                  return off;
                }
              if (err == ERR_CLSD)
                return 0;
              else
                return -err_to_errno(err);
            }

          _lastdata = buf;
        }

    if (is_tcp)
      p = (struct pbuf *)buf;
    else
      p = ((struct netbuf *)buf)->p;

    buflen = p->tot_len;
    buflen -= _lastoffset;

    if (len > buflen)
      copylen = buflen;
    else
      copylen = (u16_t)len;

    /* copy the contents of the received buffer into
    the supplied memory pointer mem */
    pbuf_copy_partial(p, (u8_t*)_buf + off, copylen, _lastoffset);

    off += copylen;

    if (is_tcp)
      {
        len -= copylen;
        if (   (len <= 0)
            || (p->flags & PBUF_FLAG_PUSH)
            || (_rcvevent <= 0)
            || ((flags & MSG_PEEK)!=0))
          done = true;
      }
    else
      done = true;

    /* Check to see from where the data was.*/
    if (done && from && fromlen)
      {
        u16_t port;
        ip_addr_t tmpaddr;
        ip_addr_t *fromaddr;
        sockaddr saddr;
        if (is_tcp)
          {
            fromaddr = &tmpaddr;
            /* @todo: this does not work for IPv6, yet */
            netconn_getaddr(_conn, fromaddr, &port, 0);
          }
        else
          {
            port = netbuf_fromport((struct netbuf *)buf);
            fromaddr = netbuf_fromaddr((struct netbuf *)buf);
          }

        socklen_t len = ip_addr_port_to_sockaddr(NETCONNTYPE_ISIPV6(netconn_type(_conn)), tmpaddr, port, &saddr);
        if (*fromlen > len)
          *fromlen = len;

        memcpy(from, &saddr, *fromlen);
      }

    /* If we don't peek the incoming message... */
    if ((flags & MSG_PEEK) == 0)
      {
        /* If this is a TCP socket, check if there is data left in the
           buffer. If so, it should be saved in the sock structure for next
           time around. */
        if (is_tcp && (buflen - copylen > 0))
          {
            _lastdata = buf;
            _lastoffset += copylen;
          }
        else
          {
            _lastdata = NULL;
            _lastoffset = 0;
            if (is_tcp)
              pbuf_free((struct pbuf *)buf);
            else
              netbuf_delete((struct netbuf *)buf);
          }
      }
    }
  while (!done);

  if ((off > 0) && is_tcp)
    netconn_recved(_conn, (u32_t)off);

  return off;
}

ssize_t
Socket_file::sendto(void const *data, size_t size, int flags, sockaddr const *to, socklen_t tolen) throw()
{
  (void)tolen;
  err_t err;
  u16_t short_size;
  u16_t remote_port;
#if !LWIP_TCPIP_CORE_LOCKING
  struct netbuf buf;
#endif

  if (NETCONNTYPE_GROUP(netconn_type(_conn)) == NETCONN_TCP)
    return send(data, size, flags);

  if ((to != NULL) && !match_connection_type(to))
    return -EINVAL;

  /* @todo: split into multiple sendto's? */
  short_size = (u16_t)size;

  /* initialize a buffer */
  buf.p = buf.ptr = NULL;
  if (to)
    sockaddr_to_ip_addr_port(&buf.addr, &remote_port, to);
  else
    {
      remote_port = 0;
      ip_addr_set_any(NETCONNTYPE_ISIPV6(netconn_type(_conn)), &buf.addr);
    }
  netbuf_fromport(&buf) = remote_port;

  /* make the buffer point to the data that should be sent */
#if LWIP_NETIF_TX_SINGLE_PBUF
  /* Allocate a new netbuf and copy the data into it. */
  if (netbuf_alloc(&buf, short_size) == NULL)
    err = ERR_MEM;
  else
    err = netbuf_take(&buf, data, short_size);

#else /* LWIP_NETIF_TX_SINGLE_PBUF */
  err = netbuf_ref(&buf, data, short_size);
#endif /* LWIP_NETIF_TX_SINGLE_PBUF */
  if (err == ERR_OK)
    /* send the data */
    err = netconn_send(_conn, &buf);

  /* deallocated the buffer */
  netbuf_free(&buf);

  return (err == ERR_OK ? short_size : -err_to_errno(err));
}

ssize_t
Socket_file::send(void const *data, size_t size, int flags) throw()
{
  u8_t write_flags;
  size_t written;

  if (NETCONNTYPE_GROUP(netconn_type(_conn)) != NETCONN_TCP)
    return sendto(data, size, flags, NULL, 0);

  write_flags = NETCONN_COPY |
    ((flags & MSG_MORE)     ? NETCONN_MORE      : 0) |
    ((flags & MSG_DONTWAIT) ? NETCONN_DONTBLOCK : 0);
  written = 0;
  err_t err = netconn_write_partly(_conn, data, size, write_flags, &written);

  return (err == ERR_OK ? (int)written : -err_to_errno(err));
}

ssize_t
Socket_file::recv(void *data, size_t size, int flags) throw()
{ return recvfrom(data, size, flags, 0, 0); }

ssize_t
Socket_file::readv(const struct iovec *vec, int iovcnt) throw()
{
  if (iovcnt > 1) printf("Socket_file::readv: Implement iovcnt>1\n");
  return recvfrom(vec[0].iov_base, vec[0].iov_len, 0, 0, 0);
}

ssize_t
Socket_file::writev(const struct iovec *vec, int iovcnt) throw()
{
  if (iovcnt > 1) printf("Socket_file::writev: Implement iovcnt>1\n");
  return send(vec[0].iov_base, vec[0].iov_len, 0);
}


}

int socket(int domain, int type, int protocol) __THROW
{
  using cxx::Ref_ptr;
  Ref_ptr<Socket_file> s(Socket_file::socket(domain, type, protocol));
  if (!s)
    return -1;

  int fd = L4Re::Vfs::vfs_ops->alloc_fd(s);
  if (fd < 0)
    {
      errno = ENFILE;
      return -1;
    }

  return fd;
}


static void  __attribute__((constructor)) init()
{
  tcpip_init(NULL, NULL);
}
