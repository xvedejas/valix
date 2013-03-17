#include <nic.h>

const u16 realtekVendorID = 0x10EC;
const u16 rtl8139DeviceID = 0x8139;

NetworkInterfaceController *getNetworkInterfaceController(u32 interrupt)
{
    return &(networkInterfaceControllers[interrupt - 9]);
}
