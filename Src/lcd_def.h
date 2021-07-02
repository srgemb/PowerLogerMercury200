
#ifndef __LCD_DEF_H
#define __LCD_DEF_H

//********************************************************************************
//      HD44780 Instruction Set             DB7 DB6 DB5 DB4 DB3 DB2 DB1 DB0
//      =======================             === === === === === === === ===
//********************************************************************************
#define LCD_MAIN_MODE               0x20 // 0   0   1   DL  N   F   *   *
#define LCD_DATA4                   0x00 // .   .   .   0   .   .   .   .
#define LCD_DATA8                   0x10 // .   .   .   1   .   .   .   .
#define LCD_1LINE                   0x00 // .   .   .   .   0   .   .   .
#define LCD_2LINE                   0x08 // .   .   .   .   1   .   .   .
#define LCD_FONT_5X8                0x00 // .   .   .   .   .   0   .   .
#define LCD_FONT_5X10               0x04 // .   .   .   .   .   1   .   .

//управление счетчиком адреса
#define LCD_CNT_SHIFT               0x04 // 0   0   0   0   0   1  I/D  S
#define LCD_DEC_ADDR                0x00 // .   .   .   .   .   .   0   .
#define LCD_INC_ADDR                0x02 // .   .   .   .   .   .   1   .
//управление сдвигом экрана
#define LCD_SHIFT_DISP_OFF          0x00 // .   .   .   .   .   .   .   0
#define LCD_SHIFT_DISP_ON           0x01 // .   .   .   .   .   .   .   1

//режимы управления курсором и дисплеем
#define LCD_CURS_MODE               0x08 // 0   0   0   0   1   D   C   B
#define LCD_DISPLAY_OFF             0x00 // .   .   .   .   .   0   .   .
#define LCD_DISPLAY_ON              0x04 // .   .   .   .   .   1   .   .
#define LCD_CURSOR_OFF              0x00 // .   .   .   .   .   .   0   .
#define LCD_CURSOR_ON               0x02 // .   .   .   .   .   .   1   .
#define LCD_BLINK_OFF               0x00 // .   .   .   .   .   .   .   0
#define LCD_BLINK_ON                0x01 // .   .   .   .   .   .   .   1

//управление сдвигом экрана или куросора
#define LCD_CURS_MOVE               0x10 // 0   0   0   1  S/C R/L  *   *
#define LCD_MOVE_CURSOR             0x00 // .   .   .   .   0   .   .   .
#define LCD_MOVE_DISPLAY            0x08 // .   .   .   .   1   .   .   .
#define LCD_SHIFT_LEFT              0x00 // .   .   .   .   .   0   .   .
#define LCD_SHIFT_RIGHT             0x04 // .   .   .   .   .   1   .   .

#define LCD_CLS                     0x01 // 0   0   0   0   0   0   0   1
#define LCD_HOME                    0x02 // 0   0   0   0   0   0   1   *

#define LCD_CGRAM_ADDR              0x40 // 0   1   A   A   A   A   A   A
#define LCD_DDRAM_ADDR              0x80 // 1   A   A   A   A   A   A   A
#define LCD_SET_CURSOR              0x80 // 1   A   A   A   A   A   A   A

#endif
