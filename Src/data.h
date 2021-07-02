
#ifndef __DATA_H
#define __DATA_H

#include <stdint.h>
#include <stdbool.h>

//Коды чтения мгновенных значений
#define INSTVAL_VOLTAGE         1       //напряжение сети (V)
#define INSTVAL_CURRENT         2       //ток в нагрузке (A)
#define INSTVAL_POWER           3       //мощность нагрузки (P)
#define INSTVAL_TARIFF1         4       //накопленное значение мощности, дневной тариф (kWh)
#define INSTVAL_TARIFF2         5       //накопленное значение мощности, дневной тариф (kWh)

void InitData( void );
void DataRecv( void );
char *GetStatus( void );
uint32_t GetData( uint8_t data );

#endif



