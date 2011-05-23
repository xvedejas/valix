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
 *      Xander Vedėjas <xvedejas@gmail.com>
 */
#ifndef __acpi_h__
#define __acpi_h__
#include <main.h>

#define RSDPFirstHalfSize (8+1+6+1+4)
#define RSDPTotalSize (sizeof(RSDP))

typedef struct
{
    char signature[4];
    u32  length;
    u8   revision;
    u8   checksum;
    char oemID[6];
    char oemTableID[8];
    u32  oemRevision;
    u32  creatorID;
    u32  creatorRevision;
} ACPISDTHeader;

typedef struct
{
  u8 AddressSpace;
  u8 BitWidth;
  u8 BitOffset;
  u8 AccessSize;
  u64 Address;
} GenericAddressStructure;

typedef struct
{
    ACPISDTHeader header;
    u32 firmwareCtrl;
    u32 dsdt;
 
    // field used in ACPI 1.0; no longer in use, for compatibility only
    u8  reserved;
 
    u8  preferredPowerManagementProfile;
    u16 sciInterrupt;
    u32 smiCommandPort;
    u8  acpiEnable;
    u8  acpiDisable;
    u8  s4BIOSREQ;
    u8  pSTATEControl;
    u32 pm1aEventBlock;
    u32 pm1bEventBlock;
    u32 pm1aControlBlock;
    u32 pm1bControlBlock;
    u32 pm2ControlBlock;
    u32 pmTimerBlock;
    u32 gpe0Block;
    u32 gpe1Block;
    u8  pm1EventLength;
    u8  pm1ControlLength;
    u8  pm2ControlLength;
    u8  pmTimerLength;
    u8  gpe0Length;
    u8  gpe1Length;
    u8  gpe1Base;
    u8  cStateControl;
    u16 worstC2Latency;
    u16 worstC3Latency;
    u16 flushSize;
    u16 flushStride;
    u8  dutyOffset;
    u8  dutyWidth;
    u8  dayAlarm;
    u8  monthAlarm;
    u8  century;
 
    // reserved in ACPI 1.0; used since ACPI 2.0+
    u16 bootArchitectureFlags;
 
    u8  reserved2;
    u32 flags;
 
    // 12 byte structure; see below for details
    GenericAddressStructure resetReg;
 
    u8  resetValue;
    u8  reserved3[3];
 
    // 64bit pointers - Available on ACPI 2.0+
    u64 xFirmwareControl;
    u64 xDsdt;
 
    GenericAddressStructure xPM1aEventBlock;
    GenericAddressStructure xPM1bEventBlock;
    GenericAddressStructure xPM1aControlBlock;
    GenericAddressStructure xPM1bControlBlock;
    GenericAddressStructure xPM2ControlBlock;
    GenericAddressStructure xPMTimerBlock;
    GenericAddressStructure xGPE0Block;
    GenericAddressStructure xGPE1Block;
} FADT;

typedef struct
{
    ACPISDTHeader header;
    void *otherHeaders[];
} RSDT;

typedef struct
{
    char signature[8];
    u8   checksum;
    char oemID[6];
    u8   revision;
    RSDT *rsdt;
    /* 2.0 Only below */
    u32  length;
    u64  xsdtAddress; // used for 64-bit systems
    u8   extendedChecksum;
    u8   reserved[3];
} RSDP;

RSDP *rsdp;
RSDT *rsdt;
FADT *fadt;

void acpiInstall();

#endif
