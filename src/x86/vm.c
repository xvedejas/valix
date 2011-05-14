#include <vm.h>
#include <threading.h>
#include <mm.h>
#include <data.h>
#include <string.h>
#include <types.h>
#include <stdarg.h>

void vmInstall()
{	
	return;
}

void runtimeRequire(bool value, String message)
{
	if (unlikely(value))
		printf(message);
}

/* Bytecode is not null-terminated! it is terminated with EOS and contains
 * multiple null characters */
ThreadFunc execute(u8 *bytecode)
{
	u8 *IP = bytecode;
    while (true)
    {
		printf("%x %c\n", *IP, *IP); // print the byte we're looking at
        switch (*IP++)
        {
            case 0x80: /* Import */
            {
                panic("Not implemented!");
            } break;
            case 0x81: /* String */
            {
                panic("Not implemented!");
            } break;
            case 0x82: /* Number */
            {
                panic("Not implemented!");
            } break;
            case 0x83: /* Keyword */
            {
				panic("Not implemented!");
            } break;
            case 0x84: /* Call Method */
            {
                panic("Not implemented!");
            } break;
            case 0x85: /* Begin List */
            {
                panic("Not implemented!");
            } break;
            case 0x86: /* Begin Procedure */
            {
                panic("Not implemented!");
            } break;
            case 0x87:
            {
                panic("Not implemented!");
            } break;
            case 0x88:
            {
                panic("Not implemented!");
            } break;
            case 0x89: /* End Statement */
            {
				// todo: clear value stack
				panic("Not implemented!");
            } break;
            case 0x8A:
            {
                panic("Not implemented!");
            } break;
            case 0x8B: /* Equality */
            {
				panic("Not implemented!");
            } break;
            case 0x8C: /* Binary method */
            {
				panic("Not implemented!");
            } break;
            case 0xFF: /* EOS */
            {
                printf("Execution done! Exiting thread.");
                endThread();
            } break;
            default:
            {
				runtimeRequire(false, "Execution error: Malformed bytecode.");
            } break;
        }
    }
}
