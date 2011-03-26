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

Size chrcount(String s, char c)
{
    int i = 0;
    while (*s++)
       	if (*s == c)
            i++;
    return i;
}

String strchr(String s, char c)
{
    for(; *s != c; s++)
        if (*s == '\0')
            return NULL;
    return (String)s;
}

String strncpy(String dest, String src, Size count)
{
    String tmp = dest;
    while (count-- && (*dest++ = *src++) != '\0');
    return tmp;
}

String strncat(String dest, String src, Size count)
{
    String tmp = dest;
    if (count)
    {
     	while (*dest)
            dest++;
        while ((*dest++ = *src++))
        {
            if (--count == 0)
            {
             	*dest = '\0';
                break;
            }
        }
    }

    return tmp;
}
