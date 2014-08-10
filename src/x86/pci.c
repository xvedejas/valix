/*  Copyright (C) 2014 Xander Vedejas <xvedejas@gmail.com>
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
 *      Tim Sarbin <tim.sarbin@gmail.com>
 */
 #include <pci.h>
 #include <main.h>
 #include <mm.h>
 
const u16 pciConfigAddressPort = 0x0CF8;
const u16 pciConfigDataPort    = 0x0CFC;
const u16 pciInvalidVendorId   = 0xFFFF;

u8 pciConfigReadByte(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) |
        (offset & 0xFC) | ((u32)0x80000000);
    outd(pciConfigAddressPort, address);
    return (u8)((ind(pciConfigDataPort) >> ((offset & 3) << 3)) & 0xFF);
}


u16 pciConfigReadWord(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) |
        (offset & 0xFC) | ((u32)0x80000000);
    outd(pciConfigAddressPort, address);
    return (u16)((ind(pciConfigDataPort) >> ((offset & 2) << 3)) & 0xFFFF);
}

u32 pciConfigReadDword(u8 bus, u8 slot, u8 func, u8 offset)
{
    u32 address = ((u32)bus << 16) | ((u32)slot << 11) | ((u32)func << 8) |
        (offset & 0xFC) | ((u32)0x80000000);
    outd(pciConfigAddressPort, (u32)address);
    return (u32)ind(pciConfigDataPort);
}


bool pciCheckDeviceExists(u8 bus, u8 slot, u8 func)
{
    return (pciConfigReadWord(bus, slot, func, 0) != 0xFFFF);
}
 
PciConfigHeaderBasic pciGetBasicConfigHeader(u8 bus, u8 slot, u8 func)
{
    PciConfigHeaderBasic pciHeader;
    pciHeader.bus                       = bus;
    pciHeader.dev                       = slot;
    pciHeader.func                      = func;
    pciHeader.vendorId                  = pciConfigReadWord(bus, slot, func, 0x00);
    pciHeader.deviceId                  = pciConfigReadWord(bus, slot, func, 0x02);
    pciHeader.command                   = pciConfigReadWord(bus, slot, func, 0x04);
    pciHeader.status                    = pciConfigReadWord(bus, slot, func, 0x06);
    pciHeader.revisionId                = pciConfigReadByte(bus, slot, func, 0x08);
    pciHeader.classProgrammingInterface = pciConfigReadByte(bus, slot, func, 0x09);
    pciHeader.classSubclass             = pciConfigReadByte(bus, slot, func, 0x0A);
    pciHeader.classBase                 = pciConfigReadByte(bus, slot, func, 0x0B);
    pciHeader.headerType                = pciConfigReadByte(bus, slot, func, 0x0E);
    pciHeader.baseAddr[0]               = pciConfigReadDword(bus, slot, func, 0x10);
    pciHeader.baseAddr[1]               = pciConfigReadDword(bus, slot, func, 0x14);
    pciHeader.baseAddr[2]               = pciConfigReadDword(bus, slot, func, 0x18);
    pciHeader.baseAddr[3]               = pciConfigReadDword(bus, slot, func, 0x1C);
    pciHeader.baseAddr[4]               = pciConfigReadDword(bus, slot, func, 0x20);
    pciHeader.baseAddr[5]               = pciConfigReadDword(bus, slot, func, 0x24);
    pciHeader.interruptLine             = pciConfigReadByte(bus, slot, func, 0x3C);
    return pciHeader;
}

u32 pciGetBar(u8 bus, u8 slot, u8 func, u8 bar)
{
    if (unlikely((u8)bar > 5))
        return 0;
    return pciConfigReadDword(bus, slot, func, 0x10 + ((u8)bar * 4));
}

PciConfigCompatDeviceList pciGetCompatibleDevices(u8 classcode)
{
    PciConfigCompatDeviceList devlist;

    devlist.count = 0;
    u16 bus, dev, func;
    
    // Get the count first
    for (bus = 0; bus < 256; bus++)
    {
        for (dev = 0; dev < 32; dev++)
        {
            for (func = 0; func < 8; func++)
            {
                if (likely(!pciCheckDeviceExists(bus, dev, func)))
                    continue;

                PciConfigHeaderBasic header = pciGetBasicConfigHeader(bus, dev, func);
                if (unlikely(header.classBase == classcode))
                    devlist.count++;
            }
        }
    }
    
    devlist.basicHeaders = malloc(sizeof(PciConfigHeaderBasic) * devlist.count);
    u32 i = 0;

    for (bus = 0; bus < 256; bus++)
    {
        for (dev = 0; dev < 32; dev++)
        {
            for (func = 0; func < 8; func++)
            {
                if (likely(!pciCheckDeviceExists(bus, dev, func)))
                    continue;

                PciConfigHeaderBasic header = pciGetBasicConfigHeader(bus, dev, func);
                if (likely(header.classBase != classcode))
                    continue;
                    
                devlist.basicHeaders[i] = header;
                i++;
                if (unlikely(i > devlist.count))
                    return devlist;
            }
        }
    }

    return devlist;
}

PciConfigCompatDeviceList pciGetDevices(u16 vendorId, u16 deviceId)
{
    PciConfigCompatDeviceList devlist;

    devlist.count = 0;
    u16 bus, dev, func;
    
    // Get the count first
    for (bus = 0; bus < 256; bus++)
    {
        for (dev = 0; dev < 32; dev++)
        {
            for (func = 0; func < 8; func++)
            {
                if (likely(!pciCheckDeviceExists(bus, dev, func)))
                    continue;

                PciConfigHeaderBasic header = pciGetBasicConfigHeader(bus, dev, func);
                if (unlikely((header.vendorId == vendorId) && (header.deviceId == deviceId)))
                    devlist.count++;
            }
        }
    }
    
    devlist.basicHeaders = malloc(sizeof(PciConfigHeaderBasic) * devlist.count);
    u32 i = 0;

    for (bus = 0; bus < 256; bus++)
    {
        for (dev = 0; dev < 32; dev++)
        {
            for (func = 0; func < 8; func++)
            {
                if (likely(!pciCheckDeviceExists(bus, dev, func)))
                    continue;

                PciConfigHeaderBasic header = pciGetBasicConfigHeader(bus, dev, func);
                if (likely((header.vendorId != vendorId) || (header.deviceId != deviceId)))
                    continue;

                devlist.basicHeaders[i++] = header;
                if (unlikely(i > devlist.count))
                    return devlist;
            }
        }
    }

    return devlist;
}

u8Type pciGetBarType(u8 bus, u8 slot, u8 func, u8 bar)
{
    if (pciGetBar(bus, slot, func, bar) & 0x00000001)
        return pciIO;
    return pciMemory;
}

u32 pciGetBarValue(u8 bus, u8 slot, u8 func, u8 bar)
{
    u32 barv = pciGetBar(bus, slot, func, bar);
    if (barv & 0x00000001)
    {
        // I/O
        return barv & (~0x03);
    }
    else
    {
        // Memory
        return barv & (~0x0F);
    }
}

