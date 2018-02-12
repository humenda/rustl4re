#ifndef _SA1100_LCD_H_
#define _SA1100_LCD_H_

/*
 * ganked from:
 *	FILE    	SA-1100.h
 *
 *	Version 	1.2
 *	Author  	Copyright (c) Marc A. Viredaz, 1998
 *	        	DEC Western Research Laboratory, Palo Alto, CA
 *	Date    	January 1998 (April 1997)
 *	System  	StrongARM SA-1100
 *	Language	C or ARM Assembly
 *	Purpose 	Definition of constants related to the StrongARM
 *	        	SA-1100 microprocessor (Advanced RISC Machine (ARM)
 *	        	architecture version 4). This file is based on the
 *	        	StrongARM SA-1100 data sheet version 2.2.
 *
 *	        	Language-specific definitions are selected by the
 *	        	macro "LANGUAGE", which should be defined as either
 *	        	"C" (default) or "Assembly".
 *
 * and hacked by davep@crl.dec.com
 */

#define Fld(Size, Shft)	(((Size) << 16) + (Shft))
#define FSize(Field)	((Field) >> 16)
#define FShft(Field)	((Field) & 0x0000FFFF)
#define FMsk(Field)	(((UData (1) << FSize (Field)) - 1) << FShft (Field))
#define FAlnMsk(Field)	((UData (1) << FSize (Field)) - 1)
#define F1stBit(Field)	(UData (1) << FShft (Field))

#define	LCD_MAX_BPP (16)    /* that we allow in this simple situation */
#define	LCD_BPP	    (16)
#define	LCD_MONO_BPP	(4)
#define	LCD_XRES    (320)
#define	LCD_YRES    (240)

#define	MAX_PALETTE_COLORS()	(1<<12)
#define NUM_PALETTE_ENTRIES(bpp)	((bpp)==8?256:16)
#define PALETTE_MEM_SIZE(bpp)	(NUM_PALETTE_ENTRIES(bpp)<<1)
#define PALETTE_MODE_VAL(bpp)    (((bpp) & 0x018) << 9)

#define	LCD_PALETTE_SIZExx	    (512)
#define LCD_NUM_PIXELS()	    (LCD_XRES * LCD_YRES)
#define LCD_NUM_DISPLAY_BYTES(bpp)  ((LCD_NUM_PIXELS() * (bpp) + 7)/8)

#define	LCD_FB_SIZE(bpp) ((LCD_NUM_DISPLAY_BYTES(bpp) + \
    PALETTE_MEM_SIZE(bpp) + 3) & ~3)

#define	LCD_FB_MAX()		    LCD_FB_SIZE(LCD_MAX_BPP)
#define	LCD_FB_IMAGE_OFFSET(bpp)    PALETTE_MEM_SIZE(bpp)
#define	LCD_FB_IMAGE(p, bpp)	    (((char*)(p)) + LCD_FB_IMAGE_OFFSET(bpp))

#define	io_p2v(a)   a
typedef unsigned long	u_long;
typedef unsigned long	Word;
typedef unsigned long	Address;

/*
 * Liquid Crystal Display (LCD) control registers
 *
 * Registers
 *    LCCR0     	Liquid Crystal Display (LCD) Control Register 0
 *              	(read/write).
 *              	[Bits LDM, BAM, and ERM are only implemented in
 *              	versions 2.0 (rev. = 8) and higher of the StrongARM
 *              	SA-1100.]
 *    LCSR      	Liquid Crystal Display (LCD) Status Register
 *              	(read/write).
 *              	[Bit LDD can be only read in versions 1.0 (rev. = 1)
 *              	and 1.1 (rev. = 2) of the StrongARM SA-1100, it can be
 *              	read and written (cleared) in versions 2.0 (rev. = 8)
 *              	and higher.]
 *    DBAR1     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Base Address Register channel 1 (read/write).
 *    DCAR1     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Current Address Register channel 1 (read).
 *    DBAR2     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Base Address Register channel 2 (read/write).
 *    DCAR2     	Liquid Crystal Display (LCD) Direct Memory Access
 *              	(DMA) Current Address Register channel 2 (read).
 *    LCCR1     	Liquid Crystal Display (LCD) Control Register 1
 *              	(read/write).
 *              	[The LCCR1 register can be only written in
 *              	versions 1.0 (rev. = 1) and 1.1 (rev. = 2) of the
 *              	StrongARM SA-1100, it can be written and read in
 *              	versions 2.0 (rev. = 8) and higher.]
 *    LCCR2     	Liquid Crystal Display (LCD) Control Register 2
 *              	(read/write).
 *              	[The LCCR1 register can be only written in
 *              	versions 1.0 (rev. = 1) and 1.1 (rev. = 2) of the
 *              	StrongARM SA-1100, it can be written and read in
 *              	versions 2.0 (rev. = 8) and higher.]
 *    LCCR3     	Liquid Crystal Display (LCD) Control Register 3
 *              	(read/write).
 *              	[The LCCR1 register can be only written in
 *              	versions 1.0 (rev. = 1) and 1.1 (rev. = 2) of the
 *              	StrongARM SA-1100, it can be written and read in
 *              	versions 2.0 (rev. = 8) and higher. Bit PCP is only
 *              	implemented in versions 2.0 (rev. = 8) and higher of
 *              	the StrongARM SA-1100.]
 *
 * Clocks
 *    fcpu, Tcpu	Frequency, period of the CPU core clock (CCLK).
 *    fmem, Tmem	Frequency, period of the memory clock (fmem = fcpu/2).
 *    fpix, Tpix	Frequency, period of the pixel clock.
 *    fln, Tln  	Frequency, period of the line clock.
 *    fac, Tac  	Frequency, period of the AC bias clock.
 */

#define LCD_PEntrySp	2       	/* LCD Palette Entry Space [byte]  */
#define LCD_4BitPSp	        	/* LCD 4-Bit pixel Palette Space   */ \
                	        	/* [byte]                          */ \
                	(16*LCD_PEntrySp)
#define LCD_8BitPSp	        	/* LCD 8-Bit pixel Palette Space   */ \
                	        	/* [byte]                          */ \
                	(256*LCD_PEntrySp)
#define LCD_12_16BitPSp	        	/* LCD 12/16-Bit pixel             */ \
                	        	/* dummy-Palette Space [byte]      */ \
                	(16*LCD_PEntrySp)

#define LCD_PGrey	Fld (4, 0)	/* LCD Palette entry Grey value    */
#define LCD_PBlue	Fld (4, 0)	/* LCD Palette entry Blue value    */
#define LCD_PGreen	Fld (4, 4)	/* LCD Palette entry Green value   */
#define LCD_PRed	Fld (4, 8)	/* LCD Palette entry Red value     */
#define LCD_PBS 	Fld (2, 12)	/* LCD Pixel Bit Size              */
#define LCD_4Bit	        	/*  LCD 4-Bit pixel mode           */ \
                	(0 << FShft (LCD_PBS))
#define LCD_8Bit	        	/*  LCD 8-Bit pixel mode           */ \
                	(1 << FShft (LCD_PBS))
#define LCD_12_16Bit	        	/*  LCD 12/16-Bit pixel mode       */ \
                	(2 << FShft (LCD_PBS))

#define LCD_Int0_0	0x0     	/* LCD Intensity =   0.0% =  0     */
#define LCD_Int11_1	0x1     	/* LCD Intensity =  11.1% =  1/9   */
#define LCD_Int20_0	0x2     	/* LCD Intensity =  20.0% =  1/5   */
#define LCD_Int26_7	0x3     	/* LCD Intensity =  26.7% =  4/15  */
#define LCD_Int33_3	0x4     	/* LCD Intensity =  33.3% =  3/9   */
#define LCD_Int40_0	0x5     	/* LCD Intensity =  40.0% =  2/5   */
#define LCD_Int44_4	0x6     	/* LCD Intensity =  44.4% =  4/9   */
#define LCD_Int50_0	0x7     	/* LCD Intensity =  50.0% =  1/2   */
#define LCD_Int55_6	0x8     	/* LCD Intensity =  55.6% =  5/9   */
#define LCD_Int60_0	0x9     	/* LCD Intensity =  60.0% =  3/5   */
#define LCD_Int66_7	0xA     	/* LCD Intensity =  66.7% =  6/9   */
#define LCD_Int73_3	0xB     	/* LCD Intensity =  73.3% = 11/15  */
#define LCD_Int80_0	0xC     	/* LCD Intensity =  80.0% =  4/5   */
#define LCD_Int88_9	0xD     	/* LCD Intensity =  88.9% =  8/9   */
#define LCD_Int100_0	0xE     	/* LCD Intensity = 100.0% =  1     */
#define LCD_Int100_0A	0xF     	/* LCD Intensity = 100.0% =  1     */
                	        	/* (Alternative)                   */

#define _LCCR0  	0xB0100000	/* LCD Control Reg. 0              */
#define _LCSR   	0xB0100004	/* LCD Status Reg.                 */
#define _DBAR1  	0xB0100010	/* LCD DMA Base Address Reg.       */
                	        	/* channel 1                       */
#define _DCAR1  	0xB0100014	/* LCD DMA Current Address Reg.    */
                	        	/* channel 1                       */
#define _DBAR2  	0xB0100018	/* LCD DMA Base Address Reg.       */
                	        	/* channel 2                       */
#define _DCAR2  	0xB010001C	/* LCD DMA Current Address Reg.    */
                	        	/* channel 2                       */
#define _LCCR1  	0xB0100020	/* LCD Control Reg. 1              */
#define _LCCR2  	0xB0100024	/* LCD Control Reg. 2              */
#define _LCCR3  	0xB0100028	/* LCD Control Reg. 3              */

#if LANGUAGE == C
#define LCCR0   	        	/* LCD Control Reg. 0              */ \
                	(*((volatile Word *) io_p2v (_LCCR0)))
#define LCSR    	        	/* LCD Status Reg.                 */ \
                	(*((volatile Word *) io_p2v (_LCSR)))
#define DBAR1   	        	/* LCD DMA Base Address Reg.       */ \
                	        	/* channel 1                       */ \
                	(*((volatile Address *) io_p2v (_DBAR1)))
#define DCAR1   	        	/* LCD DMA Current Address Reg.    */ \
                	        	/* channel 1                       */ \
                	(*((volatile Address *) io_p2v (_DCAR1)))
#define DBAR2   	        	/* LCD DMA Base Address Reg.       */ \
                	        	/* channel 2                       */ \
                	(*((volatile Address *) io_p2v (_DBAR2)))
#define DCAR2   	        	/* LCD DMA Current Address Reg.    */ \
                	        	/* channel 2                       */ \
                	(*((volatile Address *) io_p2v (_DCAR2)))
#define LCCR1   	        	/* LCD Control Reg. 1              */ \
                	(*((volatile Word *) io_p2v (_LCCR1)))
#define LCCR2   	        	/* LCD Control Reg. 2              */ \
                	(*((volatile Word *) io_p2v (_LCCR2)))
#define LCCR3   	        	/* LCD Control Reg. 3              */ \
                	(*((volatile Word *) io_p2v (_LCCR3)))
#endif /* LANGUAGE == C */

#define LCCR0_LEN	0x00000001	/* LCD ENable                      */
#define LCCR0_CMS	0x00000002	/* Color/Monochrome display Select */
#define LCCR0_Color	(LCCR0_CMS*0)	/*  Color display                  */
#define LCCR0_Mono	(LCCR0_CMS*1)	/*  Monochrome display             */
#define LCCR0_SDS	0x00000004	/* Single/Dual panel display       */
                	        	/* Select                          */
#define LCCR0_Sngl	(LCCR0_SDS*0)	/*  Single panel display           */
#define LCCR0_Dual	(LCCR0_SDS*1)	/*  Dual panel display             */
#define LCCR0_LDM	0x00000008	/* LCD Disable done (LDD)          */
                	        	/* interrupt Mask (disable)        */
#define LCCR0_BAM	0x00000010	/* Base Address update (BAU)       */
                	        	/* interrupt Mask (disable)        */
#define LCCR0_ERM	0x00000020	/* LCD ERror (BER, IOL, IUL, IOU,  */
                	        	/* IUU, OOL, OUL, OOU, and OUU)    */
                	        	/* interrupt Mask (disable)        */
#define LCCR0_PAS	0x00000080	/* Passive/Active display Select   */
#define LCCR0_Pas	(LCCR0_PAS*0)	/*  Passive display (STN)          */
#define LCCR0_Act	(LCCR0_PAS*1)	/*  Active display (TFT)           */
#define LCCR0_BLE	0x00000100	/* Big/Little Endian select        */
#define LCCR0_LtlEnd	(LCCR0_BLE*0)	/*  Little Endian frame buffer     */
#define LCCR0_BigEnd	(LCCR0_BLE*1)	/*  Big Endian frame buffer        */
#define LCCR0_DPD	0x00000200	/* Double Pixel Data (monochrome   */
                	        	/* display mode)                   */
#define LCCR0_4PixMono	(LCCR0_DPD*0)	/*  4-Pixel/clock Monochrome       */
                	        	/*  display                        */
#define LCCR0_8PixMono	(LCCR0_DPD*1)	/*  8-Pixel/clock Monochrome       */
                	        	/*  display                        */
#define LCCR0_PDD	Fld (8, 12)	/* Palette DMA request Delay       */
                	        	/* [Tmem]                          */
#define LCCR0_DMADel(Tcpu)      	/*  palette DMA request Delay      */ \
                	        	/*  [0..510 Tcpu]                  */ \
                	((Tcpu)/2 << FShft (LCCR0_PDD))

#define LCSR_LDD	0x00000001	/* LCD Disable Done                */
#define LCSR_BAU	0x00000002	/* Base Address Update (read)      */
#define LCSR_BER	0x00000004	/* Bus ERror                       */
#define LCSR_ABC	0x00000008	/* AC Bias clock Count             */
#define LCSR_IOL	0x00000010	/* Input FIFO Over-run Lower       */
                	        	/* panel                           */
#define LCSR_IUL	0x00000020	/* Input FIFO Under-run Lower      */
                	        	/* panel                           */
#define LCSR_IOU	0x00000040	/* Input FIFO Over-run Upper       */
                	        	/* panel                           */
#define LCSR_IUU	0x00000080	/* Input FIFO Under-run Upper      */
                	        	/* panel                           */
#define LCSR_OOL	0x00000100	/* Output FIFO Over-run Lower      */
                	        	/* panel                           */
#define LCSR_OUL	0x00000200	/* Output FIFO Under-run Lower     */
                	        	/* panel                           */
#define LCSR_OOU	0x00000400	/* Output FIFO Over-run Upper      */
                	        	/* panel                           */
#define LCSR_OUU	0x00000800	/* Output FIFO Under-run Upper     */
                	        	/* panel                           */

#define LCCR1_PPL	Fld (6, 4)	/* Pixels Per Line/16 - 1          */
#define LCCR1_DisWdth(Pixel)    	/*  Display Width [16..1024 pix.]  */ \
                	(((Pixel) - 16)/16 << FShft (LCCR1_PPL))
#define LCCR1_HSW	Fld (6, 10)	/* Horizontal Synchronization      */
                	        	/* pulse Width - 2 [Tpix] (L_LCLK) */
#define LCCR1_HorSnchWdth(Tpix) 	/*  Horizontal Synchronization     */ \
                	        	/*  pulse Width [2..65 Tpix]       */ \
                	(((Tpix) - 2) << FShft (LCCR1_HSW))
#define LCCR1_ELW	Fld (8, 16)	/* End-of-Line pixel clock Wait    */
                	        	/* count - 1 [Tpix]                */
#define LCCR1_EndLnDel(Tpix)    	/*  End-of-Line Delay              */ \
                	        	/*  [1..256 Tpix]                  */ \
                	(((Tpix) - 1) << FShft (LCCR1_ELW))
#define LCCR1_BLW	Fld (8, 24)	/* Beginning-of-Line pixel clock   */
                	        	/* Wait count - 1 [Tpix]           */
#define LCCR1_BegLnDel(Tpix)    	/*  Beginning-of-Line Delay        */ \
                	        	/*  [1..256 Tpix]                  */ \
                	(((Tpix) - 1) << FShft (LCCR1_BLW))

#define LCCR2_LPP	Fld (10, 0)	/* Line Per Panel - 1              */
#define LCCR2_DisHght(Line)     	/*  Display Height [1..1024 lines] */ \
                	(((Line) - 1) << FShft (LCCR2_LPP))
#define LCCR2_VSW	Fld (6, 10)	/* Vertical Synchronization pulse  */
                	        	/* Width - 1 [Tln] (L_FCLK)        */
#define LCCR2_VrtSnchWdth(Tln)  	/*  Vertical Synchronization pulse */ \
                	        	/*  Width [1..64 Tln]              */ \
                	(((Tln) - 1) << FShft (LCCR2_VSW))
#define LCCR2_EFW	Fld (8, 16)	/* End-of-Frame line clock Wait    */
                	        	/* count [Tln]                     */
#define LCCR2_EndFrmDel(Tln)    	/*  End-of-Frame Delay             */ \
                	        	/*  [0..255 Tln]                   */ \
                	((Tln) << FShft (LCCR2_EFW))
#define LCCR2_BFW	Fld (8, 24)	/* Beginning-of-Frame line clock   */
                	        	/* Wait count [Tln]                */
#define LCCR2_BegFrmDel(Tln)    	/*  Beginning-of-Frame Delay       */ \
                	        	/*  [0..255 Tln]                   */ \
                	((Tln) << FShft (LCCR2_BFW))

#define LCCR3_PCD	Fld (8, 0)	/* Pixel Clock Divisor/2 - 2       */
                	        	/* [1..255] (L_PCLK)               */
                	        	/* fpix = fcpu/(2*(PCD + 2))       */
                	        	/* Tpix = 2*(PCD + 2)*Tcpu         */
#define LCCR3_PixClkDiv(Div)    	/*  Pixel Clock Divisor [6..514]   */ \
                	(((Div) - 4)/2 << FShft (LCCR3_PCD))
                	        	/*  fpix = fcpu/(2*Floor (Div/2))  */
                	        	/*  Tpix = 2*Floor (Div/2)*Tcpu    */
#define LCCR3_CeilPixClkDiv(Div)	/*  Ceil. of PixClkDiv [6..514]    */ \
                	(((Div) - 3)/2 << FShft (LCCR3_PCD))
                	        	/*  fpix = fcpu/(2*Ceil (Div/2))   */
                	        	/*  Tpix = 2*Ceil (Div/2)*Tcpu     */
#define LCCR3_ACB	Fld (8, 8)	/* AC Bias clock half period - 1   */
                	        	/* [Tln] (L_BIAS)                  */
#define LCCR3_ACBsDiv(Div)      	/*  AC Bias clock Divisor [2..512] */ \
                	(((Div) - 2)/2 << FShft (LCCR3_ACB))
                	        	/*  fac = fln/(2*Floor (Div/2))    */
                	        	/*  Tac = 2*Floor (Div/2)*Tln      */
#define LCCR3_CeilACBsDiv(Div)  	/*  Ceil. of ACBsDiv [2..512]      */ \
                	(((Div) - 1)/2 << FShft (LCCR3_ACB))
                	        	/*  fac = fln/(2*Ceil (Div/2))     */
                	        	/*  Tac = 2*Ceil (Div/2)*Tln       */
#define LCCR3_API	Fld (4, 16)	/* AC bias Pin transitions per     */
                	        	/* Interrupt                       */
#define LCCR3_ACBsCntOff        	/*  AC Bias clock transition Count */ \
                	        	/*  Off                            */ \
                	(0 << FShft (LCCR3_API))
#define LCCR3_ACBsCnt(Trans)    	/*  AC Bias clock transition Count */ \
                	        	/*  [1..15]                        */ \
                	((Trans) << FShft (LCCR3_API))
#define LCCR3_VSP	0x00100000	/* Vertical Synchronization pulse  */
                	        	/* Polarity (L_FCLK)               */
#define LCCR3_VrtSnchH	(LCCR3_VSP*0)	/*  Vertical Synchronization pulse */
                	        	/*  active High                    */
#define LCCR3_VrtSnchL	(LCCR3_VSP*1)	/*  Vertical Synchronization pulse */
                	        	/*  active Low                     */
#define LCCR3_HSP	0x00200000	/* Horizontal Synchronization      */
                	        	/* pulse Polarity (L_LCLK)         */
#define LCCR3_HorSnchH	(LCCR3_HSP*0)	/*  Horizontal Synchronization     */
                	        	/*  pulse active High              */
#define LCCR3_HorSnchL	(LCCR3_HSP*1)	/*  Horizontal Synchronization     */
                	        	/*  pulse active Low               */
#define LCCR3_PCP	0x00400000	/* Pixel Clock Polarity (L_PCLK)   */
#define LCCR3_PixRsEdg	(LCCR3_PCP*0)	/*  Pixel clock Rising-Edge        */
#define LCCR3_PixFlEdg	(LCCR3_PCP*1)	/*  Pixel clock Falling-Edge       */
#define LCCR3_OEP	0x00800000	/* Output Enable Polarity (L_BIAS, */
                	        	/* active display mode)            */
#define LCCR3_OutEnH	(LCCR3_OEP*0)	/*  Output Enable active High      */
#define LCCR3_OutEnL	(LCCR3_OEP*1)	/*  Output Enable active Low       */

#endif /* _SA1100_LCD_H_ */
