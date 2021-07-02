#ifndef __DATALOGER_H
#define __DATALOGER_H

#include <stdint.h>
#include <stdbool.h>

#define GET_ERROR_MAKE_DIR          0           //ошибки создания каталога
#define GET_ERROR_OPEN_FILE         1           //ошибки открытия файлов

void DataLogerInit( void );
uint16_t DataLogerError( uint8_t id_error );

#endif
