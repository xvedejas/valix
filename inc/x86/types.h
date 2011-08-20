/*  Copyright (C) 2011 Xander Vedejas <xvedejas@gmail.com>
 *  Conversion functions Copyright 1988 Regents of the University of California
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
 */

#ifndef __types_h__
#define __types_h__

#define isalnum(c)  ({ typeof(c) _c = c; isalpha(_c) || isdigit(_c); })
#define isalpha(c)  ({ typeof(c) _c = c; isupper(_c) || islower(_c); }) 
#define isascii(c)  ({ typeof(c) _c = c; _c > 0 && _c <= 0x7f; })
#define iscntrl(c)  ({ typeof(c) _c = c; ((_c >= 0) && ((_c <= 0x1F) || (_c == 0x7f))); })
#define isdigit(c)  ({ typeof(c) _c = c; (_c >= '0' && _c <= '9'); })
#define isgraph(c)  ({ typeof(c) _c = c; (_c != ' ' && isprint(_c)); })
#define islower(c)  ({ typeof(c) _c = c; (_c >=  'a' && _c <= 'z'); })
#define isprint(c)  ({ typeof(c) _c = c; (_c >= ' ' && _c <= '~'); })
#define ispunct(c)  ({ typeof(c) _c = c; ((_c > ' ' && _c <= '~') && !isalnum(_c)); })
#define isspace(c)  ({ typeof(c) _c = c; (_c ==  ' ' || _c == '\f' || _c == '\n' || _c == '\r' || _c == '\t' || _c == '\v'); })
#define isupper(c)  ({ typeof(c) _c = c; (_c >=  'A' && _c <= 'Z'); })
#define isxdigit(c) ({ typeof(c) _c = c; (isxupper(_c) || isxlower(_c)); }) 
#define isxlower(c) ({ typeof(c) _c = c; (isdigit(_c) || (_c >= 'a' && _c <= 'f')); })
#define isxupper(c) ({ typeof(c) _c = c; (isdigit(_c) || (_c >= 'A' && _c <= 'F')); })
#define tolower(c)  ({ typeof(c) _c = c; (isupper(_c) ? (_c - 'A' + 'a') : (_c)); })
#define toupper(c)  ({ typeof(c) _c = c; (islower(_c) ? (_c - 'a' + 'A') : (_c)); })


char *itoa(Size input, char *buffer, Size radix);
// atoi
// atol
double strtod(const char *String, char **endPtr);
long strtol(char *nptr, char **endptr, int base);
unsigned long int strtoul(char *String, char **endPtr, int base);
void *sort(void *tbs);
// strtoll
// strtoul

#endif
