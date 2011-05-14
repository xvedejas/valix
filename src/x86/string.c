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

u32 strncmp(String cs, String ct, Size n)
{
	register signed char __res;
	Size i = 0;
    while (i++ < n)
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

Size strlen(String s)
{
    String sc;
    for (sc = s; *sc != '\0'; ++sc);
    return sc - s;
}

void *memmove(void * dest, const void *src, Size count)
{
    char *tmp, *s;

    if (dest <= src)
    {
     	tmp = (char *) dest;
        s = (char *) src;
        while (count--)
            *tmp++ = *s++;
    }
    else
    {
     	tmp = (char *) dest + count;
        s = (char *) src + count;
        while (count--)
            *--tmp = *--s;
    }

    return dest;
}
