
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "crc16.h"
#include "data.h"
#include "main.h"
#include "param.h"
#include "events.h"
#include "mercury.h"

#include "cmsis_os.h"
#include "stm32f1xx_hal.h"

//macros convert little <-> big endian 
#define SWAP16( val )          ( (((val) & 0xFF00) >> 8) | (((val) & 0x00FF) << 8) )
#define SWAP32( val )          ( (((val) & 0xFF) << 24) | (((val) & 0xFF00) << 8) | (((val) & 0xFF0000) >> 8) | (((val) >> 24) & 0xFF) )

//****************************************************************************************************************
// Локальные константы
//****************************************************************************************************************
//Коды проверки принятого пакета от счетчика
#define DATA_ANSWER_VALID       0       //данные получены и проверены
#define DATA_NO_ANSWER          1       //счетчик не отвечает
#define DATA_CRC_ERROR          2       //ошибка принятых данных
#define DATA_ANSWER_ERROR       3       //данные ответа не совпадают с типом запроса
#define DATA_ECHO_ERROR         4       //нет эхо запроса данных

//****************************************************************************************************************
// Внешние переменные
//****************************************************************************************************************
extern UART_HandleTypeDef huart2;

//****************************************************************************************************************
// Локальные переменные
//****************************************************************************************************************
MERC_REQUEST req;
osThreadId tid_ThreadReq;
static char * const link_error[] = { 
        " Соединение  OK ", 
        "   Нет ответа   ", 
        "   Ошибка  КС   ", 
        " Ошибка  ответа ", 
        " Нет эхо запроса" 
      };
static uint8_t recv_data[64];
static uint8_t stat_link = 0, next_cmnd = 0;
static uint8_t list_cmnd[] = { INSTANTVAL, POWERTAR };      //список команд отправляемых счетчику
static uint32_t voltage, current, power, tariff1, tariff2;

//****************************************************************************************************************
// Прототипы локальные функций
//****************************************************************************************************************
static void RequestData( uint8_t command );
static void ThreadRequest( void const *arg );
static uint8_t DataCheck( uint8_t );
static uint8_t RecvCnt( void );
static void ClearRecvBuff( void );
static uint32_t BCDToInt( uint8_t *ptr, uint8_t cnt_byte );

osThreadDef( ThreadRequest, osPriorityNormal, 1, 0 ); 

//****************************************************************************************************************
// Инициализация, создание потока обмена данными со счетчиком Меркурий
//****************************************************************************************************************
void InitData( void ) {

    voltage = current = power = tariff1 = tariff2 = 0;
    memset( recv_data, 0x00, sizeof( recv_data ) ); 
    HAL_UART_Receive_IT( &huart2, recv_data, sizeof( recv_data ) );
    tid_ThreadReq = osThreadCreate( osThread( ThreadRequest ), NULL );
 }

//****************************************************************************************************************
// Проверка на переполнение буфера данных
// Вызов из stm32f1xx_it.c (USART2_IRQHandler)
//****************************************************************************************************************
void DataRecv( void ) {

    //проверка на переполнения буфера
    if ( RecvCnt() > sizeof( recv_data ) - 2 )
        ClearRecvBuff();
 } 

//**********************************************************************************
// Кол-во принятых байт по UART2
//**********************************************************************************
static uint8_t RecvCnt( void ) {

    return huart2.pRxBuffPtr - (uint8_t *)recv_data;
 }

//**********************************************************************************
// Чистим приемный буфер
//**********************************************************************************
static void ClearRecvBuff( void ) {

    memset( recv_data, 0x00, sizeof( recv_data ) );
    huart2.RxXferCount = sizeof( recv_data );
    huart2.pRxBuffPtr = (uint8_t*)recv_data;
 }

//****************************************************************************************************************
// Поток, отправка запроса к счетчику и проверка принятого ответа
//****************************************************************************************************************
static void ThreadRequest( void const *arg ) {

    while ( true ) {
        //ждем секундный сигнал от RTC для отправки запроса
        osDelay( 500 ); //период запроса даных счетчика
        ClearRecvBuff(); 
        //отправка запроса счетчику
        RequestData( list_cmnd[next_cmnd++] );
        if ( next_cmnd >= sizeof( list_cmnd ) )
            next_cmnd = 0; 
        osDelay( 150 ); //ждем ответ счетчика
        //проверка принятых данных
        stat_link = DataCheck( RecvCnt() );
       }
 }

//****************************************************************************************************************
// Формирование пакета с запросом данных от счетчика
// uint8_t command - код команды запроса
//****************************************************************************************************************
static void RequestData( uint8_t command ) {

    uint8_t *ptr;
    uint32_t numb_dev;

    ptr = (uint8_t *)&req;
    //номер счетчика
    numb_dev = GlbParamGet( GLB_MERCURY_NUMB, GLB_PARAM_VALUE );
    req.num_dev = SWAP32( numb_dev );
    req.command = command;
    //контрольная сумма
    req.crc = CalcCRC16( ptr, sizeof( req ) - 2 );
    HAL_UART_Transmit_IT( &huart2, ptr, sizeof( req ) );
    
 }

//****************************************************************************************************************
// Проверка принятых пакетов данных от счетчика
// uint8_t data_len           - размер блока данных с ответом
// return = DATA_ANSWER_VALID - данные получены и проверены
//          DATA_NO_ANSWER    - счетчик не отвечает
//          DATA_CRC_ERROR    - ошибка принятых данных
//****************************************************************************************************************
static uint8_t DataCheck( uint8_t data_len ) {

    uint16_t crc;
    uint32_t *ptr;
    uint8_t check, temp;
    MERC_TARIFF tariff;
    MERC_INST_VAL inst_val;

    if ( !data_len )
        return DATA_ECHO_ERROR;
    if ( data_len == sizeof( req ) )
        return DATA_NO_ANSWER;
    //определим тип принятого пакета данных
    if ( data_len == sizeof( inst_val ) ) {
        //мгновенные значения ток, напряжение, мощность
        memcpy( &inst_val, recv_data, sizeof( inst_val ) );
        //проверим КС только по данным ответа
        check = 0;
        crc = CalcCRC16( ((uint8_t *)&inst_val) + sizeof( req ), sizeof( inst_val ) - 2 - sizeof( req ) );
        if ( inst_val.num_dev == req.num_dev )
            check++;
        if ( inst_val.command2 == req.command )
            check++;
        if ( crc != inst_val.crc_answer ) {
            voltage = current = power = 0;
            return DATA_CRC_ERROR;
           }
        if ( check == 2 ) {
            //счетчик возвращает значения 0x23 0x76 (упакованный BCD формат)
            //например: 0x23 0x76 = 237.6V, значение в памяти: 0x23 0x76, значение в переменной 0x7623
            //значение после перестановки 0x2376
            temp = inst_val.power[0];
            inst_val.power[0] = inst_val.power[2];
            inst_val.power[2] = temp;
            inst_val.voltage = SWAP16( inst_val.voltage );
            inst_val.current = SWAP16( inst_val.current );
            voltage = BCDToInt( (uint8_t *)&inst_val.voltage, sizeof( inst_val.voltage ) );
            current = BCDToInt( (uint8_t *)&inst_val.current, sizeof( inst_val.current ) );
            power = BCDToInt( (uint8_t *)&inst_val.power, sizeof( inst_val.power ) );
            return DATA_ANSWER_VALID;
           }
        voltage = current = power = 0;
        //данные ответа не совпадают с типом запроса
        return DATA_ANSWER_ERROR;
       }

    if ( data_len == sizeof( tariff ) ) {
        //значения расхода по тарифам
        check = 0;
        memcpy( &tariff, recv_data, sizeof( tariff ) );
        //проверим КС только по данным ответа
        crc = CalcCRC16( ((uint8_t *)&tariff) + sizeof( req ), sizeof( tariff ) - 2 - sizeof( req ) );
        if ( tariff.num_dev == req.num_dev )
            check++;
        if ( tariff.command2 == req.command )
            check++;
        if ( crc != tariff.crc_answer ) {
            tariff1 = tariff2 = 0;
            return DATA_CRC_ERROR;
           }
        if ( check == 2 ) {
            //необходимо переставить байты местами.
            tariff.tariff1 = SWAP32( tariff.tariff1 );
            tariff.tariff2 = SWAP32( tariff.tariff2 );
            tariff1 = BCDToInt( (uint8_t *)&tariff.tariff1, sizeof( tariff.tariff1 ) );
            tariff2 = BCDToInt( (uint8_t *)&tariff.tariff2, sizeof( tariff.tariff2 ) );
            return DATA_ANSWER_VALID;
           }
        tariff1 = tariff2 = 0;
        //данные ответа не совпадают с типом запроса
        return DATA_ANSWER_ERROR;
       }
    //данные ответа не соответствуют запросу
    return DATA_ANSWER_ERROR;
 }

//****************************************************************************************************************
// Возвращает значения показаний счетчика
// uint8_t data - код типа данных, см. INSTVAL_*
// return       - значение показаний
//****************************************************************************************************************
uint32_t GetData( uint8_t data ) {

    if ( data == INSTVAL_VOLTAGE )
        return voltage;
    if ( data == INSTVAL_CURRENT )
        return current; 
    if ( data == INSTVAL_POWER )
        return power;
    if ( data == INSTVAL_TARIFF1 )
        return tariff1;
    if ( data == INSTVAL_TARIFF2 )
        return tariff2;
    return 0;
 }

//****************************************************************************************************************
// Преобразует значения в упакованном BCD формате в челое число (аналог функции atoi())
// uint8_t *ptr     - указатель на первый (младший байт) числа
// uint8_t cnt_byte - кол-во байт для преобразования
// return           - преобразованное число
//****************************************************************************************************************
static uint32_t BCDToInt( uint8_t *ptr, uint8_t cnt_byte ) {

    uint8_t i, low, high;
    uint32_t dec = 1, result = 0;
  
    for ( i = 0; i < cnt_byte; i++, ptr++ ) {
        low = ( (*ptr) & 0x0F );
        result += low * dec;
        dec *= 10;
        high = ( ( (*ptr) & 0xF0 ) >> 4 );
        result += high * dec;
        dec *= 10;
       }
    return result;
 }

//****************************************************************************************************************
// Возвращает текстовую расшифровку состояние связи со счетчиком
//****************************************************************************************************************
char *GetStatus( void ) {

    return link_error[stat_link];
 }
