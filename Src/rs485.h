#ifndef __RS485_H
#define __RS485_H

#include <stdint.h>
#include <stdbool.h>

//****************************************************************************************************************
// Прототипы функций
//****************************************************************************************************************
void RS485Init( void );
void RS485Irq( void );
void RS485Timer( void );
void RS485Send( uint8_t *data, uint8_t len_data );

#endif
