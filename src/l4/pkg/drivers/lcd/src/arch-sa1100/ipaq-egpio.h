#define EGPIO_IPAQ_VPEN        (1 << 0)    /* enables erasing and programming flash.  active high. */
#define EGPIO_IPAQ_CARD_RESET  (1 << 1)    /* reset the attached pcmcia/compactflash card.  active high. */
#define EGPIO_IPAQ_OPT_RESET   (1 << 2)    /* reset the attached option pack.  active high. */
#define EGPIO_IPAQ_CODEC_nRESET (1 << 3)   /* reset the onboard UDA1341.  active low. */
#define EGPIO_IPAQ_OPT_NVRAM_ON (1 << 4)   /* apply power to optionpack nvram, active high. */
#define EGPIO_IPAQ_OPT_ON       (1 << 5)   /* full power to option pack.  active high. */
#define EGPIO_IPAQ_LCD_ON       (1 << 6)   /* enable 3.3V to LCD.  active high. */
#define EGPIO_IPAQ_RS232_ON     (1 << 7)   /* UART3 transceiver force on.  Active high. */
#define EGPIO_IPAQ_LCD_PCI      (1 << 8)   /* LCD control IC enable.  active high. */
#define EGPIO_IPAQ_IR_ON        (1 << 9)   /* apply power to IR module.  active high. */
#define EGPIO_IPAQ_AUD_AMP_ON   (1 << 10)  /* apply power to audio power amp.  active high. */
#define EGPIO_IPAQ_AUD_PWR_ON   (1 << 11)  /* apply poewr to reset of audio circuit.  active high. */
#define EGPIO_IPAQ_QMUTE        (1 << 12)  /* mute control for onboard UDA1341.  active high. */
#define EGPIO_IPAQ_IR_FSEL      (1 << 13)  /* IR speed select: 1->fast, 0->slow */
#define EGPIO_IPAQ_LCD_5V_ON    (1 << 14)  /* enable 5V to LCD. active high. */
#define EGPIO_IPAQ_LVDD_ON      (1 << 15)  /* enable 9V and -6.5V to LCD. */

