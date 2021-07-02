
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "param.h"
#include "stm32f1xx_hal.h"

//****************************************************************************************************************
// Локальные константы
//****************************************************************************************************************
#define FLASH_DATA_ADDRESS      0x0801FC00      //адрес для хранения параметров
                                                //последняя страница во FLASH (1Kb)

//значения скорости для последовательных портов
static const uint32_t dev_speed[] = { 600, 1200, 2400, 4800, 9600, 14400, 19200, 28800, 38400, 56000, 57600, 115200 };

//****************************************************************************************************************
// Локальные переменные
//****************************************************************************************************************
//расшифровка кодов записи во FLASH память
static char result[20];
static char * const mask_error[]  = { "UNLOCK-", "ERASE-", "PROGRAM-", "LOCK-" };
static char * const flash_error[] = { "ОК",      "ERROR",  "BUSY",     "TIMEOUT" };

GlbConfig GlbConf;

//****************************************************************************************************************
// Инициализация параметров настроек значениями из FLASH
//****************************************************************************************************************
uint8_t GlbParamInit( void ) {

    bool change = false;
    uint8_t dw, dw_cnt;
    uint32_t *source_addr, *dest_addr, err_addr;
    uint32_t *ptr_glb, ptr_flash;
    HAL_StatusTypeDef stat_flash;
    FLASH_EraseInitTypeDef erase;
    
    source_addr = (uint32_t *)FLASH_DATA_ADDRESS;
    dest_addr = (uint32_t *)&GlbConf;
    dw_cnt = ( sizeof( GlbConf ) * 8 )/32;
    //читаем значение только как WORD по 4 байта
    for ( dw = 0; dw < dw_cnt; dw++ ) {
        *dest_addr = *(__IO uint32_t *)source_addr;
        source_addr++, 
        dest_addr++;
       }
    //проверим параметры на допустимостимые значения
    if ( GlbConf.merc_numb == 0xFFFFFFFF ) {
        change = true;
        GlbConf.merc_numb = 0;              //номер счетчика
       }
    if ( GlbConf.merc_speed == 0xFF ) {
        change = true;
        GlbConf.merc_speed = 4;             //скорость обмена со счетчиком 9600
       }
    if ( GlbConf.mbus_dev == 0xFF ) {
        change = true;
        GlbConf.mbus_dev = 10;              //номер уст-ва в сети ModBus
       }
    if ( GlbConf.mbus_speed == 0xFF ) {
        change = true;
        GlbConf.mbus_speed = 6;             //скорость обмена в сети ModBus 19200
       }
    if ( GlbConf.log_enable == 0xFF ) {
        change = true;
        GlbConf.log_enable = false;         //логирование данных - да/нет
       }
    if ( GlbConf.log_interval == 0xFF ) {
        change = true;
        GlbConf.log_interval = 60;          //интервал логирования данных
       }
    if ( change == false )
        return HAL_OK;
    //было изменение параметра, сохраним новое значения
    //разблокируем память
    stat_flash = HAL_FLASH_Unlock();
    if ( stat_flash != HAL_OK )
        return ERR_FLASH_UNLOCK | stat_flash;
    //стирание памяти
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.PageAddress = FLASH_DATA_ADDRESS;
    erase.NbPages = 1;
    stat_flash = HAL_FLASHEx_Erase( &erase, &err_addr );
    if ( stat_flash != HAL_OK )
        return ERR_FLASH_ERASE | stat_flash;
    //запись памяти по 4 байта (32 бита)
    ptr_glb = (uint32_t *)&GlbConf;
    ptr_flash = FLASH_DATA_ADDRESS;
    for ( dw = 0; dw < dw_cnt; dw++, ptr_glb++, ptr_flash += 4 ) {
        stat_flash = HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, ptr_flash, *ptr_glb );    
        if ( stat_flash != HAL_OK )
            return ERR_FLASH_PROGRAMM | stat_flash;
       }
    //блокировка памяти
    stat_flash = HAL_FLASH_Lock();
    if ( stat_flash != HAL_OK )
        return ERR_FLASH_LOCK | stat_flash;
    return HAL_OK;
 }

//****************************************************************************************************************
// Возвращает значение параметра по идентификатору параметра
// uint8_t id_param - идентификатор параметра
// uint8_t par_type - тип возвращаемого параметра: 
//                    GLB_PARAM_VALUE - фактическое значение
//                    GLB_PARAM_INDEX - индекс значения, только для GLB_MERCURY_SPEED и GLB_MBUS_SPEED
// return           - значение параметра
//****************************************************************************************************************
uint32_t GlbParamGet( uint8_t id_param, uint8_t par_type ) {

    if ( id_param == GLB_MERCURY_NUMB )
        return GlbConf.merc_numb;
    if ( id_param == GLB_MERCURY_SPEED ) {
        if ( par_type == GLB_PARAM_VALUE )
            return dev_speed[GlbConf.merc_speed];
        else return GlbConf.merc_speed;
       }
    if ( id_param == GLB_MBUS_ID )
        return GlbConf.mbus_dev;
    if ( id_param == GLB_MBUS_SPEED ) {
        if ( par_type == GLB_PARAM_VALUE )
            return dev_speed[GlbConf.mbus_speed];
        else return GlbConf.mbus_speed;
       }
    if ( id_param == GLB_DATA_LOG )
        return GlbConf.log_enable;
    if ( id_param == GLB_LOG_INTERVAL )
        return GlbConf.log_interval;
    return 0;
 }
 
//****************************************************************************************************************
// Сохранить значение параметра во FLASH
// uint8_t id_param                         - ID параметра
// uint32_t value                           - значение параметра
// return = HAL_OK                          - запись параметров выполнена
//          ERR_FLASH_* | HAL_StatusTypeDef - маска источника ошибки и код ошибки  
//****************************************************************************************************************
uint8_t GlbParamSave( uint8_t id_param, uint32_t value ) {

    uint8_t dw, dw_cnt;
    uint32_t err_addr, *ptr_glb, ptr_flash;
    HAL_StatusTypeDef stat_flash;
    FLASH_EraseInitTypeDef erase;
    
    //сохраним значение параметров в структуре PARAM
    if ( id_param == GLB_MERCURY_NUMB )
        GlbConf.merc_numb = value;
    if ( id_param == GLB_MERCURY_SPEED )
        GlbConf.merc_speed = (uint8_t)value;
    if ( id_param == GLB_MBUS_ID )
        GlbConf.mbus_dev = (uint8_t)value;
    if ( id_param == GLB_MBUS_SPEED )
        GlbConf.mbus_speed = (uint8_t)value;
    if ( id_param == GLB_DATA_LOG )
        GlbConf.log_enable = (bool)value;
    if ( id_param == GLB_LOG_INTERVAL )
        GlbConf.log_interval = value;
    GlbConf.unused[0] = GlbConf.unused[1] = GlbConf.unused[2] = 0;
    //сохраним параметры во FLASH
    //разблокируем память
    stat_flash = HAL_FLASH_Unlock();
    if ( stat_flash != HAL_OK )
        return ERR_FLASH_UNLOCK | stat_flash;
    //стирание памяти
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.Banks = FLASH_BANK_1;
    erase.PageAddress = FLASH_DATA_ADDRESS;
    erase.NbPages = 1;
    stat_flash = HAL_FLASHEx_Erase( &erase, &err_addr );
    if ( stat_flash != HAL_OK )
        return ERR_FLASH_ERASE | stat_flash;
    //запись памяти по 4 байта (32 бита)
    ptr_glb = (uint32_t *)&GlbConf;
    ptr_flash = FLASH_DATA_ADDRESS;
    dw_cnt = ( sizeof( GlbConf ) * 8 )/32;
    for ( dw = 0; dw < dw_cnt; dw++, ptr_glb++, ptr_flash += 4 ) {
        stat_flash = HAL_FLASH_Program( FLASH_TYPEPROGRAM_WORD, ptr_flash, *ptr_glb );    
        if ( stat_flash != HAL_OK )
            return ERR_FLASH_PROGRAMM | stat_flash;
       }
    //блокировка памяти
    stat_flash = HAL_FLASH_Lock();
    if ( stat_flash != HAL_OK )
        return ERR_FLASH_LOCK | stat_flash;
    return HAL_OK;
 }

//****************************************************************************************************************
// Возвращает фактическое значение скорости для последовательного порта по индексу
// uint8_t index - значение индекса (код) скорости: 0 - 11
// return        - значение скорости 600, 1200 ... 57600, 115200
//           = 0 - превышение значения индекса   
//****************************************************************************************************************
uint32_t GlbValueIndex( uint8_t index ) {

    if ( index > ( sizeof( dev_speed ) / sizeof( uint32_t ) ) )
        return 0;
    return dev_speed[index];
 }
 
//****************************************************************************************************************
// Текстовая расшифровка кодов ошибок при записи во FLASH
// uint8_t error - маски ошибок
// return        - адрес строки с расшифровкой кода ошибки
//****************************************************************************************************************
char *FlashDescErr( uint8_t error ) {

    if ( !error )
        return flash_error[error];
    memset( result, 0x00, sizeof( result ) );
    //имя источника ошибки
    if ( error & ERR_FLASH_UNLOCK )
        strcpy( result, mask_error[0] );
    if ( error & ERR_FLASH_ERASE )
        strcpy( result, mask_error[1] );
    if ( error & ERR_FLASH_PROGRAMM )
        strcpy( result, mask_error[2] );
    if ( error & ERR_FLASH_LOCK )
        strcpy( result, mask_error[3] );
    //ошибка функции
    strcat( result, flash_error[error & 0x0F] );
    return result;
 }

//**********************************************************************************
// Возвращает флаги источника сброса процессора
//**********************************************************************************
uint8_t StatReset( void ) {

    uint8_t  reset = 0;
    uint32_t res_src;
    
    res_src = RCC->CSR;
    if ( res_src & RCC_CSR_PINRSTF )
        reset |= STAT_EPC_PINRST;
    if ( res_src & RCC_CSR_PORRSTF )
        reset |= STAT_EPC_PORRST;
    if ( res_src & RCC_CSR_SFTRSTF )
        reset |= STAT_EPC_SFTRST;
    if ( res_src & RCC_CSR_IWDGRSTF )
        reset |= STAT_EPC_WDTRST;
    return reset;
 }
