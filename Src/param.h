
#ifndef __PARAM_H
#define __PARAM_H

#include <stdint.h>
#include <stdbool.h>

//****************************************************************************************************************
// Идентификаторы параметров настроек
//****************************************************************************************************************
#define GLB_MERCURY_NUMB        1               //номер счетчика
#define GLB_MERCURY_SPEED       2               //скорость обмена данными
#define GLB_MBUS_ID             3               //номер уст-ва в сети MODBUS
#define GLB_MBUS_SPEED          4               //скорость обмена в сети MODBUS
#define GLB_DATA_LOG            5               //признак логирования данных
#define GLB_LOG_INTERVAL        6               //признак логирования данных

//Тип возвращаемого значения
#define GLB_PARAM_INDEX         0               //только индекс параметра
#define GLB_PARAM_VALUE         1               //значение параметра по индексу параметра
                                                //только для GLB_MERCURY_SPEED и GLB_MBUS_SPEED

//маски ошибок при сохранении параметров
#define ERR_FLASH_UNLOCK        0x10            //разблокировка памяти
#define ERR_FLASH_ERASE         0x20            //стирание FLASH
#define ERR_FLASH_PROGRAMM      0x40            //сохранение параметров
#define ERR_FLASH_LOCK          0x80            //блокировка памяти

//флаги источника сброса контроллера
#define STAT_EPC_PORRST         0x0001          //сброс от схемы POR
#define STAT_EPC_PINRST         0x0002          //сброс внешним сигналом
#define STAT_EPC_SFTRST         0x0004          //программный сброс
#define STAT_EPC_WDTRST         0x0008          //сброс по таймеру WDT

#pragma pack( push, 1 )

//****************************************************************************************************************
// Структура хранения параметров уст-ва
//****************************************************************************************************************
typedef struct {
    uint32_t merc_numb;                         //номер счетчика
    uint8_t merc_speed;                         //скорость обмена данными со счетчиком
    uint8_t mbus_dev;                           //номер уст-ва в сети MODBUS
    uint8_t mbus_speed;                         //скорость обмена в сети MODBUS
    uint8_t log_enable;                         //признак логирования данных со счетчика на карту памяти
    uint8_t log_interval;                       //значение указывает интервал записи данных в секундах
    uint8_t unused[3];                          //выравнивание структуры до 12 байт
 } GlbConfig;

#pragma pack( pop )

//****************************************************************************************************************
// Прототипы функций
//****************************************************************************************************************
uint8_t  GlbParamInit( void );
uint32_t GlbParamGet( uint8_t id_param, uint8_t par_type );
uint8_t  GlbParamSave( uint8_t id_param, uint32_t value );
uint32_t GlbValueIndex( uint8_t index );
char    *FlashDescErr( uint8_t error );
uint8_t  StatReset( void );

#endif



