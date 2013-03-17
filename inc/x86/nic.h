#include <main.h>

extern const u16 realtekVendorID;
extern const u16 rtl8139DeviceID;

typedef struct
{
    u32 interrupt;   // Interrupt used by device
    u16 baseAddress; // Typically the address of first port used
    u16 deviceID;
} NetworkInterfaceController;

/* We make the assumption that there can be at most one NIC per interrupt
 * available to NICs, which includes interrupts 0x09, 0x0A, and 0x0B. Here we
 * preallocate structs available for this purpose at the expense of binary size
 * but for the sake of simplicity. */
NetworkInterfaceController networkInterfaceControllers[3];
extern NetworkInterfaceController *getNetworkInterfaceController(u32 interrupt);
