/*  Copyright (C) 2010 Xander Vedejas <xvedejas@gmail.com>
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

#ifndef _pci_h_
#define _pci_h_

#include <main.h>

const u16 pciConfigAddressPort;
const u16 pciConfigDataPort;
const u16 pciInvalidVendorId;

typedef enum
{
    classcodeObsolete =                                   0x00,
    classcodeMassStorageController =                      0x01,
    classcodeNetworkController =                          0x02,
    classcodeDisplayController =                          0x03,
    classcodeMultimediaDevice =                           0x04,
    classcodeMemoryController =                           0x05,
    classcodeBridgeDevice =                               0x06,
    classcodeSimpleCommunicationControllers =             0x07,
    classcodeBaseSystemPeripherals =                      0x08,
    classcodeInputDevices =                               0x09,
    classcodeDockingStations =                            0x0A,
    classcodeProcessors =                                 0x0B,
    classcodeSerialBusControllers =                       0x0C,
    classcodeWirelessController =                         0x0D,
    classcodeIntelligentIoControllers =                   0x0E,
    classcodeSatelliteCommunicationControllers =          0x0F,
    classcodeEncryptionDecryptionControllers =            0X10,
    classcodeDataAcquisitionSignalProcessingControllers = 0x11
} pci_classcodes;

typedef enum
{
    pciMemory = 0,
    pciIO
} u8Type;

typedef void* pciConfigSpaceHeader;

typedef struct pciConfigHeaderBasic
{
    u8 bus, dev, func;
    u16 vendorId;
    u16 deviceId;
    u16 command;
    u16 status;
    u8 revisionId;
    u8 classBase;
    u8 classSubclass;
    u8 classProgrammingInterface;
    u8 headerType;
} PciConfigHeaderBasic;

typedef struct pciConfigCompatDeviceList
{
    u32 count;
    PciConfigHeaderBasic *basicHeaders;
} PciConfigCompatDeviceList;

u8 pciConfigReadByte(u8 bus, u8 slot, u8 func, u8 offset);
u16 pciConfigReadWord(u8 bus, u8 slot, u8 func, u8 offset);
u32 pciConfigReadDword(u8 bus, u8 slot, u8 func, u8 offset);
bool pciCheckDeviceExists(u8 bus, u8 slot, u8 func);
PciConfigHeaderBasic pciGetBasicConfigHeader(u8 bus, u8 slot, u8 func);
PciConfigCompatDeviceList pciGetCompatibleDevices(u8 classcode);
PciConfigCompatDeviceList pciGetDevices(u16 vendorId, u16 deviceId);
u32 pciGetBar(u8 bus, u8 slot, u8 func, u8 bar);
u8Type pciGetBarType(u8 bus, u8 slot, u8 func, u8 bar);
u32 pciGetBarValue(u8 bus, u8 slot, u8 func, u8 bar);

#endif

