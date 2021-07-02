
#ifndef __LCD_H
#define __LCD_H

#include <stdint.h>
#include <stdbool.h>

//*********************************************************************************************
//Функции управления
//*********************************************************************************************
void LCDInit( void );
void LCDOn( void );
void LCDCls( void );
void LCDGotoXY( uint8_t x, uint8_t y );
void LCDCurOn( void );
void LCDCurOff( void );
void LCDPutc( uint8_t ch );
void LCDPuts( char *str );
void LCDBarGraph( uint32_t val, uint32_t size );

//*********************************************************************************************
//Функции статуса
//*********************************************************************************************

#endif

