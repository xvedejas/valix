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

#include <rumpkernel.h>
#include <mm.h>

// Some useful pages:
// http://netbsd.gw.com/cgi-bin/man-cgi?rumpuser++NetBSD-current
// http://ftp.vim.org/NetBSD/NetBSD-release-6/src/sys/rump/include/rump/rumpuser.h
// https://github.com/rumpkernel/rumpuser-baremetal/blob/master/include/arch/i386/types.h
// https://github.com/rumpkernel/rumpuser-baremetal/blob/master/rumpuser.c

int rumpuser_init(int version, struct rump_hyperup *hyp)
{
    return 0;
}

int rumpuser_malloc(size_t len, int alignment, void **memp)
{
    // Aligned alloc
    *memp = aalloc(len, alignment);
    if (memp == NULL)
        return -1; // error
    return 0;
}

void rumpuser_free(void *mem, size_t len)
{
    free(mem);
}

int rumpuser_open(const char *name, int mode, int *fdp)
{
    return 0;
}

int rumpuser_close(int fd)
{
    return 0;
}

int rumpuser_getfileinfo(const char *name, uint64_t *size, int *type)
{
    return 0;
}

void rumpuser_bio(int fd, int op, void *data, size_t dlen, int64_t off,
                  rump_biodone_fn biodone, void *donearg)
{

}

int rumpuser_iovread(int fd, struct rumpuser_iovec *ruiov, size_t iovlen,
                     int64_t off, size_t *retv)
{
    return 0;
}

int rumpuser_iovwrite(int fd, struct rumpuser_iovec *ruiov,
                      size_t iovlen, int64_t off, size_t *retv)
{
    return 0;
}

int rumpuser_syncfd(int fd, int flags, uint64_t start, uint64_t len)
{
    return 0;
}


/* ok, this isn't really random, but let's make-believe */
int rumpuser_getrandom(void *buf, size_t buflen, int flags, size_t *retp)
{
    static unsigned seed = 12345;
    unsigned *v = buf;
    *retp = buflen;
    while (buflen >= 4)
    {
        buflen -= 4;
        *v++ = seed;
        seed = (seed * 1103515245 + 12345) % (0x80000000L);
    }
    return 0;
}
