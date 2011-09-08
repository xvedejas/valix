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
#include <main.h>
#include <types.h>

static int maxExponent = 511;   /* Largest possible base 10 exponent.  Any
                                 * exponent larger than this will already
                                 * produce underflow or overflow, so there's
                                 * no need to worry about additional digits.
                                 */

static double powersOf10[] = {   /* Table giving binary powers of 10.  Entry */
    10.,                         /* is 10^2^i.  Used to convert decimal */
    100.,                        /* exponents into floating-point numbers. */
    1.0e4,
    1.0e8,
    1.0e16,
    1.0e32,
    1.0e64,
    1.0e128,
    1.0e256
};

static char cvtIn[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,           /* '0' - '9' */
    100, 100, 100, 100, 100, 100, 100,      /* punctuation */
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, /* 'A' - 'Z' */
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35,
    100, 100, 100, 100, 100, 100,           /* punctuation */
    10, 11, 12, 13, 14, 15, 16, 17, 18, 19, /* 'a' - 'z' */
    20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
    30, 31, 32, 33, 34, 35};

static char numerals[16] = "0123456789ABCDEF";

long strtol(char *nptr, char **endptr, int base)
{
    long result;
    char *p = nptr;

    while (isspace(*p))
        p++;
        
    if (*p == '-')
    {
        p++;
        result = -strtoul(p, endptr, base);
    }
    else
    {
        if (*p == '+')
            p++;
        result = strtoul(p, endptr, base);
    }
    if (endptr != 0 && *endptr == p)
        *endptr = nptr;
    
    return result;
}

double strtod(const char *String, char **endPtr)
{
    bool sign, expSign = false;
    double fraction, dblExp, *d;
    register const char *p;
    register int c;
    int exp = 0;
    int fracExp = 0;
    int mantsize;
    int decPt;
    const char *pExp;
    p = String;
    while (isspace(*p))
        p++;
    
    if (*p == '-')
    {
        sign = true;
        p += 1;
    }
    else
    {
        if (*p == '+')
            p += 1;
        sign = false;
    }

    decPt = -1;
    for (mantsize = 0; ; mantsize += 1)
    {
        c = *p;
        if (!isdigit(c))
        {
            if ((c != '.') || (decPt >= 0))
                break;
            decPt = mantsize;
        }
        p += 1;
    }

    pExp  = p;
    p -= mantsize;

    if (decPt < 0)
        decPt = mantsize;
    else
        mantsize -= 1;
        
    if (mantsize > 18)
    {
        fracExp = decPt - 18;
        mantsize = 18;
    }
    else
        fracExp = decPt - mantsize;
    
    if (mantsize == 0)
    {
        fraction = 0.0;
        p = String;
    }
    else
    {
        int frac1, frac2;
        for (frac1 = 0; mantsize > 9; mantsize -= 1)
        {
            c = *p;
            p += 1;
            if (c == '.')
            {
                c = *p;
                p += 1;
            }
            frac1 = 10*frac1 + (c - '0');
        }
        for (frac2 = 0; mantsize > 0; mantsize -= 1)
        {
            c = *p;
            p += 1;
            if (c == '.')
            {
                c = *p;
                p += 1;
            }
            frac2 = 10*frac2 + (c - '0');
        }
        fraction = (1.0e9 * frac1) + frac2;
        
        p = pExp;
        if ((*p == 'E') || (*p == 'e'))
        {
            p += 1;
            if (*p == '-')
            {
                expSign = true;
                p += 1;
            } else {
                if (*p == '+')
                    p += 1;
                expSign = false;
            }
            while (isdigit(*p))
            {
                exp = exp * 10 + (*p - '0');
                p += 1;
            }
        }
        
        if (expSign)
            exp = fracExp - exp;
        else
            exp = fracExp + exp;
        
        if (exp < 0)
        {
            expSign = true;
            exp = -exp;
        }
        else
            expSign = false;

        if (exp > maxExponent)
        {
            exp = maxExponent;
            //errno = 0 /* ERANGE */;
        }
        
        dblExp = 1.0;
        
        for (d = powersOf10; exp != 0; exp >>= 1, d += 1)
        {
            if (exp & 01)
                dblExp *= *d;
        }
        
        if (expSign)
            fraction /= dblExp;
        else
            fraction *= dblExp;
    }
    
    if (endPtr != NULL)
    *endPtr = (char *) p;

    if (sign)
        return -fraction;
    else
        return fraction;
}

unsigned long int strtoul(String string, char **endPtr, int base)
{
    register char *p;
    register unsigned long int result = 0;
    register unsigned digit;
    int anyDigits = 0;

    p = string;
    while (isspace(*p))
        p += 1;

    if (base == 0)
    {
        if (*p == '0')
        {
            p += 1;
            if (*p == 'x')
            {
                p += 1;
                base = 16;
            }
            else
            {
                anyDigits = 1;
                base = 8;
            }
        }
        else base = 10;
    }
    else if (base == 16)
    {
        if ((p[0] == '0') && (p[1] == 'x'))
            p += 2;
    }

    if (base == 8)
    {
        for ( ; ; p += 1)
        {
            digit = *p - '0';
            if (digit > 7)
                break;
            result = (result << 3) + digit;
            anyDigits = 1;
        }
    }
    else if (base == 10)
    {
        for ( ; ; p += 1)
        {
            digit = *p - '0';
            if (digit > 9)
                break;
            result = (10*result) + digit;
            anyDigits = 1;
        }
    }
    else if (base == 16)
    {
        for ( ; ; p += 1)
        {
            digit = *p - '0';
            if (digit > ('z' - '0'))
                break;
            digit = cvtIn[digit];
            if (digit > 15)
                break;
            result = (result << 4) + digit;
            anyDigits = 1;
        }
    }
    else
    {
        for ( ; ; p += 1)
        {
            digit = *p - '0';
            if (digit > ('z' - '0'))
                break;
            digit = cvtIn[digit];
            if (digit >= base)
                break;
            result = result*base + digit;
            anyDigits = 1;
        }
    }

    if (!anyDigits)
        p = string;

    if (endPtr != 0)
        *endPtr = p;

    return result;
}

double atof(const char *nptr)
{
    return (strtod(nptr, NULL));
}


char *itoa(Size input, char *buffer, Size radix)
{
    Size t = input;
    Size i = 1;
    while ((t /= radix) > 0) i++;
    buffer[i] = 0;
    do
    {
        i--;
        buffer[i] = numerals[input % radix];
        input /= radix;
    } while (i);
    return buffer;
}
