#include <vm.h>
#include <data.h>

typedef struct frame
{
    Map *locals;
    struct frame *previous;
} VMFrame;

void vmInstall()
{
    return;
}

void execute(String bytecode)
{
    return;
}
