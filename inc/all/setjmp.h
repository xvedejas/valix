 /*  Copyright (C) 2012 Xander Vedejas <xvedejas@gmail.com>
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
 *      Xander Vedėjas <xvedejas@gmail.com>
 */
#ifndef __setjmp_h__
#define __setjmp_h__

typedef void *jmp_buf[6];

//extern bool __attribute__((fastcall)) setjmp(jmp_buf env);
//extern void __attribute__((fastcall)) longjmp(jmp_buf env, bool val);

#define jmpBufDebug(env) \
    printf("JMP BUF:\n%x\n%x\n%x\n%x\n%x\n%x\n----\n", \
        env[0], env[1], env[2], env[3], env[4], env[5]);

#define setjmp(env) __builtin_setjmp(env)
#define longjmp(env, val) __builtin_longjmp(env, val)

#endif


