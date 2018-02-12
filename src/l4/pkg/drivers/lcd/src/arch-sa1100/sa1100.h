/****************************************************************************/
/* Copyright 2000 Compaq Computer Corporation.                              */
/*                                           .                              */
/* Copying or modifying this code for any purpose is permitted,             */
/* provided that this copyright notice is preserved in its entirety         */
/* in all copies or modifications.  COMPAQ COMPUTER CORPORATION             */
/* MAKES NO WARRANTIES, EXPRESSED OR IMPLIED, AS TO THE USEFULNESS          */
/* OR CORRECTNESS OF THIS CODE OR ITS FITNESS FOR ANY PARTICULAR            */
/* PURPOSE.                                                                 */
/****************************************************************************/
#ifndef SA1100_H_INCLUDED
#define SA1100_H_INCLUDED


#define UTCR0     0x00
#define UTCR1     0x04
#define UTCR2     0x08
#define UTCR3     0x0C
#define UTCR4     0x10
#define UTDR      0x14
#define UTSR0     0x1c
#define UTSR1     0x20

#define UTCR0_PE		(1 << 0)   /* parity enable */
#define UTCR0_OES	(1 << 1)   /* 1 for even parity */
#define UTCR0_2STOP	(1 << 2)   /* 1 for 2 stop bits */
#define UTCR0_8BIT	(1 << 3)   /* 1 for 8 bit data */
#define UTCR0_SCE	(1 << 4)   /* sample clock enable */
#define UTCR0_RCE	(1 << 5)   /* receive clock edge select */
#define UTCR0_TCE	(1 << 6)   /* transmit clock edge select */

#define UTCR1_BRDHIMASK  0xF
#define UTCR2_BRDLoMASK  0xFF

#define UTCR3_RXE	(1 << 0)	/* receiver enable */
#define UTCR3_TXE	(1 << 1)	/* transmit enable */
#define UTCR3_BRK	(1 << 2)	/* send a BRK */
#define UTCR3_RIE	(1 << 3)	/* receive FIFO interrupt enable */
#define UTCR3_TIE	(1 << 4)	/* transmit FIFO interrupt enable */
#define UTCR3_LBM	(1 << 5)	/* loopback mode */

/* [1] 11.11.6 */
#define UTDR_PRE		(1 << 8)	/* parity error */
#define UTDR_FRE		(1 << 9)	/* framing error */
#define UTDR_ROR		(1 << 10)	/* receiver overrun */

/* [1] 11.11.7 */
#define UTSR0_TFS	0x00000001  /* transmit FIFO service request */
#define UTSR0_RFS	0x00000002	/* Receive FIFO 1/3-to-2/3-full or */
                                        /* more Service request (read)     */
#define UTSR0_RID	0x00000004	/* Receiver IDle                   */
#define UTSR0_RBB    0x00000008	/* Receive Beginning of Break      */
#define UTSR0_REB	0x00000010	/* Receive End of Break            */
#define UTSR0_EIF	0x00000020	/* Error In FIFO (read)            */

/* [1] 11.11.8 */
#define UTSR1_TBY	(1 << 0)	/* transmit FIFO busy */
#define UTSR1_RNE	(1 << 1)        /* receive FIFO not empty */
#define UTSR1_TNF	(1 << 2)        /* transmit FIFO not full */
#define UTSR1_PRE	(1 << 3)        /* parity error */          
#define UTSR1_FRE	(1 << 4)        /* framing error */         
#define UTSR1_ROR	(1 << 5)        /* receiver overrun */      

/* infrared */
#define HSCR0_ITR	0x00000001	/* IrDA Transmission Rate          */
#define HSCR0_UART	(HSCR0_ITR*0)	/*  UART mode (115.2 kb/s if IrDA) */
#define HSCR0_HSSP	(HSCR0_ITR*1)	/*  HSSP mode (4 Mb/s)             */

/* (HP-SIR) modulation Enable      */
#define UTCR4_HSE    0x00000001      /* Hewlett-Packard Serial InfraRed */
#define UTCR4_NRZ	(UTCR4_HSE*0)	/*  Non-Return to Zero modulation  */
#define UTCR4_HPSIR	(UTCR4_HSE*1)	/*  HP-SIR modulation              */
#define UTCR4_LPM	0x00000002	/* Low-Power Mode                  */
#define UTCR4_Z3_16Bit	(UTCR4_LPM*0)	/*  Zero pulse = 3/16 Bit time     */
#define UTCR4_Z1_6us	(UTCR4_LPM*1)	/*  Zero pulse = 1.6 us            */


#define UTSR1_ERROR_MASK 0x38

#define SDLCBASE		0x80020060
#define UART1BASE	0x80010000 
#define UART2BASE	0x80030000 
#define UART3BASE	0x80050000 

#define UART1_UTCR0      (UART1BASE + UTCR0)
#define UART1_UTCR1      (UART1BASE + UTCR1)
#define UART1_UTCR2      (UART1BASE + UTCR2)
#define UART1_UTCR3      (UART1BASE + UTCR3)
#define UART1_UTDR       (UART1BASE + UTDR)
#define UART1_UTSR0      (UART1BASE + UTSR0)
#define UART1_UTSR1      (UART1BASE + UTSR1)

#define UART2_UTCR0      (UART2BASE + UTCR0)
#define UART2_UTCR1      (UART2BASE + UTCR1)
#define UART2_UTCR2      (UART2BASE + UTCR2)
#define UART2_UTCR3      (UART2BASE + UTCR3)
/* the IR uart has an extra control register */
#define UART2_UTCR4      (UART2BASE + UTCR4)
#define UART2_UTDR       (UART2BASE + UTDR)
#define UART2_UTSR0      (UART2BASE + UTSR0)
#define UART2_UTSR1      (UART2BASE + UTSR1)

#define UART3_UTCR0      (UART3BASE + UTCR0)
#define UART3_UTCR1      (UART3BASE + UTCR1)
#define UART3_UTCR2      (UART3BASE + UTCR2)
#define UART3_UTCR3      (UART3BASE + UTCR3)
#define UART3_UTDR       (UART3BASE + UTDR)
#define UART3_UTSR0      (UART3BASE + UTSR0)
#define UART3_UTSR1      (UART3BASE + UTSR1)


/*
 * Operating System (OS) timer control registers
 *
 * Registers
 *    OSMR0     	Operating System (OS) timer Match Register 0
 *              	(read/write).
 *    OSMR1     	Operating System (OS) timer Match Register 1
 *              	(read/write).
 *    OSMR2     	Operating System (OS) timer Match Register 2
 *              	(read/write).
 *    OSMR3     	Operating System (OS) timer Match Register 3
 *              	(read/write).
 *    OSCR      	Operating System (OS) timer Counter Register
 *              	(read/write).
 *    OSSR      	Operating System (OS) timer Status Register
 *              	(read/write).
 *    OWER      	Operating System (OS) timer Watch-dog Enable Register
 *              	(read/write).
 *    OIER      	Operating System (OS) timer Interrupt Enable Register
 *              	(read/write).
 */

#define OSMR0  		*(long*)(0x90000000)  /* OS timer Match Reg. 0 */
#define OSMR1  		*(long*)(0x90000004)  /* OS timer Match Reg. 1 */
#define OSMR2  		*(long*)(0x90000008)  /* OS timer Match Reg. 2 */
#define OSMR3  		*(long*)(0x9000000c)  /* OS timer Match Reg. 3 */
#define OSCR   	*(volatile long*)(0x90000010)  /* OS timer Counter Reg. */
#define OSSR   	*(volatile long*)(0x90000014	)  /* OS timer Status Reg. */
#define OWER   	*(volatile long*)(0x90000018	)  /* OS timer Watch-dog Enable Reg. */
#define OIER   	*(volatile long*)(0x9000001C	)  /* OS timer Interrupt Enable Reg. */


/* 
 * DRAM Configuration values 
 */

/* [1] 10.2 */
#define DRAM_CONFIGURATION_BASE 0xA0000000

#define MDCNFG			0x00 /* must be initialized */
#define MDCAS00			0x04 /* must be initialized */
#define MDCAS01			0x08 /* must be initialized */
#define MDCAS02			0x0c /* must be initialized */

#define MSC0			0x10 /* must be initialized */
#define MSC1			0x14 /* must be initialized */
#define MECR			0x18 /* should be initialized */
#define MDREFR			0x1c /* must be initialized */

#define MDCAS20                  0x20 /* OK not to initialize this register, because the enable bits are cleared on reset */
#define MDCAS21                  0x24 /* OK not to initialize this register, because the enable bits are cleared on reset */
#define MDCAS22                  0x28 /* OK not to initialize this register, because the enable bits are cleared on reset */

#define MSC2                     0x2C /* must be initialized */

#define SMCNFG                   0x30 /* should be initialized */

#define ABS_MDCNFG *(volatile unsigned long*)(((char*)DRAM_CONFIGURATION_BASE)+MDCNFG)
#define ABS_MSC0 *(volatile unsigned long*)(((char*)DRAM_CONFIGURATION_BASE)+MSC0)
#define ABS_MSC1 *(volatile unsigned long*)(((char*)DRAM_CONFIGURATION_BASE)+MSC1)
#define ABS_MSC2 *(volatile unsigned long*)(((char*)DRAM_CONFIGURATION_BASE)+MSC2)

#define MDCNFG_BANK0_ENABLE (1 << 0)
#define MDCNFG_BANK1_ENABLE (1 << 1)
#define MDCNFG_DTIM0_SDRAM  (1 << 2)
#define MDCNFG_DWID0_32B    (0 << 3)
#define MDCNFG_DWID0_16B    (1 << 3)
#define MDCNFG_DRAC0(n_)    (((n_) & 7) << 4)
#define MDCNFG_TRP0(n_)     (((n_) & 0xF) << 8)
#define MDCNFG_TDL0(n_)     (((n_) & 3) << 12)
#define MDCNFG_TWR0(n_)     (((n_) & 3) << 14)
	
/* DRAM Refresh Control Register (MDREFR) [1] 10.2.2 */	
#define MDREFR_TRASR(n_)    (((n_) & 0xF) << 0)	
#define MDREFR_DRI(n_)    (((n_) & 0xFFF) << 4)
#define MDREFR_E0PIN		(1 << 16)
#define MDREFR_K0RUN		(1 << 17)
#define MDREFR_K0DB2		(1 << 18)
#define MDREFR_E1PIN		(1 << 20)	/* SDRAM clock enable pin 1 (banks 0-1) */
#define MDREFR_K1RUN		(1 << 21)	/* SDRAM clock pin 1 run (banks 0-1) */
#define MDREFR_K1DB2		(1 << 22)	/* SDRAM clock pin 1 divide-by-two (banks 0-1) */
#define MDREFR_K2RUN		(1 << 25)       /* SDRAM clock enable pin 2 (banks 2-3) */       
#define MDREFR_K2DB2		(1 << 26)       /* SDRAM clock pin 2 run (banks 2-3) */          
#define MDREFR_EAPD		(1 << 28)       /* SDRAM clock pin 2 divide-by-two (banks 2-3) */
#define MDREFR_KAPD		(1 << 29)
#define MDREFR_SLFRSH		(1 << 31)	/* SDRAM self refresh */


#define MSC_RT_ROMFLASH         0
#define MSC_RT_SRAM_012         1
#define MSC_RT_VARLAT_345       1
#define MSC_RT_BURST4           2
#define MSC_RT_BURST8           3

#define MSC_RBW32               (0 << 2)
#define MSC_RBW16               (1 << 2)

#define MSC_RDF(n_)             (((n_)&0x1f)<<3)
#define MSC_RDN(n_)             (((n_)&0x1f)<<8)
#define MSC_RRR(n_)             (((n_)&0x7)<<13)

#define RCSR_REG		0x90030004
#define RCSR_HWR		(1 << 0)	
#define RCSR_SWR		(1 << 1)
#define RCSR_WDR		(1 << 2)
#define RCSR_SMR		(1 << 3)

#define PSSR_REG		0x90020004
#define PSSR_SSS		(1 << 0) 
#define PSSR_BFS		(1 << 1)
#define PSSR_VFS                (1 << 2)
#define PSSR_DH			(1 << 3)
#define PSSR_PH			(1 << 4)

#define PSPR_REG		0x90020008

#define ICMR_REG         0x90050004

#define PPCR_REG         0x90020014

#define PPCR_59MHZ	0
#define PPCR_73MHZ	1
#define PPCR_88MHZ	2
#define PPCR_103MHZ	3
#define PPCR_118MHZ	4
#define PPCR_132MHZ	5
#define PPCR_147MHZ	6
#define PPCR_162MHZ	7
#define PPCR_176MHZ	8
#define PPCR_191MHZ	9
#define PPCR_206MHZ	10
#define PPCR_221MHZ	11

/* ser. port 2:                    */
#define PPC_TXD2	0x00004000	/*  IPC Transmit Data 2            */
#define PPC_RXD2	0x00008000	/*  IPC Receive Data 2             */

#define PPDR_REG   	0x90060000	/* PPC Pin Direction Reg.          */
#define PPDR_LFCLK 	0x400

#define PPSR_REG   	0x90060004	/* PPC Pin State Reg.              */


#define GPIO_GPIO(Nb)	        	/* GPIO [0..27]                    */ \
                	(0x00000001 << (Nb))

#if defined(CONFIG_MACH_IPAQ)
/* The EGPIO is a write only control register at physical address 0x49000000
 * See the hardware spec for more details.
 */
#define IPAQ_EGPIO 0x49000000
#define H3600_EGPIO_VIRT 0x49000000
#define H3800_ASIC_BASE 0x49000000
//3800 stuff
#define H3800_FLASH_VPP_ADDR (H3800_ASIC_BASE + _H3800_ASIC2_FlashWP_Base)
#define H3800_FLASH_VPP_ON 0xf1e1;
#define H3800_FLASH_VPP_OFF 0xf1e0;
// defines for our access to the 3800 asics.  requires h3600_asic.h andh3600_gpio.h from linux.
#define H3800_ASIC1_GPIO_MASK_ADDR (H3800_ASIC_BASE  + _H3800_ASIC1_GPIO_Base + _H3800_ASIC1_GPIO_Mask)
#define H3800_ASIC1_GPIO_DIR_ADDR (H3800_ASIC_BASE  + _H3800_ASIC1_GPIO_Base + _H3800_ASIC1_GPIO_Direction)
#define H3800_ASIC1_GPIO_OUT_ADDR (H3800_ASIC_BASE  + _H3800_ASIC1_GPIO_Base + _H3800_ASIC1_GPIO_Out)
#define _H3800_ASIC2_KPIO_Base                 0x0200
#define _H3800_ASIC2_KPIO_Data                 0x0014    /* R/W, 16 bits */
#define _H3800_ASIC2_KPIO_Alternate            0x003c    /* R/W, 6 bits */
#define H3800_ASIC2_KPIO_ADDR	(H3800_ASIC_BASE  + _H3800_ASIC2_KPIO_Base + _H3800_ASIC2_KPIO_Data)
#define H3800_ASIC1_GPIO_MASK_INIT 0x7fff
#define H3800_ASIC1_GPIO_DIR_INIT  0x7fff
#define H3800_ASIC1_GPIO_OUT_INIT  0x2405

#include "ipaq-egpio.h"

#endif

#if defined(CONFIG_MACH_ASSABET) || defined(CONFIG_NEPONSET)

#define ASSABET_BCR		0x12000000
#define BCR_CF_PWR		(1 << 0)
#define BCR_CF_RST		(1 << 1)
#define BCR_SOFT_RST		(1 << 2)
#define BCR_IRDA_FSEL		(1 << 3)
#define BCR_CF_BUS_ON		(1 << 7)
#define BCR_RS232EN		(1 << 12)
#endif

#if defined(CONFIG_MACH_JORNADA56X) || defined(CONFIG_MACH_IPAQ)
#define JORNADA56X_ASIC_BASE_PHYSICAL	0x40000000
#ifdef __ASSEMBLY__
#define JORNADA_E_GPIO_BASE_PHYSICAL(A) (JORNADA56X_ASIC_BASE_PHYSICAL+0x700 + (A * 4))
#else
#define JORNADA_E_GPIO_BASE_PHYSICAL(A) (*(long*)(JORNADA56X_ASIC_BASE_PHYSICAL+0x700 + (A * 4)))
#endif
#define JORNADA_GPDPSR_PHYSICAL  JORNADA_E_GPIO_BASE_PHYSICAL(24)
#define JORNADA_GPDPCR_PHYSICAL  JORNADA_E_GPIO_BASE_PHYSICAL(25)
#define JORNADA_GPDPLR_PHYSICAL  JORNADA_E_GPIO_BASE_PHYSICAL(26)
#define JORNADA_GPDPDR_PHYSICAL  JORNADA_E_GPIO_BASE_PHYSICAL(27)
#define JORNADA_RS232_ON	(1 << 1)	/* ASIC GPIO D */

#define JORNADA_ASIC_RESET	(1 << 20)	/* SA-1110 GPIO */
#endif


#if defined(CONFIG_MACH_SKIFF) 
#define GPIO_BASE       (0x41800000)
#else
#define	GPIO_BASE	(0x90040000)
#endif


#define	GPIO_GPLR_OFF	(0)
#define	GPIO_GPDR_OFF	(4)
#define	GPIO_GPSR_OFF	(8)
#define	GPIO_GPCR_OFF	(0xc)
#define	GPIO_GRER_OFF	(0x10)
#define	GPIO_GFER_OFF	(0x14)
#define	GPIO_GEDR_OFF	(0x18)
#define	GPIO_GAFR_OFF	(0x1c)


#define	GPIO_SET(off, bits)  \
    ((*((volatile unsigned long *)(((char*)GPIO_BASE)+(off))))|=(bits))

#define	GPIO_CLR(off, bits)  \
    ((*((volatile unsigned long *)(((char*)GPIO_BASE)+(off))))&=~(bits))

#define	GPIO_READ(off)  \
    (*((volatile unsigned long *)(((char*)GPIO_BASE)+(off))))

#define	GPIO_WRITE(off, v)  \
    ((*((volatile unsigned long *)(((char*)GPIO_BASE)+(off)))) = (v))

#define	GPIO_GAFR_LCD_BITS   (0xff << 2)
#define	GPIO_GPDR_LCD_BITS   (0xff << 2)

#define	GPIO_GPLR_READ()	    GPIO_READ(GPIO_GPLR_OFF)
#define	GPIO_GPDR_READ()	    GPIO_READ(GPIO_GPDR_OFF)
#define	GPIO_GPSR_READ()	    GPIO_READ(GPIO_GPSR_OFF)
#define	GPIO_GPCR_READ()	    GPIO_READ(GPIO_GPCR_OFF)
#define	GPIO_GRER_READ()	    GPIO_READ(GPIO_GRER_OFF)
#define	GPIO_GFER_READ()	    GPIO_READ(GPIO_GFER_OFF)
#define	GPIO_GEDR_READ()	    GPIO_READ(GPIO_GEDR_OFF)
#define	GPIO_GAFR_READ()	    GPIO_READ(GPIO_GAFR_OFF)

#define	GPIO_GPLR_WRITE(v)   GPIO_WRITE(GPIO_GPLR_OFF,v)
#define	GPIO_GPDR_WRITE(v)   GPIO_WRITE(GPIO_GPDR_OFF,v)
#define	GPIO_GPSR_WRITE(v)   GPIO_WRITE(GPIO_GPSR_OFF,v)
#define	GPIO_GPCR_WRITE(v)   GPIO_WRITE(GPIO_GPCR_OFF,v)
#define	GPIO_GRER_WRITE(v)   GPIO_WRITE(GPIO_GRER_OFF,v)
#define	GPIO_GFER_WRITE(v)   GPIO_WRITE(GPIO_GFER_OFF,v)
#define	GPIO_GEDR_WRITE(v)   GPIO_WRITE(GPIO_GEDR_OFF,v)
#define	GPIO_GAFR_WRITE(v)   GPIO_WRITE(GPIO_GAFR_OFF,v)

#define	GPIO_GPLR_SET(v)	    GPIO_SET(GPIO_GPLR_OFF,v)
#define	GPIO_GPDR_SET(v)	    GPIO_SET(GPIO_GPDR_OFF,v)
#define	GPIO_GPSR_SET(v)	    GPIO_SET(GPIO_GPSR_OFF,v)
#define	GPIO_GPCR_SET(v)	    GPIO_SET(GPIO_GPCR_OFF,v)
#define	GPIO_GRER_SET(v)	    GPIO_SET(GPIO_GRER_OFF,v)
#define	GPIO_GFER_SET(v)	    GPIO_SET(GPIO_GFER_OFF,v)
#define	GPIO_GEDR_SET(v)	    GPIO_SET(GPIO_GEDR_OFF,v)
#define	GPIO_GAFR_SET(v)	    GPIO_SET(GPIO_GAFR_OFF,v)

#define	GPIO_GPLR_CLR(v)	    GPIO_CLR(GPIO_GPLR_OFF,v)
#define	GPIO_GPDR_CLR(v)	    GPIO_CLR(GPIO_GPDR_OFF,v)
#define	GPIO_GPSR_CLR(v)	    GPIO_CLR(GPIO_GPSR_OFF,v)
#define	GPIO_GPCR_CLR(v)	    GPIO_CLR(GPIO_GPCR_OFF,v)
#define	GPIO_GRER_CLR(v)	    GPIO_CLR(GPIO_GRER_OFF,v)
#define	GPIO_GFER_CLR(v)	    GPIO_CLR(GPIO_GFER_OFF,v)
#define	GPIO_GEDR_CLR(v)	    GPIO_CLR(GPIO_GEDR_OFF,v)
#define	GPIO_GAFR_CLR(v)	    GPIO_CLR(GPIO_GAFR_OFF,v)

#define GPIO_LDD8  (1 << 2)
#define GPIO_LDD9  (1 << 3)
#define GPIO_LDD10 (1 << 4)
#define GPIO_LDD11 (1 << 5)
#define GPIO_LDD12 (1 << 6)
#define GPIO_LDD13 (1 << 7)
#define GPIO_LDD14 (1 << 8)
#define GPIO_LDD15 (1 << 9)

#define GAFR *(volatile unsigned long*)(((char*)GPIO_BASE)+(GPIO_GAFR_OFF))
#define GPCR *(volatile unsigned long*)(((char*)GPIO_BASE)+(GPIO_GPCR_OFF))
#define GPDR *(volatile unsigned long*)(((char*)GPIO_BASE)+(GPIO_GPDR_OFF))
#define GPLR *(volatile unsigned long*)(((char*)GPIO_BASE)+(GPIO_GPLR_OFF))
#define GPSR *(volatile unsigned long*)(((char*)GPIO_BASE)+(GPIO_GPSR_OFF))

/*
 * power management reggies
 */

#define	PMCR 0x90020000	/* Power manager control register */
#define	PSSR 0x90020004	/* Power manager sleep status register */
#define	PSPR 0x90020008	/* Power manager scratch pad register */
#define	PWER 0x9002000C	/* Power manager wake-up enable register */
#define	PCFR 0x90020010	/* Power manager general configuration reg */
#define	PCFR_OPDE   (1<<0)	/* power down 3.6MHz osc */
#define	PCFR_FP	    (1<<1)	/* float PCMCIA controls during sleep */
#define	PCFR_FS	    (1<<2)	/* float static chip selects during sleep */
#define	PCFR_FO	    (1<<3)	/* force 32KHz osc enable on */

#define	PPCR 0x90020014 /* Power manager PLL configuration register */
#define	PGSR 0x90020018 /* Power manager GPIO sleep state register */
#define	POSR 0x9002001C /* Power manager oscillator status register */


#define	RSRR_SWR    (1<<0)	/* software reset bit */
#define RSRR 0x90030000 /* Reset controller software reset register */
#define RCSR 0x90030004 /* Reset controller status register */


#define	ICIP 0x90050000	/* interrupt controller IRQ pend reg */
#define	ICFP 0x90050010	/* interrupt controller FIQ pend reg */
#define	ICMR 0x90050004	/* interrupt controller mask reg */
#define	ICLR 0x90050008	/* interrupt controller level reg */
#define	ICCR 0x9005000C	/* interrupt controller control reg */

#define RTC_BASE 0x90010000
#define	RTAR_OFFSET 0	/* RTC alarm reg */
#define	RTAR (RTC_BASE + RTAR_OFFSET)
#define	RCNR_OFFSET 4	/* RTC count reg */
#define	RCNR (RTC_BASE + RCNR_OFFSET)
#define	RTTR_OFFSET 8	/* RTC timer trim reg */
#define	RTTR (RTC_BASE + RTTR_OFFSET)
#define	RTSR_OFFSET 0x10	/* RTC status reg */
#define	RTSR (RTC_BASE + RTSR_OFFSET)
#define UART2_HSCR0_OFFSET 0x80040060
#define UART2_HSCR0 (RTC_BASE + UART2_HSCR0_OFFSET)


#endif /* SA1100_H_INCLUDED */
