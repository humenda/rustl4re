#pragma once

enum Leon_memory_map
{
	LEON_PROM_BASE = 0x00000000,
	LEON_IO_BASE   = 0x20000000,
	LEON_RAM_BASE  = 0x40000000,
	LEON_AHB_BASE  = 0x80000000,
	LEON_ETH_BASE  = 0xFFFB0000,
	LEON_CAN_BASE  = 0xFFFC0000,
	LEON_PNP_BASE  = 0xFFFFF000,
};


enum Leon_register_offsets
{
	LEON_MCTRL    =       0,
	LEON_APBUART  =   0x100,
	LEON_IRQMP    =   0x200,
	LEON_GPTIMER  =   0x300,
	LEON_PS2      =   0x500,
	LEON_VGA      =   0x600,
	LEON_AHBUART  =   0x700,
	LEON_GRGPIO   =   0x800,
	LEON_GRSPW1   =   0xA00,
	LEON_GRSPW2   =   0xB00,
	LEON_GRSPW3   =   0xD00,
	LEON_AHBSTAT  =   0xF00,
	LEON_AHBPNP   = 0xFF000,
};


enum Leon_traps
{
	LEON_TR_RESET            = 0x00,
	LEON_TR_INSTR_ACCESS_ERR = 0x01,
	LEON_TR_ILLEGAL_INSTR    = 0x02,
	LEON_TR_PRIV_INSTR       = 0x03,
	LEON_TR_FP_DISABLED      = 0x04,
	LEON_TR_WRITE_ERR        = 0x2B,
	LEON_TR_CP_DISABLED      = 0x2f,
	LEON_TR_WATCHPOINT       = 0x0B,
	LEON_TR_WINDOW_OVERFL    = 0x05,
	LEON_TR_WINDOW_UNDERFL   = 0x06,
	LEON_TR_REG_HW_ERR       = 0x20,
	LEON_TR_ALIGN            = 0x07,
	LEON_TR_FP_EXC           = 0x08,
	LEON_TR_CP_EXC           = 0x28,
	LEON_TR_DATA_ACCESS_EXC  = 0x09,
	LEON_TR_TAG_OVERFL       = 0x0A,
	LEON_TR_DIVIDE_EXC       = 0x2A,
	LEON_TR_IRQ1             = 0x11,
	LEON_TR_IRQ2             = 0x12,
	LEON_TR_IRQ3             = 0x13,
	LEON_TR_IRQ4             = 0x14,
	LEON_TR_IRQ5             = 0x15,
	LEON_TR_IRQ6             = 0x16,
	LEON_TR_IRQ7             = 0x17,
	LEON_TR_IRQ8             = 0x18,
	LEON_TR_IRQ9             = 0x19,
	LEON_TR_IRQ10            = 0x1A,
	LEON_TR_IRQ11            = 0x1B,
	LEON_TR_IRQ12            = 0x1C,
	LEON_TR_IRQ13            = 0x1D,
	LEON_TR_IRQ14            = 0x1E,
	LEON_TR_IRQ15            = 0x1F,
	LEON_TR_SW_MIN           = 0x80,
	LEON_TR_SW_MAX           = 0xFF,
};
