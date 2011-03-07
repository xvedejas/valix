#include <string.h>

u32 strcmp(String cs, String ct)
{
	register signed char __res;
    while (true)
    {
        if ((__res = *cs - *ct++) != 0 || !*cs++)
            break;
    }
    return __res;
}

Size strlcpy(String dest, String src, Size count)
{
	register u32 i = 0;
    while (*src && i++ < count) *dest++ = *src++;
    *dest++ = 0;
    return i;
}
