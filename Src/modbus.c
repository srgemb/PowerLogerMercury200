
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>

#include "rs485.h"
#include "data.h"
#include "crc16.h"
#include "param.h"
#include "modbus.h"

#include "modbus_def.h"
#include "mercury_ext.h"

//*****************************************************************************************
// Локальные константы
//*****************************************************************************************
#define MAX_DATA_CRC        2               //кол-во байт для хранения КС

//*****************************************************************************************
// Локальные переменные 
//*****************************************************************************************
#pragma pack( push, 1 )                     //выравнивание структуры по границе 1 байта
//чтение регистра(ов) FUNC_RD_HOLD_REG (0x03)
typedef struct {
    uint8_t dev_addr;                       //Адрес устройства
    uint8_t function;                       //Функциональный код
    uint16_t addr_reg;                      //Адрес регистра
    uint16_t cnt_reg;                       //Количество регистров
    uint16_t crc;                           //контрольная сумма
 } REQ_RD_REG;

//*****************************************************************************************
//Структура данных ответа на запросы (0x03) чтение нескольких регистров
typedef struct {
    uint8_t dev_addr;                       //Адрес устройства
    uint8_t function;                       //Функциональный код
    uint8_t cnt_byte;                       //Количество байт данных регистров
    uint16_t data_reg[EXMER_REG_RD_MAX+1];  //Значения регистров + КС
 } ANSW_RD_REGS;

//Структура данных ответа с ошибкой
typedef struct {
    uint8_t  dev_addr;                      //Адрес устройства
    uint8_t  function;                      //Функциональный код запроса, со снятой маской FUNC_ANSWER_ERROR
    uint8_t  error;                         //Код ошибки
    uint16_t crc;                           //КС
 } ANSW_ERROR;

#pragma pack( pop )

REQ_RD_REG   req_rd_reg;
ANSW_RD_REGS rd_regs;
ANSW_ERROR   answ_error;

//*****************************************************************************************
// Прототипы локальных функций
//*****************************************************************************************
static bool CrtFrame( uint8_t func, uint16_t adr_reg, uint16_t cnt_reg, uint16_t *data_reg );
static uint8_t GetRegister( uint16_t *data, uint16_t adr_reg, uint16_t cnt_reg );
static void Swap16( uint16_t *var );

//*****************************************************************************************
// Проверяем правильность фрейма запроса
// char *data     - указатель на данные 
// uint8_t len    - размер принятых данных
// return = true  - запрос проверен
//          false - запрос не нам, КС не совпала
// Вызов из Thread485Recv()
//*****************************************************************************************
bool CheckFrame( uint8_t *data, uint8_t len ) {

    uint8_t func;
    uint16_t crc_calc, crc_data;

    if ( data == NULL || len < 8 )
        return false; //кол-во принятых данных не соответствут размеру запроса
    //проверим КС
    crc_calc = CalcCRC16( data, len - 2 );
    crc_data = *((uint16_t*)( data + len - 2 ));
    if ( crc_calc != crc_data )
        return false; //КС не совпали
    if ( *data != GlbParamGet( GLB_MBUS_ID, GLB_PARAM_VALUE ) )
        return false; //фрейм не для нас
    func = *( data + 1 );
    if ( func != FUNC_RD_HOLD_REG ) {
        //запрос не поддерживаемой функции, доступные функции: 0x03, 0x06, 0x10
        //формируем ответ с ошибкой
        answ_error.dev_addr = GlbParamGet( GLB_MBUS_ID, GLB_PARAM_VALUE );
        answ_error.function = func | FUNC_ANSWER_ERROR;
        answ_error.error = MB_ERROR_CRC;
        answ_error.crc = CalcCRC16( (uint8_t *)&answ_error, sizeof( answ_error ) - 2 );
        RS485Send( (uint8_t *)&answ_error, sizeof( answ_error ) );
       }
    //обработаем принятый фрейм для дальнейшей проверки
    if ( func == FUNC_RD_HOLD_REG ) {
        //чтение значений из нескольких регистров хранения
        memset( &req_rd_reg, 0x00, sizeof( req_rd_reg ) );
        memcpy( &req_rd_reg, data, sizeof( req_rd_reg ) );
        Swap16( &req_rd_reg.addr_reg );
        Swap16( &req_rd_reg.cnt_reg );
        CrtFrame( req_rd_reg.function, req_rd_reg.addr_reg, req_rd_reg.cnt_reg, NULL );  
       } 
    return true;
 }

//*****************************************************************************************
// Формируем фрейм ответа и запускаем выполнение функции
// uint8_t func       - код функции  
// uint16_t adr_reg   - адрес регистра   
// uint16_t cnt_reg   - кол-во регистров  
// uint16_t *data_reg - указатель на значения регистров 
//*****************************************************************************************
static bool CrtFrame( uint8_t func, uint16_t adr_reg, uint16_t cnt_reg, uint16_t *data_reg ) {

    uint16_t crc;
    uint8_t idx, error = 0;
    
    //проверка исходных параметров
    if ( func == FUNC_RD_HOLD_REG && ( adr_reg >= EXMER_REG_RD_MAX || ( adr_reg + cnt_reg ) > EXMER_REG_RD_MAX ) && !error ) {
        //чтение значений из нескольких регистров хранения
        error = MB_ERROR_ADDR; //выход за пределы адресов регистров чтения
        func |= FUNC_ANSWER_ERROR;
       }   
    //выполнение функции
    if ( func == FUNC_RD_HOLD_REG && !error ) {
        //чтение значений из нескольких регистров хранения
        memset( (uint8_t *)&rd_regs, 0x00, sizeof( rd_regs ) );
        rd_regs.dev_addr = GlbParamGet( GLB_MBUS_ID, GLB_PARAM_VALUE );
        rd_regs.function = func;
        //запишем значения запрашиваемых регистров в массив
        rd_regs.cnt_byte = GetRegister( rd_regs.data_reg, adr_reg, cnt_reg );
        //поменяем байты местами для переменных uint16_t, т.к. сначала передаем старший байт
        for ( idx = 0; idx < cnt_reg; idx++ )
            Swap16( &rd_regs.data_reg[idx] );
        crc = CalcCRC16( (uint8_t *)&rd_regs, rd_regs.cnt_byte + 3 );
        rd_regs.data_reg[cnt_reg] = crc;
        //передаем ответ
        RS485Send( (uint8_t *)&rd_regs, rd_regs.cnt_byte + 3 + 2 ); 
        return true;
       }  
    if ( func & FUNC_ANSWER_ERROR ) {
        //формируем ответ с ошибкой
        answ_error.dev_addr = GlbParamGet( GLB_MBUS_ID, GLB_PARAM_VALUE );
        answ_error.function = func;
        answ_error.error = error;
        answ_error.crc = CalcCRC16( (uint8_t *)&answ_error, sizeof( answ_error ) - 2 );
        RS485Send( (uint8_t *)&answ_error, sizeof( answ_error ) );
        return true;
       }
    return false; 
 }
 
//*****************************************************************************************
// Заполняем блок памяти значениями регистров
// uint16_t *data   - адрес памяти для размещения данных 
// uint16_t adr_reg - адрес регистра 
// uint16_t cnt_reg - кол-во регистров
// return           - кол-во записанных байт  
//*****************************************************************************************
static uint8_t GetRegister( uint16_t *data, uint16_t adr_reg, uint16_t cnt_reg ) {

    uint8_t bytes = 0;
    
    //регистр текущего источника сброса контроллера
    if ( adr_reg == EXMER_REG_RD_STAT && cnt_reg ) {
        bytes += 2;
        *data = StatReset();
        if ( cnt_reg-- ) {
            data++;
            adr_reg++;
           }
       }
    //регистр мгновенного значения тока
    if ( adr_reg == EXMER_REG_RD_CURRENT && cnt_reg ) {
        bytes += 2;
        *data = GetData( INSTVAL_CURRENT );
        if ( cnt_reg-- ) {
            data++;
            adr_reg++;
           }
       }
    //регистр мгновенного значения напряжения
    if ( adr_reg == EXMER_REG_RD_VOLTAGE && cnt_reg ) {
        bytes += 2;
        *data = GetData( INSTVAL_VOLTAGE );
        if ( cnt_reg-- ) {
            data++;
            adr_reg++;
           }
       }
    //регистр мгновенного значения потребляемой мощности
    if ( adr_reg == EXMER_REG_RD_POWER && cnt_reg ) {
        bytes += 2;
        *data = GetData( INSTVAL_POWER );
        if ( cnt_reg-- ) {
            data++;
            adr_reg++;
           }
       }
    //регистр значения накопленной мощности дневного тарифа
    if ( adr_reg == EXMER_REG_RD_TARIFF1 && cnt_reg ) {
        bytes += 2;
        *data = (uint16_t)GetData( INSTVAL_TARIFF1 );
        if ( cnt_reg-- ) {
            data++;
            adr_reg++;
           }
       }
    //регистр значения накопленной мощности ночного тарифа
    if ( adr_reg == EXMER_REG_RD_TARIFF2 && cnt_reg ) {
        bytes += 2;
        *data = (uint16_t)GetData( INSTVAL_TARIFF2 );
       }
    return bytes;
 }

//*********************************************************************************************
// Перестановка в переменной uint16_t байт местами
//*********************************************************************************************
static void Swap16( uint16_t *var ) {

    uint8_t *temp, val;
    
    temp = (uint8_t *)var;
    val = *temp;
    *temp = *( temp + 1 );
    *( temp + 1 ) = val;
 }
