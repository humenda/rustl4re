#ifndef RPC_H
#define RPC_H

#include "types.h"
#include "in.h"
#include "timer.h"

enum xdr_op {
	XDR_ENCODE = 0,
	XDR_DECODE = 1,
	XDR_FREE = 2
};

#define BYTES_PER_XDR_UNIT	(4)

#define RNDUP(x)  (((x) + BYTES_PER_XDR_UNIT - 1) & ~(BYTES_PER_XDR_UNIT - 1))

typedef struct XDR XDR;
struct XDR {
	enum xdr_op x_op;		/* operation; fast additional param */
	
	/* We use following private */
	caddr_t x_public;		/* users' data */
	caddr_t x_private;		/* pointer to private data */
	caddr_t x_base;		/* private used for position info */
	int x_handy;		/* extra private word */
};

typedef bool_t (*xdrproc_t) (XDR *, void *,...);

#define XDR_INLINE xdr_inline

#define NULL_xdrproc_t ((xdrproc_t)0)
struct xdr_discrim {
	int value;
	xdrproc_t proc;
};

#define IXDR_GET_INT32(buf)           ((int32_t)ntohl((uint32_t)*(buf)++))
#define IXDR_PUT_INT32(buf, v)        (*(buf)++ = (int32_t)htonl((uint32_t)(v)))
#define IXDR_GET_U_INT32(buf)         ((uint32_t)IXDR_GET_INT32(buf))
#define IXDR_PUT_U_INT32(buf, v)      IXDR_PUT_INT32(buf, (int32_t)(v))

/* WARNING: The IXDR_*_LONG defines are removed by Sun for new platforms
 * and shouldn't be used any longer. Code which use this defines or longs
 * in the RPC code will not work on 64bit Solaris platforms !
 */
#define IXDR_GET_LONG(buf) ((long)IXDR_GET_U_INT32(buf))
#define IXDR_PUT_LONG(buf, v) ((long)IXDR_PUT_INT32(buf, (long)(v)))
#define IXDR_GET_U_LONG(buf)	      ((u_long)IXDR_GET_LONG(buf))
#define IXDR_PUT_U_LONG(buf, v)	      IXDR_PUT_LONG(buf, (long)(v))


#define IXDR_GET_BOOL(buf)            ((bool_t)IXDR_GET_LONG(buf))
#define IXDR_GET_ENUM(buf, t)         ((t)IXDR_GET_LONG(buf))
#define IXDR_GET_SHORT(buf)           ((short)IXDR_GET_LONG(buf))
#define IXDR_GET_U_SHORT(buf)         ((u_short)IXDR_GET_LONG(buf))

#define IXDR_PUT_BOOL(buf, v)         IXDR_PUT_LONG(buf, (long)(v))
#define IXDR_PUT_ENUM(buf, v)         IXDR_PUT_LONG(buf, (long)(v))
#define IXDR_PUT_SHORT(buf, v)        IXDR_PUT_LONG(buf, (long)(v))
#define IXDR_PUT_U_SHORT(buf, v)      IXDR_PUT_LONG(buf, (long)(v))

/*
 * These are the "generic" xdr routines.
 * None of these can have const applied because it's not possible to
 * know whether the call is a read or a write to the passed parameter
 * also, the XDR structure is always updated by some of these calls.
 */
extern bool_t xdr_void (void)  ;
extern bool_t xdr_short (XDR *__xdrs, short *__sp)  ;
extern bool_t xdr_u_short (XDR *__xdrs, u_short *__usp)  ;
extern bool_t xdr_int (XDR *__xdrs, int *__ip)  ;
extern bool_t xdr_u_int (XDR *__xdrs, u_int *__up)  ;
extern bool_t xdr_long (XDR *__xdrs, long *__lp)  ;
extern bool_t xdr_u_long (XDR *__xdrs, u_long *__ulp)  ;
extern bool_t xdr_hyper (XDR *__xdrs, quad_t *__llp)  ;
extern bool_t xdr_u_hyper (XDR *__xdrs, u_quad_t *__ullp)  ;
extern bool_t xdr_longlong_t (XDR *__xdrs, quad_t *__llp)  ;
extern bool_t xdr_u_longlong_t (XDR *__xdrs, u_quad_t *__ullp)  ;
extern bool_t xdr_int8_t (XDR *__xdrs, int8_t *__ip)  ;
extern bool_t xdr_uint8_t (XDR *__xdrs, uint8_t *__up)  ;
extern bool_t xdr_int16_t (XDR *__xdrs, int16_t *__ip)  ;
extern bool_t xdr_uint16_t (XDR *__xdrs, uint16_t *__up)  ;
extern bool_t xdr_int32_t (XDR *__xdrs, int32_t *__ip)  ;
extern bool_t xdr_uint32_t (XDR *__xdrs, uint32_t *__up)  ;
extern bool_t xdr_int64_t (XDR *__xdrs, int64_t *__ip)  ;
extern bool_t xdr_uint64_t (XDR *__xdrs, uint64_t *__up)  ;
extern bool_t xdr_bool (XDR *__xdrs, bool_t *__bp)  ;
extern bool_t xdr_enum (XDR *__xdrs, enum_t *__ep)  ;
extern bool_t xdr_array (XDR * _xdrs, caddr_t *__addrp, u_int *__sizep,
			 u_int __maxsize, u_int __elsize, xdrproc_t __elproc)
      ;
extern bool_t xdr_bytes (XDR *__xdrs, char **__cpp, u_int *__sizep,
			 u_int __maxsize)  ;
extern bool_t xdr_opaque (XDR *__xdrs, caddr_t __cp, u_int __cnt)  ;
extern bool_t xdr_string (XDR *__xdrs, char **__cpp, u_int __maxsize)  ;
extern bool_t xdr_union (XDR *__xdrs, enum_t *__dscmp, char *__unp,
			 __const struct xdr_discrim *__choices,
			 xdrproc_t dfault)  ;
extern bool_t xdr_char (XDR *__xdrs, char *__cp)  ;
extern bool_t xdr_u_char (XDR *__xdrs, u_char *__cp)  ;
extern bool_t xdr_vector (XDR *__xdrs, char *__basep, u_int __nelem,
			  u_int __elemsize, xdrproc_t __xdr_elem)  ;
extern bool_t xdr_float (XDR *__xdrs, float *__fp)  ;
extern bool_t xdr_double (XDR *__xdrs, double *__dp)  ;
extern bool_t xdr_reference (XDR *__xdrs, caddr_t *__xpp, u_int __size,
			     xdrproc_t __proc)  ;
extern bool_t xdr_pointer (XDR *__xdrs, char **__objpp,
			   u_int __obj_size, xdrproc_t __xdr_obj)  ;
extern bool_t xdr_wrapstring (XDR *__xdrs, char **__cpp)  ;
extern u_long xdr_sizeof (xdrproc_t, void *)  ;

/*
 * Common opaque bytes objects used by many rpc protocols;
 * declared here due to commonality.
 */
#define MAX_NETOBJ_SZ 1024
struct netobj {
	u_int n_len;
	char *n_bytes;
};
typedef struct netobj netobj;
extern bool_t xdr_netobj (XDR *__xdrs, struct netobj *__np)  ;


/*
 * Rpc calls return an enum clnt_stat.  This should be looked at more,
 * since each implementation is required to live with this (implementation
 * independent) list of errors.
 */
enum clnt_stat {
	RPC_SUCCESS=0,			/* call succeeded */
	/*
	 * local errors
	 */
	RPC_CANTENCODEARGS=1,		/* can't encode arguments */
	RPC_CANTDECODERES=2,		/* can't decode results */
	RPC_CANTSEND=3,			/* failure in sending call */
	RPC_CANTRECV=4,			/* failure in receiving result */
	RPC_TIMEDOUT=5,			/* call timed out */
	/*
	 * remote errors
	 */
	RPC_VERSMISMATCH=6,		/* rpc versions not compatible */
	RPC_AUTHERROR=7,		/* authentication error */
	RPC_PROGUNAVAIL=8,		/* program not available */
	RPC_PROGVERSMISMATCH=9,		/* program version mismatched */
	RPC_PROCUNAVAIL=10,		/* procedure unavailable */
	RPC_CANTDECODEARGS=11,		/* decode arguments error */
	RPC_SYSTEMERROR=12,		/* generic "other problem" */
	RPC_NOBROADCAST = 21,		/* Broadcasting not supported */
	/*
	 * callrpc & clnt_create errors
	 */
	RPC_UNKNOWNHOST=13,		/* unknown host name */
	RPC_UNKNOWNPROTO=17,		/* unknown protocol */
	RPC_UNKNOWNADDR = 19,		/* Remote address unknown */

	/*
	 * rpcbind errors
	 */
	RPC_RPCBFAILURE=14,		/* portmapper failed in its call */
#define RPC_PMAPFAILURE RPC_RPCBFAILURE
	RPC_PROGNOTREGISTERED=15,	/* remote program is not registered */
	RPC_N2AXLATEFAILURE = 22,	/* Name to addr translation failed */
	/*
	 * unspecified error
	 */
	RPC_FAILED=16,
	RPC_INTR=18,
	RPC_TLIERROR=20,
	RPC_UDERROR=23,
        /*
         * asynchronous errors
         */
        RPC_INPROGRESS = 24,
        RPC_STALERACHANDLE = 25
};

/*
 * Status returned from authentication check
 */
enum auth_stat {
	AUTH_OK=0,
	/*
	 * failed at remote end
	 */
	AUTH_BADCRED=1,			/* bogus credentials (seal broken) */
	AUTH_REJECTEDCRED=2,		/* client should begin new session */
	AUTH_BADVERF=3,			/* bogus verifier (seal broken) */
	AUTH_REJECTEDVERF=4,		/* verifier expired or was replayed */
	AUTH_TOOWEAK=5,			/* rejected due to security reasons */
	/*
	 * failed locally
	*/
	AUTH_INVALIDRESP=6,		/* bogus response verifier */
	AUTH_FAILED=7			/* some unknown reason */
};




/*
 * Error info.
 */
struct rpc_err {
	enum clnt_stat re_status;
	union {
		int RE_errno;		/* related system error */
		enum auth_stat RE_why;	/* why the auth error occurred */
		struct {
			u_long low;		/* lowest verion supported */
			u_long high;		/* highest verion supported */
		} RE_vers;
		struct {			/* maybe meaningful if RPC_FAILED */
			long s1;
			long s2;
		} RE_lb;			/* life boot & debugging only */
	} ru;
#define	re_errno	ru.RE_errno
#define	re_why		ru.RE_why
#define	re_vers		ru.RE_vers
#define	re_lb		ru.RE_lb
};

struct opaque_auth{
	long flavor;
	long length;
	long data[0];
};
extern long *add_auth_unix(struct opaque_auth *auth);
extern long *add_auth_none(struct opaque_auth *auth);

typedef struct AUTH AUTH;
struct AUTH{
	long *(* add_auth)(struct opaque_auth *);

};
extern AUTH *__authnone_create(AUTH *auth);
extern void auth_destroy(AUTH *auth);

/*
 * Client rpc handle.
 * Created by individual implementations, see e.g. rpc_udp.c.
 * Client is responsible for initializing auth, see e.g. auth_none.c.
 */
typedef struct CLIENT CLIENT;
struct CLIENT {
	AUTH	*cl_auth;	/* authenticator */
	unsigned long xid;	/* RPC id */
	int server;		/* entry in ARP table */
	u_short port;		/* RPC server port */
	u_short sport;		/* Local port */
	u_long prognum;		/* RPC program number */
	u_long versnum;		/* RPC program version */
};

/*
 * control operations that apply to all transports
 *
 * Note: options marked XXX are no-ops in this implementation of RPC.
 * The are present in TI-RPC but can't be implemented here since they
 * depend on the presence of STREAMS/TLI, which we don't have.
 */
#define CLSET_TIMEOUT        1    /* set timeout (timeval) */
#define CLGET_TIMEOUT        2    /* get timeout (timeval) */
#define CLGET_SERVER_ADDR    3    /* get server's address (sockaddr) */
#define CLGET_FD             6    /* get connections file descriptor */
#define CLGET_SVC_ADDR       7    /* get server's address (netbuf)      XXX */
#define CLSET_FD_CLOSE       8    /* close fd while clnt_destroy */
#define CLSET_FD_NCLOSE      9    /* Do not close fd while clnt_destroy*/
#define CLGET_XID            10   /* Get xid */
#define CLSET_XID            11   /* Set xid */
#define CLGET_VERS           12   /* Get version number */
#define CLSET_VERS           13   /* Set version number */
#define CLGET_PROG           14   /* Get program number */
#define CLSET_PROG           15   /* Set program number */
#define CLSET_SVC_ADDR       16   /* get server's address (netbuf)      XXX */
#define CLSET_PUSH_TIMOD     17   /* push timod if not already present  XXX */
#define CLSET_POP_TIMOD      18   /* pop timod                          XXX */
/*
 * Connectionless only control operations
 */
#define CLSET_RETRY_TIMEOUT	4	/* set retry timeout (timeval) */
#define CLGET_RETRY_TIMEOUT	5	/* get retry timeout (timeval) */


#define UDPMSGSIZE	8800	/* rpc imposed limit on udp msg size */
#define RPCSMALLMSGSIZE	400	/* a more reasonable packet size */

#define clnt_call clntudp_call

CLIENT *__clntudp_create(CLIENT *__clnt, AUTH *__auth, int __server,
			 u_short __port, u_long __prognum, u_long __versnum);

struct rpc_pkg{
	struct iphdr ip;
	struct udphdr udp;
	union{
		char data[300];		/* longest RPC call must fit!!!! */
		/* The call body */
		struct{
			long xid;
			long mtype;
			long rpcvers;
			long prog;
			long vers;
			long proc;
			struct opaque_auth auth[0];
		}call;
		/* The reply body */
		struct{
			long xid;
			long mtype;
			long stat;
			long verifier;
			long v2;
			long astatus;
			long data[0];
		}reply;
	}u;
};
#define cbody u.call
#define rbody u.reply
#define areply u.reply.u

#define AUTH_NONE	0		/* no authentication */
#define	AUTH_NULL	0		/* backward compatibility */
#define	AUTH_SYS	1		/* unix style (uid, gids) */
#define	AUTH_UNIX	AUTH_SYS
#define	AUTH_SHORT	2		/* short hand unix style */
#define AUTH_DES	3		/* des style (encrypted timestamps) */
#define AUTH_DH		AUTH_DES	/* Diffie-Hellman (this is DES) */
#define AUTH_KERB       4               /* kerberos style */

#define PMAP_PORT 111

struct mapping {
	u_int prog;
	u_int vers;
	u_int prot;
	u_int port;
};

typedef struct mapping mappping;
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17

#define PMAP_PROG 100000
#define PMAP_VERS 2

#define PMAPROC_GETPORT 3
extern  bool_t xdr_mappping (XDR *, mappping*);


enum msg_type {
	MSG_CALL = 0,
	MSG_REPLY = 1
};



#endif /*RPC_H*/
