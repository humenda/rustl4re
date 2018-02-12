PKGDIR          ?= ..
L4DIR           ?= $(PKGDIR)/../..

TARGET	         = liblwip.a liblwip.so
PC_FILENAME      = lwip
REQUIRES_LIBS    = libpthread
CONTRIB_INCDIR   = lwip \
                   lwip/contrib/src/include \
                   lwip/contrib/src/include/ipv4 \
                   lwip/contrib/src/include/ipv6
DEFINES          = -DLWIP_L4=1
SRC_C            = arch/sys_arch.c \
                   arch/perf.c \
                   contrib/src/core/def.c \
                   contrib/src/core/init.c \
                   contrib/src/core/dns.c \
                   contrib/src/core/inet_chksum.c \
                   contrib/src/core/mem.c \
                   contrib/src/core/memp.c \
                   contrib/src/core/netif.c \
                   contrib/src/core/pbuf.c \
                   contrib/src/core/raw.c \
                   contrib/src/core/stats.c \
                   contrib/src/core/sys.c \
                   contrib/src/core/tcp.c \
                   contrib/src/core/tcp_in.c \
                   contrib/src/core/tcp_out.c \
                   contrib/src/core/timers.c \
                   contrib/src/core/udp.c \
                   contrib/src/api/api_lib.c \
                   contrib/src/api/api_msg.c \
                   contrib/src/api/err.c \
                   contrib/src/api/netbuf.c \
                   contrib/src/api/netdb.c \
                   contrib/src/api/netifapi.c \
                   contrib/src/api/sockets.c \
                   contrib/src/api/tcpip.c \
                   contrib/src/netif/etharp.c \
                   contrib/src/core/ipv4/autoip.c \
                   contrib/src/core/ipv4/dhcp.c \
                   contrib/src/core/ipv4/icmp.c \
                   contrib/src/core/ipv4/igmp.c \
                   contrib/src/core/ipv4/ip4_addr.c \
                   contrib/src/core/ipv4/ip4.c \
                   contrib/src/core/ipv4/ip_frag.c \
                   contrib/src/core/ipv6/dhcp6.c \
                   contrib/src/core/ipv6/ethip6.c \
                   contrib/src/core/ipv6/icmp6.c \
                   contrib/src/core/ipv6/inet6.c \
                   contrib/src/core/ipv6/ip6.c \
                   contrib/src/core/ipv6/ip6_addr.c \
                   contrib/src/core/ipv6/ip6_frag.c \
                   contrib/src/core/ipv6/mld6.c \
                   contrib/src/core/ipv6/nd6.c \
                   contrib/src/core/snmp/asn1_dec.c \
                   contrib/src/core/snmp/asn1_enc.c \
                   contrib/src/core/snmp/mib2.c \
                   contrib/src/core/snmp/mib_structs.c \
                   contrib/src/core/snmp/msg_in.c \
                   contrib/src/core/snmp/msg_out.c

include $(L4DIR)/mk/lib.mk

CFLAGS += -Wno-unused-function -Wno-address
