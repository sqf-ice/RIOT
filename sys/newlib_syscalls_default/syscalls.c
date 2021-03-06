/*
 * Copyright (C) 2015 Kaspar Schleiser <kaspar@schleiser.de>
 *               2014 Freie Universität Berlin
 *
 * This file is subject to the terms and conditions of the GNU Lesser General
 * Public License v2.1. See the file LICENSE in the top level directory for more
 * details.
 */

/**
 * @ingroup     sys_newlib
 * @{
 *
 * @file
 * @brief       Newlib system call implementations
 *
 * @author      Michael Baar <michael.baar@fu-berlin.de>
 * @author      Stefan Pfeiffer <pfeiffer@inf.fu-berlin.de>
 * @author      Hauke Petersen <hauke.petersen@fu-berlin.de>
 * @author      Kaspar Schleiser <kaspar@schleiser.de>
 *
 * @}
 */

#include <unistd.h>
#include <reent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdint.h>

#include "cpu.h"
#include "board.h"
#include "sched.h"
#include "thread.h"
#include "irq.h"
#include "log.h"
#include "periph/pm.h"
#if MODULE_VFS
#include "vfs.h"
#endif

#include "uart_stdio.h"

#ifdef MODULE_XTIMER
#include <sys/time.h>
#include "div.h"
#include "xtimer.h"
#endif

/**
 * @brief manage the heap
 */
extern char _sheap;                 /* start of the heap */
extern char _eheap;                 /* end of the heap */
char *heap_top = &_sheap + 4;

/* MIPS newlib crt implements _init,_fini and _exit and manages the heap */
#ifndef __mips__
/**
 * @brief Initialize NewLib, called by __libc_init_array() from the startup script
 */
void _init(void)
{
    uart_stdio_init();
}

/**
 * @brief Free resources on NewLib de-initialization, not used for RIOT
 */
/* __attribute__((used)) fixes linker errors when building with LTO, but without nano.specs */
__attribute__((used)) void _fini(void)
{
    /* nothing to do here */
}

/**
 * @brief Exit a program without cleaning up files
 *
 * If your system doesn't provide this, it is best to avoid linking with subroutines that
 * require it (exit, system).
 *
 * @param n     the exit code, 0 for all OK, >0 for not OK
 */
void _exit(int n)
{
    LOG_INFO("#! exit %i: powering off\n", n);
    pm_off();
    while(1);
}

/**
 * @brief Allocate memory from the heap.
 *
 * The current heap implementation is very rudimentary, it is only able to allocate
 * memory. But it does not have any means to free memory again
 *
 * @return      pointer to the newly allocated memory on success
 * @return      pointer set to address `-1` on failure
 */
void *_sbrk_r(struct _reent *r, ptrdiff_t incr)
{
    unsigned int state = irq_disable();
    void *res = heap_top;

    if ((heap_top + incr > &_eheap) || (heap_top + incr < &_sheap)) {
        r->_errno = ENOMEM;
        res = (void *)-1;
    }
    else {
        heap_top += incr;
    }

    irq_restore(state);
    return res;
}

#endif /*__mips__*/

/**
 * @brief Get the process-ID of the current thread
 *
 * @return      the process ID of the current thread
 */
pid_t _getpid(void)
{
    return sched_active_pid;
}

/**
 * @brief Get the process-ID of the current thread
 *
 * @return      the process ID of the current thread
 */
pid_t _getpid_r(struct _reent *ptr)
{
    (void) ptr;
    return sched_active_pid;
}

/**
 * @brief Send a signal to a given thread
 *
 * @param r     TODO
 * @param pid   TODO
 * @param sig   TODO
 *
 * @return      TODO
 */
__attribute__ ((weak))
int _kill_r(struct _reent *r, pid_t pid, int sig)
{
    (void) pid;
    (void) sig;
    r->_errno = ESRCH;                      /* not implemented yet */
    return -1;
}

#if MODULE_VFS
/**
 * @brief Open a file
 *
 * This is a wrapper around @c vfs_open
 *
 * @param r     pointer to reent structure
 * @param name  file name to open
 * @param flags flags, see man 3p open
 * @param mode  mode, file creation mode if the file is created when opening
 *
 * @return      fd number (>= 0) on success
 * @return      -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
int _open_r(struct _reent *r, const char *name, int flags, int mode)
{
    int fd = vfs_open(name, flags, mode);
    if (fd < 0) {
        /* vfs returns negative error codes */
        r->_errno = -fd;
        return -1;
    }
    return fd;
}

/**
 * @brief Read bytes from an open file
 *
 * This is a wrapper around @c vfs_read
 *
 * @param[in]  r      pointer to reent structure
 * @param[in]  fd     open file descriptor obtained from @c open()
 * @param[out] dest   destination buffer
 * @param[in]  count  maximum number of bytes to read
 *
 * @return       number of bytes read on success
 * @return       -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
_ssize_t _read_r(struct _reent *r, int fd, void *dest, size_t count)
{
    int res = vfs_read(fd, dest, count);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return res;
}

/**
 * @brief Write bytes to an open file
 *
 * This is a wrapper around @c vfs_write
 *
 * @param[in]  r      pointer to reent structure
 * @param[in]  fd     open file descriptor obtained from @c open()
 * @param[in]  src    source data buffer
 * @param[in]  count  maximum number of bytes to write
 *
 * @return       number of bytes written on success
 * @return       -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
_ssize_t _write_r(struct _reent *r, int fd, const void *src, size_t count)
{
    int res = vfs_write(fd, src, count);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return res;
}

/**
 * @brief Close an open file
 *
 * This is a wrapper around @c vfs_close
 *
 * If this call returns an error, the fd should still be considered invalid and
 * no further attempt to use it shall be made, not even to retry @c close()
 *
 * @param[in]  r      pointer to reent structure
 * @param[in]  fd     open file descriptor obtained from @c open()
 *
 * @return       0 on success
 * @return       -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
int _close_r(struct _reent *r, int fd)
{
    int res = vfs_close(fd);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return res;
}

/**
 * @brief Query or set options on an open file
 *
 * This is a wrapper around @c vfs_fcntl
 *
 * @param[in]  r      pointer to reent structure
 * @param[in]  fd     open file descriptor obtained from @c open()
 * @param[in]  cmd    fcntl command, see man 3p fcntl
 * @param[in]  arg    argument to fcntl command, see man 3p fcntl
 *
 * @return       0 on success
 * @return       -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
int _fcntl_r (struct _reent *r, int fd, int cmd, int arg)
{
    int res = vfs_fcntl(fd, cmd, arg);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return res;
}

/**
 * @brief Seek to position in file
 *
 * This is a wrapper around @c vfs_lseek
 *
 * @p whence determines the function of the seek and should be set to one of
 * the following values:
 *
 *  - @c SEEK_SET: Seek to absolute offset @p off
 *  - @c SEEK_CUR: Seek to current location + @p off
 *  - @c SEEK_END: Seek to end of file + @p off
 *
 * @param[in]  r        pointer to reent structure
 * @param[in]  fd       open file descriptor obtained from @c open()
 * @param[in]  off      seek offset
 * @param[in]  whence   determines the seek method, see detailed description
 *
 * @return the new seek location in the file on success
 * @return -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
_off_t _lseek_r(struct _reent *r, int fd, _off_t off, int whence)
{
    int res = vfs_lseek(fd, off, whence);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return res;
}

/**
 * @brief Get status of an open file
 *
 * This is a wrapper around @c vfs_fstat
 *
 * @param[in]  r        pointer to reent structure
 * @param[in]  fd       open file descriptor obtained from @c open()
 * @param[out] buf      pointer to stat struct to fill
 *
 * @return 0 on success
 * @return -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
int _fstat_r(struct _reent *r, int fd, struct stat *buf)
{
    int res = vfs_fstat(fd, buf);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return 0;
}

/**
 * @brief Status of a file (by name)
 *
 * This is a wrapper around @c vfs_fstat
 *
 * @param[in]  r        pointer to reent structure
 * @param[in]  name     path to file
 * @param[out] buf      pointer to stat struct to fill
 *
 * @return 0 on success
 * @return -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
int _stat_r(struct _reent *r, const char *name, struct stat *st)
{
    int res = vfs_stat(name, st);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return 0;
}

/**
 * @brief  Unlink (delete) a file
 *
 * @param[in]  r        pointer to reent structure
 * @param[in]  path     path to file to be deleted
 *
 * @return 0 on success
 * @return -1 on error, @c r->_errno set to a constant from errno.h to indicate the error
 */
int _unlink_r(struct _reent *r, const char *path)
{
    int res = vfs_unlink(path);
    if (res < 0) {
        /* vfs returns negative error codes */
        r->_errno = -res;
        return -1;
    }
    return 0;
}

#else /* MODULE_VFS */

/* Fallback stdio_uart wrappers for when VFS is not used, does not allow any
 * other file access */
/*
 * Fallback read function
 *
 * All input is read from uart_stdio regardless of fd number. The function will
 * block until a byte is actually read.
 *
 * Note: the read function does not buffer - data will be lost if the function is not
 * called fast enough.
 */
_ssize_t _read_r(struct _reent *r, int fd, void *buffer, size_t count)
{
    (void)r;
    (void)fd;
    return uart_stdio_read(buffer, count);
}

/*
 * Fallback write function
 *
 * All output is directed to uart_stdio, independent of the given file descriptor.
 * The write call will further block until the byte is actually written to the UART.
 */
_ssize_t _write_r(struct _reent *r, int fd, const void *data, size_t count)
{
    (void) r;
    (void) fd;
    return uart_stdio_write(data, count);
}

/* Stubs to avoid linking errors, these functions do not have any effect */
int _open_r(struct _reent *r, const char *name, int flags, int mode)
{
    (void) name;
    (void) flags;
    (void) mode;
    r->_errno = ENODEV;
    return -1;
}

int _close_r(struct _reent *r, int fd)
{
    (void) fd;
    r->_errno = ENODEV;
    return -1;
}

_off_t _lseek_r(struct _reent *r, int fd, _off_t pos, int dir)
{
    (void) fd;
    (void) pos;
    (void) dir;
    r->_errno = ENODEV;
    return -1;
}

int _fstat_r(struct _reent *r, int fd, struct stat *st)
{
    (void) fd;
    (void) st;
    r->_errno = ENODEV;
    return -1;
}

int _stat_r(struct _reent *r, const char *name, struct stat *st)
{
    (void) name;
    (void) st;
    r->_errno = ENODEV;
    return -1;
}

int _unlink_r(struct _reent *r, const char *path)
{
    (void) path;
    r->_errno = ENODEV;
    return -1;
}
#endif /* MODULE_VFS */

/**
 * @brief Query whether output stream is a terminal
 *
 * @param r     TODO
 * @param fd    TODO
 *
 * @return      TODO
 */
int _isatty_r(struct _reent *r, int fd)
{
    r->_errno = 0;

    if(fd == STDIN_FILENO || fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        return 1;
    }

    return 0;
}

/**
 * @brief Send a signal to a thread
 *
 * @param[in] pid the pid to send to
 * @param[in] sig the signal to send
 *
 * @return TODO
 */
__attribute__ ((weak))
int _kill(pid_t pid, int sig)
{
    (void) pid;
    (void) sig;
    errno = ESRCH;                         /* not implemented yet */
    return -1;
}

#ifdef MODULE_XTIMER
int _gettimeofday_r(struct _reent *r, struct timeval *restrict tp, void *restrict tzp)
{
    (void) r;
    (void) tzp;
    uint64_t now = xtimer_now_usec64();
    tp->tv_sec = div_u64_by_1000000(now);
    tp->tv_usec = now - (tp->tv_sec * US_PER_SEC);
    return 0;
}
#else
int _gettimeofday_r(struct _reent *r, struct timeval *restrict tp, void *restrict tzp)
{
    (void) tp;
    (void) tzp;
    r->_errno = ENOSYS;
    return -1;
}
#endif
