#include <acpi.h>
#include <cstring.h>

bool acpiInstalled = false;

void acpiInstall()
{
    /* Detect the RSDP */
    // First, we must look in the first KB of EBDA
    String searchString = "RSD PTR "; // always found on 16 byte boundaries
    bool found = false;
    char *rsdpSearch = (char*)0x09FC00; // Generally starts at this mem location
    for (; rsdpSearch < (char*)0xA0000; rsdpSearch += 16) // Look until end of EBDA
    {
        if (strncmp(rsdpSearch, searchString, 8) == 0)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        rsdpSearch = (char*)0xE0000; // now we need to look here
        for (; rsdpSearch < (char*)0x100000; rsdpSearch += 16)
        {
            if (strncmp(rsdpSearch, searchString, 8) == 0)
            {
                found = true;
                break;
            }
        }
        if (!found) // still not found!
        {
            printf("ACPI Signature not found!\n");
            return; // could not figure if acpi is installed
        }
    }
    printf("ACPI Signature found at %x\n", rsdpSearch);
    rsdp = (RSDP*)rsdpSearch;
    
    // Now we validate the RSDP. The first half should add up to 0,
    // byte-wise. The second half, if we have version 2.0, should also
    // add up to 0.
    
    u8 sum = 0;
    Size i = 0;
    for (; i < RSDPFirstHalfSize; i++)
        sum += rsdpSearch[i];
    if (sum != 0)
    {
        printf("ACPI Checksum failed\n");
        return;
    }
    
    for (; i < RSDPTotalSize; i++)
        sum += rsdpSearch[i];
    
    printf("L1: %x L2: %x\n", rsdp->length, RSDPTotalSize);
    
    if (sum != 0 || rsdp->length != RSDPTotalSize)
        printf("ACPI version 1.0 only\n");
    
    printf("ACPI checksum success!\n");
    
    // checksum the RSDT
    rsdt = rsdp->rsdt;
    
    for (sum = 0, i = 0; i < rsdt->header.length; i++)
        sum += ((char*)rsdt)[i];
    
    if (sum != 0)
    {
        printf("RSDT checksum failed\n");
        return;
    }
    printf("RSDT checksum success\n");
    
    // Now we have the RSDT so let's find the FADT with signature "FACP"
    bool foundFADT = false;
    for (i = 0; i < (rsdt->header.length - sizeof(ACPISDTHeader)) / sizeof(ACPISDTHeader*); i++)
    {
        if (strncmp(((ACPISDTHeader*)rsdt->otherHeaders[i])->signature, "FACP", 4) == 0)
        {
            printf("Found FADT\n");
            foundFADT = true;
            fadt = (FADT*)rsdt->otherHeaders[i];
            break;
        }
    }
    if (!foundFADT)
    {
        printf("FADT not found\n");
        return;
    }
    
    // checksum the FADT; the entire struct sums to zero
    
    for (sum = 0, i = 0; i < fadt->header.length; i++)
        sum += ((char*)rsdt)[i];
    
    if (sum != 0)
    {
        printf("FADT checksum failed\n");
        return;
    }
    printf("FADT checksum success\n");
    
    // Find and see if ACPI is already enabled
    
    if (fadt->smiCommandPort == 0 && fadt->acpiEnable == 0 && fadt->acpiDisable == 0)
    {
        printf("ACPI Already Enabled\n");
    }
    else
    {
        // Enable ACPI
        outb(fadt->smiCommandPort, fadt->acpiEnable);
        printf("ACPI Enabled\n");
    }
    
    // next we'll want to parse the dsdt
    
    acpiInstalled = true;
}
