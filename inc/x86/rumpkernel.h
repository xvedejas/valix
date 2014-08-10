/*  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  Maintained by:
 *      Xander Vedejas <xvedejas@gmail.com>
 */
#ifndef __rumpkernel_h__
#define __rumpkernel_h__

// See: http://netbsd.gw.com/cgi-bin/man-cgi?rumpuser++NetBSD-current

typedef unsigned int size_t;
typedef unsigned long uint64_t;
typedef long int64_t;

typedef short pid_t;

struct rumpuser_cv;
struct rumpuser_mtx; // mutex
struct rumpuser_iovec;
struct lwp;
struct rumpuser_rw;

/* hypervisor upcall routines */
struct rump_hyperup
{
	void (*hyp_schedule)(void);
	void (*hyp_unschedule)(void);
	void (*hyp_backend_unschedule)(int, int *, void *);
	void (*hyp_backend_schedule)(int, void *);
	void (*hyp_lwproc_switch)(struct lwp *);
	void (*hyp_lwproc_release)(void);
	int (*hyp_lwproc_rfork)(void *, int, const char *);
	int (*hyp_lwproc_newlwp)(pid_t);
	struct lwp * (*hyp_lwproc_curlwp)(void);
	int (*hyp_syscall)(int, void *, long *);
	void (*hyp_lwpexit)(void);
	void (*hyp_execnotify)(const char *);
	pid_t (*hyp_getpid)(void);
	void *hyp__extra[8];
};

typedef void (*rump_biodone_fn)(void *, size_t, int);

extern int rumpuser_init(int version, struct rump_hyperup *hyp);

// Memory Allocation

extern int rumpuser_malloc(size_t len, int alignment, void **memp);
extern void rumpuser_free(void *mem, size_t len);

// Files and I/O

#define RUMPUSER_OPEN_RDONLY	0x0000
#define RUMPUSER_OPEN_WRONLY	0x0001
#define RUMPUSER_OPEN_RDWR	    0x0002
#define RUMPUSER_OPEN_CREATE	0x0004 /* create file if it doesn't exist */
#define RUMPUSER_OPEN_EXCL	    0x0008 /* exclusive open */
#define RUMPUSER_OPEN_BIO	    0x0010 /* open device for block i/o */

extern int rumpuser_open(const char *name, int mode, int *fdp);
extern int rumpuser_close(int fd);
extern int rumpuser_getfileinfo(const char *name, uint64_t *size, int *type);
extern void rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off,
                         rump_biodone_fn biodone, void *donearg);
extern int rumpuser_iovread(int fd, struct rumpuser_iovec *ruiov, size_t iovlen,
                            int64_t off, size_t *retv);
extern int rumpuser_iovwrite(int fd, struct rumpuser_iovec *ruiov,
                             size_t iovlen, int64_t off, size_t *retv);
extern int rumpuser_syncfd(int fd, int flags, uint64_t start, uint64_t len);

// Clocks

extern int rumpuser_clock_gettime(int enum_rumpclock, int64_t *sec, long *nsec);
extern int rumpuser_clock_sleep(int enum_rumpclock, int64_t sec, long nsec);

// Parameter Retrieval

extern int rumpuser_getparam(const char *name, void *buf, size_t buflen);

// Termination

extern void rumpuser_exit(int value);

// Console output

extern void rumpuser_putchar(int ch);
extern void rumpuser_dprintf(const char *fmt, ...);

// Signals

extern int rumpuser_kill(int64_t pid, int sig);

// Random Pool

extern int rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp);

// Threads

extern int rumpuser_thread_create(void *(*fun)(void *), void *arg,
     const char *thrname, int mustjoin, int priority, int cpuidx,
     void **cookie);

extern void rumpuser_thread_exit(void);
extern int rumpuser_thread_join(void *cookie);
extern void rumpuser_curlwpop(int enum_rumplwpop, struct lwp *l);
extern struct lwp * rumpuser_curlwp(void);
extern void rumpuser_seterrno(int errno);

// Mutexes, rwlocks and condition variables

extern void rumpuser_mutex_init(struct rumpuser_mtx **mtxp, int flags);
extern void rumpuser_mutex_enter(struct rumpuser_mtx *mtx);
extern void rumpuser_mutex_enter_nowrap(struct rumpuser_mtx *mtx);
extern int rumpuser_mutex_tryenter(struct rumpuser_mtx *mtx);
extern void rumpuser_mutex_exit(struct rumpuser_mtx *mtx);
extern void rumpuser_mutex_destroy(struct rumpuser_mtx *mtx);
extern void rumpuser_mutex_owner(struct rumpuser_mtx *mtx, struct lwp **lp);

extern void rumpuser_rw_init(struct rumpuser_rw **rwp);
extern void rumpuser_rw_enter(int enum_rumprwlock, struct rumpuser_rw *rw);
extern int rumpuser_rw_tryenter(int enum_rumprwlock, struct rumpuser_rw *rw);
extern int rumpuser_rw_tryupgrade(struct rumpuser_rw *rw);
extern void rumpuser_rw_downgrade(struct rumpuser_rw *rw);
extern void rumpuser_rw_exit(struct rumpuser_rw *rw);
extern void rumpuser_rw_destroy(struct rumpuser_rw *rw);
extern void rumpuser_rw_held(int enum_rumprwlock, struct rumpuser_rw *rw,
                             int *heldp);

extern void rumpuser_cv_init(struct rumpuser_cv **cvp);
extern void rumpuser_cv_destroy(struct rumpuser_cv *cv);
extern void rumpuser_cv_wait(struct rumpuser_cv *cv, struct rumpuser_mtx *mtx);
extern void rumpuser_cv_wait_nowrap(struct rumpuser_cv *cv,
                                    struct rumpuser_mtx *mtx);
extern int rumpuser_cv_timedwait(struct rumpuser_cv *cv,
                                 struct rumpuser_mtx *mtx, int64_t sec,
                                 int64_t nsec);
extern void rumpuser_cv_signal(struct rumpuser_cv *cv);
extern void rumpuser_cv_broadcast(struct rumpuser_cv *cv);
extern void rumpuser_cv_has_waiters(struct rumpuser_cv *cv, int *waitersp);



#endif // __rumpkernel_h__
