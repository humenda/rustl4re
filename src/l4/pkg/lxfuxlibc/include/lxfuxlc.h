/*
 * (c) 2004-2009 Adam Lackorzynski <adam@os.inf.tu-dresden.de>,
 *               Alexander Warg <warg@os.inf.tu-dresden.de>
 *     economic rights: Technische Universität Dresden (Germany)
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */

/*
 * Header files for lxfuxlc.c
 */
#ifndef __LXFUXLC_H__
#define __LXFUXLC_H__

#include <l4/sys/compiler.h>

EXTERN_C_BEGIN

/* -- sys/time.h -- */
struct lx_timeval {
  long tv_sec;
  long tv_usec;
};

struct lx_timezone {
  int  tz_minuteswest;
  int  tz_dsttime;
};


typedef struct {
  unsigned long fds_bits[(1024/(8 * sizeof(unsigned long)))];
} lx_fd_set;

typedef long unsigned int lx_size_t;
typedef long int lx_ssize_t;

/* -- sys/poll.h */

typedef unsigned long int lx_nfds_t;

struct lx_pollfd {
  int fd;
  short int events;
  short int revents;
};

struct lx_iovec {
  void *iov_base;
  lx_size_t iov_len;
};

#define LX_POLLIN	0x001
#define LX_POLLPRI	0x002
#define LX_POLLOUT	0x004
#define LX_POLLRDNORM	0x040
#define LX_POLLRDBAND	0x080
#define LX_POLLWRNORM	0x100
#define LX_POLLWRBAND	0x200
#define LX_POLLMSG	0x400

#define LX_POLLERR	0x008
#define LX_POLLHUP	0x010
#define LX_POLLNVAL	0x020

/* -- */

#ifndef NULL
#ifdef __cplusplus
#define NULL (0)
#else
#define NULL ((void *)0)
#endif
#endif

typedef signed int lx_pid_t;

/* see asm-generic/statfs.h */
typedef struct {
	int val[2];
} lx__kernel_fsid_t;

struct lx_statfs {
	unsigned long f_type;
	unsigned long f_bsize;
	unsigned long f_blocks;
	unsigned long f_bfree;
	unsigned long f_bavail;
	unsigned long f_files;
	unsigned long f_ffree;
	lx__kernel_fsid_t f_fsid;
	unsigned long f_namelen;
	unsigned long f_frsize;
	unsigned long f_spare[5];
};

/* see asm/stat.h */
struct lx_stat {
        unsigned long  st_dev;
        unsigned long  st_ino;
        unsigned short st_mode;
        unsigned short st_nlink;
        unsigned short st_uid;
        unsigned short st_gid;
        unsigned long  st_rdev;
        unsigned long  st_size;
        unsigned long  st_blksize;
        unsigned long  st_blocks;
        unsigned long  st_atime_lxfux;
        unsigned long  st_atime_nsec_lxfux;
        unsigned long  st_mtime_lxfux;
        unsigned long  st_mtime_nsec_lxfux;
        unsigned long  st_ctime_lxfux;
        unsigned long  st_ctime_nsec_lxfux;
        unsigned long  __unused4;
        unsigned long  __unused5;
};

struct lx_stat64 {
        unsigned long long      st_dev;
        unsigned char   __pad0[4];
        unsigned long   __st_ino;
        unsigned int    st_mode;
        unsigned int    st_nlink;
        unsigned long   st_uid;
        unsigned long   st_gid;
        unsigned long long      st_rdev;
        unsigned char   __pad3[4];
        long long       st_size;
        unsigned long   st_blksize;
        unsigned long long      st_blocks;
        unsigned long   st_atime_lxfux;
        unsigned long   st_atime_nsec_lxfux;
        unsigned long   st_mtime_lxfux;
        unsigned int    st_mtime_nsec_lxfux;
        unsigned long   st_ctime_lxfux;
        unsigned long   st_ctime_nsec_lxfux;
        unsigned long long      st_ino;
};

struct lx_linux_dirent64 {
        unsigned long long d_ino;
        signed long long   d_off;
        unsigned short     d_reclen;
        unsigned char      d_type;
        char               d_name[0];
};

/*
 * socket/net defines
 */
#define LX_SOCK_STREAM 1
#define LX_PF_LOCAL    1
#define LX_PF_UNIX     LX_PF_LOCAL
#define LX_AF_UNIX     LX_PF_LOCAL
#define LX_AF_LOCAL    LX_AF_UNIX

#define LX_SOCKADDR_COMMON_SIZE  (sizeof(unsigned short int))

struct lx_sockaddr {
	unsigned short int sa_family;
	char sa_data[14];
};

struct lx_sockaddr_un {
	unsigned short int sun_family;
	char sun_path[108];
};


/*
 * fcntl defines
 */
#define LX_O_RDONLY		00
#define LX_O_WRONLY		01
#define LX_O_RDWR		02
#define LX_O_CREAT	      0100 /* not fcntl */
#define LX_O_EXCL	      0200 /* not fcntl */
#define LX_O_NOCTTY	      0400 /* not fcntl */
#define LX_O_TRUNC	     01000 /* not fcntl */
#define LX_O_APPEND	     02000
#define LX_O_NONBLOCK        04000
#define LX_O_NDELAY     O_NONBLOCK
#define LX_O_SYNC	    010000
#define LX_FASYNC	    020000 /* fcntl, for BSD compatibility */
#define LX_O_DIRECT	    040000 /* direct disk access hint */
#define LX_O_LARGEFILE     0100000
#define LX_O_DIRECTORY     0200000 /* must be a directory */
#define LX_O_NOFOLLOW      0400000 /* don't follow links */

#define LX_S_IRUSR           00400
#define LX_S_IWUSR           00200

#define LX_S_IFMT         00170000
#define LX_S_IFDIR         0040000
#define LX_S_ISDIR(m)      (((m) & LX_S_IFMT) == LX_S_IFDIR)

/*
 * Signal numbers
 */

#define LX_SIGHUP	1
#define LX_SIGINT	2
#define LX_SIGQUIT	3
#define LX_SIGILL	4
#define LX_SIGTRAP	5
#define LX_SIGABRT	6
#define LX_SIGIOT	6
#define LX_SIGBUS	7
#define LX_SIGFPE	8
#define LX_SIGKILL	9
#define LX_SIGUSR1	10
#define LX_SIGSEGV	11
#define LX_SIGUSR2	12
#define LX_SIGPIPE	13
#define LX_SIGALRM	14
#define LX_SIGTERM	15
#define LX_SIGSTKFLT	16
#define LX_SIGCHLD	17
#define LX_SIGCONT	18
#define LX_SIGSTOP	19
#define LX_SIGTSTP	20
#define LX_SIGTTIN	21
#define LX_SIGTTOU	22
#define LX_SIGURG	23
#define LX_SIGXCPU	24
#define LX_SIGXFSZ	25
#define LX_SIGVTALRM	26
#define LX_SIGPROF	27
#define LX_SIGWINCH	28
#define LX_SIGIO	29
#define LX_SIGPOLL	LX_SIGIO
#define LX_SIGPWR	30
#define LX_SIGSYS	31
#define LX_SIGUNUSED	31


/*
 * Syscall functions
 */

/*   1 */ extern void          lx_exit(int status) __attribute__ ((noreturn));
/*   2 */ extern lx_pid_t      lx_fork(void);
/*   3 */ extern long          lx_read(unsigned int fd, void *buf, unsigned int count);
/*   4 */ extern long          lx_write(unsigned int fd, const void *buf, unsigned int count);
/*   5 */ extern long          lx_open(const char *filename, int flags, int mode);
/*   6 */ extern long          lx_close(unsigned int fd);
/*   7 */ extern lx_pid_t      lx_waitpid(lx_pid_t pid, int * wait_stat, int options);
/*  10 */ extern int           lx_unlink(const char *filename);
/*  12 */ extern int           lx_chdir(const char *filename);
/*  19 */ extern long          lx_lseek(unsigned int fd, unsigned long offset, unsigned int origin);
/*  20 */ extern long          lx_getpid(void);
/*  37 */ extern int           lx_kill(lx_pid_t pid, int sig);
/*  38 */ extern int           lx_rename(const char *oldpath, const char *newpath);
/*  39 */ extern int           lx_mkdir(const char *filename, int mode);
/*  40 */ extern int           lx_rmdir(const char *filename);
/*  42 */ extern int           lx_pipe(int filesdes[2]);
/*  54 */ extern long          lx_ioctl(unsigned int fd, unsigned int cmd, unsigned long arg);
/*  78 */ extern long          lx_gettimeofday(struct lx_timeval *tv, struct lx_timezone *tz);
/*  81 */ extern void          lx_sync(void);
/*  82 */ extern int           lx_select(int n, lx_fd_set *readfds, lx_fd_set *writefds, lx_fd_set *exceptfds, struct lx_timeval *timeout);
/*  93 */ extern int           lx_ftruncate(int fd, unsigned long ofs);
/*  93 */ extern int           lx_statfs(const char *path, struct lx_statfs *buf);
/* 102 */ extern int           lx_socketcall(int call, unsigned long *args);
/* 106 */ extern int           lx_stat(const char *filename, struct lx_stat *buf);
/* 107 */ extern int           lx_lstat(const char *filename, struct lx_stat *buf);
/* 108 */ extern int           lx_fstat(int filedes, struct lx_stat *buf);
/* 117 */ extern int           lx_ipc(unsigned int call, int first, int second, int third, const void *ptr, long fifth);
/* 118 */ extern int           lx_fsync(int fd);
/* 140 */ extern long long     lx_lseek64(int fd, unsigned long long offset, int origin);
/* 145 */ extern lx_ssize_t    lx_readv(int fd, const struct lx_iovec *iov, int cnt);
/* 146 */ extern lx_ssize_t    lx_writev(int fd, const struct lx_iovec *iov, int cnt);
/* 148 */ extern int           lx_fdatasync(int fd);
/* 168 */ extern int           lx_poll(struct lx_pollfd *fds, lx_nfds_t nfds, int timeout);
#ifdef __i386__
/* 194 */ extern int           lx_ftruncate64(int fd, unsigned long long length);
#else
/* 194 */ extern int           lx_ftruncate64(int fd, unsigned long length);
#endif
/* 195 */ extern int           lx_stat64(const char *path, struct lx_stat64 *buf);
/* 197 */ extern int           lx_fstat64(int fd, struct lx_stat64 *buf);
/* 220 */ extern long          lx_getdents64(int fd, char *buf, unsigned int nbytes);
/* 221 */ extern int           lx_fcntl64(int fd, unsigned int cmd, unsigned long arg);

/* 295 */ extern int           lx_openat(int fd, const char *filename, int flags, int mode);
/* 296 */ extern int           lx_mkdirat(int fd, const char *name, int mode);
/* 301 */ extern int           lx_unlinkat(int fd, const char *name, int flags);
/* 302 */ extern int           lx_renameat(int ofd, const char *oname, int nfd, const char *nname);
/* 307 */ extern int           lx_faccessat(int dfd, const char *filename, int mode);
/* 333 */ extern lx_ssize_t    lx_preadv(int fd, const struct lx_iovec *iov, int cnt, unsigned long pl, unsigned long ph);
/* 334 */ extern lx_ssize_t    lx_pwritev(int fd, const struct lx_iovec *iov, int cnt, unsigned long pl, unsigned long ph);
/*
 * Wrapper functions
 */

extern long lx_time(int *tloc);
extern unsigned int lx_sleep(unsigned int seconds);
extern unsigned int lx_msleep(unsigned int mseconds);
extern lx_pid_t lx_wait(int *wait_stat);

extern void *lx_shmat(int shmid, const void *shmaddr, int shmflg);

extern int  lx_socket(int family, int type, int protocol);
extern int  lx_connect(int sockfd, const struct lx_sockaddr *saddr, unsigned long addrlen);

extern void lx_outchar(unsigned char c);
extern void lx_outdec32(unsigned int i);

/*
 * stdio.h
 */

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

#ifndef LX_EOF
#define LX_EOF (-1)
#endif

typedef struct {
  int fd;
  int flags;
  int taken;
} LX_FILE;

LX_FILE *lx_fopen(const char *filename, const char *mode);
LX_FILE *lx_fdopen(int fildes, const char *mode);
int lx_fseek(LX_FILE *f, long offset, int whence);
lx_size_t lx_fread(void *ptr, lx_size_t size, lx_size_t nmemb, LX_FILE *f);
lx_size_t lx_fwrite(const void *ptr, lx_size_t size, lx_size_t nmemb, LX_FILE *f);
long lx_ftell(LX_FILE *f);
void lx_rewind(LX_FILE *f);
int lx_fclose(LX_FILE *f);
int lx_fflush(LX_FILE *f);
int lx_fputc(int c, LX_FILE *f);
int lx_fprintf(LX_FILE *f, const char *format, ...);


#define SHMAT		21

EXTERN_C_END

#endif /* __LXFUXLC_H__ */
