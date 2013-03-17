#include <rtl8139.h>
#include <threading.h>
#include <interrupts.h>
#include <cstring.h>

/* This driver is for the RTL network chip, specifically the rtl8139 (it may
 * support other similar cards in the future). */

/* PCI Tuning Parameters
   Threshold is bytes transferred to chip before transmission starts. */
#define txFifoTreshhold 256      /* In bytes, rounded down to 32 byte units. */
#define rxFifoThreshhold  4       /* Rx buffer level before first PCI xfer.  */
#define rxDmaBurst    4       /* Maximum PCI burst, '4' is 256 bytes */
#define txDmaBurst    4       /* Calculate as 16<<val. */
#define numTxDesc     4       /* Number of Tx descriptor registers. */
#define txBufferSize	ethernetFrameLength	/* FCS is added by the chip */
#define rxBufferLengthIDX 0	/* 0, 1, 2 is allowed - 8,16,32K rx buffer */
#define rxBufferLength (8192 << rxBufferLengthIDX)
#define txLoopback (bit(17) | bit(18))

/* Symbolic offsets to registers. */
enum RTL8139_registers {
	MAC0 = 0,			/* Ethernet hardware address. */
	MAR0 = 8,			/* Multicast filter. */
	TxStatus0 = 0x10,		/* Transmit status (four 32bit registers). */
	TxAddr0 = 0x20,		/* Tx descriptors (also four 32bit). */
	RxBuf = 0x30, RxEarlyCnt = 0x34, RxEarlyStatus = 0x36,
	ChipCmd = 0x37, RxBufPtr = 0x38, RxBufAddr = 0x3A,
	IntrMask = 0x3C, IntrStatus = 0x3E,
	TxConfig = 0x40, RxConfig = 0x44,
	timer = 0x48,		/* general-purpose counter. */
	RxMissed = 0x4C,		/* 24 bits valid, write clears. */
	Cfg9346 = 0x50, Config0 = 0x51, Config1 = 0x52,
	TimerIntrReg = 0x54,	/* intr if gp counter reaches this value */
	MediaStatus = 0x58,
	Config3 = 0x59,
	MultiIntr = 0x5C,
	RevisionID = 0x5E,	/* revision of the RTL8139 chip */
	TxSummary = 0x60,
	MII_BMCR = 0x62, MII_BMSR = 0x64, NWayAdvert = 0x66, NWayLPAR = 0x68,
	NWayExpansion = 0x6A,
	DisconnectCnt = 0x6C, FalseCarrierCnt = 0x6E,
	NWayTestReg = 0x70,
	RxCnt = 0x72,		/* packet received counter */
	CSCR = 0x74,		/* chip status and configuration register */
	PhyParm1 = 0x78, TwisterParm = 0x7c, PhyParm2 = 0x80,	/* undocumented */
	/* from 0x84 onwards are a number of power management/wakeup frame
	 * definitions we will probably never need to know about.  */
};

enum ChipCmdBits {
	CmdReset = 0x10, CmdRxEnb = 0x08, CmdTxEnb = 0x04, RxBufEmpty = 0x01, };

/* Interrupt register bits, using my own meaningful names. */
enum IntrStatusBits {
	PCIErr = 0x8000, PCSTimeout = 0x4000, CableLenChange =  0x2000,
	RxFIFOOver = 0x40, RxUnderrun = 0x20, RxOverflow = 0x10,
	TxErr = 0x08, TxOK = 0x04, RxErr = 0x02, RxOK = 0x01,
};
enum TxStatusBits {
	TxHostOwns = 0x2000, TxUnderrun = 0x4000, TxStatOK = 0x8000,
	TxOutOfWindow = 0x20000000, TxAborted = 0x40000000,
	TxCarrierLost = 0x80000000,
};
enum RxStatusBits {
	RxMulticast = 0x8000, RxPhysical = 0x4000, RxBroadcast = 0x2000,
	RxBadSymbol = 0x0020, RxRunt = 0x0010, RxTooLong = 0x0008, RxCRCErr = 0x0004,
	RxBadAlign = 0x0002, RxStatusOK = 0x0001,
};

enum MediaStatusBits {
	MSRTxFlowEnable = 0x80, MSRRxFlowEnable = 0x40, MSRSpeed10 = 0x08,
	MSRLinkFail = 0x04, MSRRxPauseFlag = 0x02, MSRTxPauseFlag = 0x01,
};

enum MIIBMCRBits {
	BMCRReset = 0x8000, BMCRSpeed100 = 0x2000, BMCRNWayEnable = 0x1000,
	BMCRRestartNWay = 0x0200, BMCRDuplex = 0x0100,
};

enum CSCRBits {
	CSCR_LinkOKBit = 0x0400, CSCR_LinkChangeBit = 0x0800,
	CSCR_LinkStatusBits = 0x0f000, CSCR_LinkDownOffCmd = 0x003c0,
	CSCR_LinkDownCmd = 0x0f3c0,
};

/* Bits in RxConfig. */
enum rx_mode_bits {
	RxCfgWrap = 0x80,
	AcceptErr = 0x20, AcceptRunt = 0x10, AcceptBroadcast = 0x08,
	AcceptMulticast = 0x04, AcceptMyPhys = 0x02, AcceptAllPhys = 0x01,
};

/* The RTL8139 can only transmit from a contiguous, aligned memory block. 
 * move this to a NIC struct */
//static u8 tx_buffer[txBufferSize] __attribute__((aligned(4)));

//static u8 rx_ring[rxBufferLength+16] __attribute__((aligned(4)));

void handler(Regs *r)
{
    /* We should be able to identify which NIC this interrupt came from by
     * which interrupt number it was */
    
    NetworkInterfaceController *nic = getNetworkInterfaceController(r->intNo);
    
    u16 base = nic->baseAddress;
    
    printf("RECEIVED ETHERNET INTERRUPT ON %x\n", r->intNo);
    /* Read ISR (interrupt status register) at 0x3E to find the source of the interrupt */
    
    u8 status = inb(base + IntrStatus);
    
    if (status & bit(0)) // RX OK
    {
        printf("RX OK interrupt\n");
    }
    else if (status & bit(2)) // TX OK
    {
        printf("TX OK interrupt\n");
    }
    else
        panic("huh?");
}

void rtlTransmit(NetworkInterfaceController *nic)
{
}

// Start the hardware at open or resume
void rtlStart(NetworkInterfaceController *nic)
{
    u16 base = nic->baseAddress;

	/* Send 0x00 to the CONFIG_1 register (0x52) to set the LWAKE + LWPTN to
     * active high. this should essentially *power on* the device. */
    outd(base + Cfg9346, 0x00);
	outb(base + Config1, 0x00);
    
    /* Next, we should do a software reset to clear the RX and TX buffers and
     * set everything back to defaults. Do this to eliminate the possibility of
     * there still being garbage left in the buffers or registers on power on.
     * Sending 0x10 to the Command register (0x37) will send the RTL8139 into a
     * software reset. Once that byte is sent, the RST bit must be checked to
     * make sure that the chip has finished the reset. If the RST bit is high,
     * then the reset is still in operation.
     * There is a minor bug in Qemu. If you check the command register before
     * performing a soft reset, you may find the RST bit is high (1). Just
     * ignore it and carry on with the initialisation procedure. */
    
    outb(base + ChipCmd, CmdReset);
    // Check that the chip has finished the reset
    while (true)
    {
        /// todo: impose a timer in this loop
        if ((ind(base + ChipCmd) & CmdReset) == 0) break;
    }
    
    /// set MAC address here?
    //outd(base + Cfg9346, 0xC0);
    //outd(base + MAC0 + 0, *(unsigned long *)(tp->hwaddr.addr + 0));
    //outd(base + MAC0 + 4, *(unsigned long *)(tp->hwaddr.addr + 4));
    
    /* For this part, we will send the chip a memory location to use as its
     * receive buffer start location. One way to do it, would be to define a
     * buffer variable and send that variables memory location to the RBSTART
     * register (0x30). */
    
    char rx_buffer[8192 + 16 + 1500];
    outd(base + RxBuf, (u32)rx_buffer); // RBSTART

    /*  We want to put this in loopback mode, which means anything transmitted
     * is immediately received. Also set burst size. */
    outd(base + TxConfig, (txDmaBurst << 8) | txLoopback);
    
    /* Before hoping to see a packet coming to you, you should tell the RTL8139
     * to accept packets based on various rules. The configuration register is RCR. */
    outd(base + RxConfig, 0xF | (1 << 7));
    
    /* Enable RX and TX */
    outb(base + ChipCmd, CmdRxEnb | CmdTxEnb);
    outd(base + RxMissed, 0);

      /* The Interrupt Mask Register (IMR) and Interrupt Service Register (ISR)
     * are responsible for firing up different IRQs. The IMR bits line up with
     * the ISR bits to work in sync. If an IMR bit is low, then the
     * corresponding ISR bit with never fire an IRQ when the time comes for it
     * to happen. The IMR is located at 0x3C and the ISR is located at 0x3E. */

    // Enable interrupts by setting the interrupt mask. These will now fire an IRQ
    outw(base + IntrMask, 0x0005);

    printf("RTL8139 INITIALIZED\n");
}

NetworkInterfaceController *rtlInstall(PciConfigHeaderBasic *pci)
{
    u32 interrupt = pci->interruptLine;
    NetworkInterfaceController *nic = getNetworkInterfaceController(interrupt);
    nic->interrupt = interrupt;
    u16 base = pci->baseAddr[0] & ~0b11;
    nic->baseAddress = base;
    
    
    // Bring the chip out of low-power mode
    int config1 = ind(base + Config1);

    // Put the chip into low-power mode
    outd(base + Cfg9346, 0xC0);
    
    irqInstallHandler(interrupt, handler);
    
    return nic;
}
