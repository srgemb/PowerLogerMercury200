#ifndef __MODBUS_H
#define __MODBUS_H

#include <stdint.h>
#include <stdbool.h>

bool CheckFrame( uint8_t *data, uint8_t len );

#endif
