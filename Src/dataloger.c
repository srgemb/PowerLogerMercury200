
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "sd.h"
#include "data.h"
#include "main.h"
#include "param.h"
#include "events.h"
#include "display.h"
#include "xtime.h"
#include "dataloger.h"

#include "fatfs.h"
#include "cmsis_os.h"
#include "stm32f1xx_hal.h"

//****************************************************************************************************************
// Внешние переменные
//****************************************************************************************************************
extern bool sd_mount;

//****************************************************************************************************************
// Локальные переменные
//****************************************************************************************************************
uint8_t log_time;
uint16_t err_mkdir = 0, err_file = 0;
osThreadId tid_ThreadLog, tid_ThreadLogTimer; 

//****************************************************************************************************************
// Локальные прототипы функций потоков и таймеров
//****************************************************************************************************************
static void ThreadLog( void const *arg );
static void ThreadLogTimer( void const *arg );

osThreadDef( ThreadLog, osPriorityNormal, 1, 2048 );
osThreadDef( ThreadLogTimer, osPriorityNormal, 1, 0 );

//****************************************************************************************************************
// Инициализация процесса сохранения данных в файлах
//****************************************************************************************************************
void DataLogerInit( void ) {

    log_time = GlbParamGet( GLB_LOG_INTERVAL, GLB_PARAM_VALUE );
    tid_ThreadLog = osThreadCreate( osThread( ThreadLog ), NULL );
    tid_ThreadLogTimer = osThreadCreate( osThread( ThreadLogTimer ), NULL );
 } 

//****************************************************************************************************************
// Отслеживаем секундные интервалы от RTC для сохранения данных в файле
//****************************************************************************************************************
static void ThreadLogTimer( void const *arg ) {

    timedate tm;
    
    while ( true ) {
        //ждем сигнала от RTC
        osSignalWait( EVN_SEC_TIMER, osWaitForever );
        //проверка включения режима логирования
        if ( !GlbParamGet( GLB_DATA_LOG, GLB_PARAM_VALUE ) )
            continue;
        //проверка наличия SD карты
        if ( HAL_GPIO_ReadPin( MMC_INS_GPIO_Port, MMC_INS_Pin ) )
            continue;
        //проверка монтирования SD карты
        if ( sd_mount == false )
            continue;
        //текущее время
        GetTimeDate( &tm );
        if ( !tm.td_hour && !tm.td_min && !tm.td_sec )
            osSignalSet( tid_ThreadLog, EVN_LOG_TARIFF ); //полночь, сохраним накопленный тариф 
        if ( log_time )
            log_time--;
        else {
            osSignalSet( tid_ThreadLog, EVN_LOG_DATA );
            log_time = GlbParamGet( GLB_LOG_INTERVAL, GLB_PARAM_VALUE );
           }
       }
 }
 
//****************************************************************************************************************
// Сохраняет текущее значения данных (V,I,P) в файле: YYYYMM\YYYYMMDD_dat.csv
// Сохраняет текущее значение тарифов день/ночь в файлах: YYYYMM\YYYYMMDD_tar.csv и YYYY_tar.csv
// Для данного потока выделим индивидуальный размер стека, предварительно настроим RTX_Conf_CM.с, параметры:
// Number of threads with user-provided stack size (Defines the number of threads with user-provided stack size.)
// Определяет количество потоков с предоставленным пользователем размером стека.
// Total stack size [bytes] for threads with user-provided stack size (Defines the combined stack size for threads 
// with user-provided stack size.)
// Определяет объединенный размер стека для потоков с предоставленным пользователем размером стека.
//****************************************************************************************************************
static void ThreadLog( void const *arg ) {

    FIL dat_file;
    osEvent event;
    FRESULT file_result, dir_result;
    char path[64], str[64];
    
    while ( true ) {
        //бесконечно ждем любое нажатие клавиши
        event = osSignalWait( EVN_LOG_ANY, osWaitForever );
        if ( event.status == osEventSignal ) {
            //проверим маску сигнала
            if ( event.value.signals & EVN_LOG_DATA ) {
                //сохраняем текущие данные
                memset( path, 0x00, sizeof( path ) );
                strcat( path, GetDateYM() );
                dir_result = f_mkdir( path );
                if ( !( dir_result == FR_OK || dir_result == FR_EXIST ) )
                    err_mkdir++;
                strcat( path, "/" );
                strcat( path, GetDateYMD() );
                strcat( path, "_dat.csv" );
                file_result = f_open( &dat_file, path, FA_OPEN_ALWAYS | FA_WRITE );
                if ( file_result == FR_OK ) {
                    f_lseek( &dat_file, dat_file.fsize );
                    if ( !dat_file.fsize )
                        f_puts( "Date;Time;Voltage;Current;Power\r\n", &dat_file );
                    sprintf( str, "%s;%.1f;%.2f;%u\r\n", GetDateTimeStr(), (float)GetData( INSTVAL_VOLTAGE )/10, (float)GetData( INSTVAL_CURRENT )/100, GetData( INSTVAL_POWER ) );
                    f_puts( str, &dat_file );
                    f_close( &dat_file );
                   }
                else err_file++;
               }
            if ( event.value.signals & EVN_LOG_TARIFF ) {
                //сохраняем тарифные данные в ежедневном файле
                memset( path, 0x00, sizeof( path ) );
                strcat( path, GetDateYM() );
                dir_result = f_mkdir( path );
                if ( !( dir_result == FR_OK || dir_result == FR_EXIST ) )
                    err_mkdir++;
                strcat( path, "/" );
                strcat( path, GetDateYMD() );
                strcat( path, "_tar.csv" );
                file_result = f_open( &dat_file, path, FA_OPEN_ALWAYS | FA_WRITE );
                if ( file_result == FR_OK ) {
                    f_lseek( &dat_file, dat_file.fsize );
                    if ( !dat_file.fsize )
                        f_puts( "Date;Time;Tariff1;Tariff2\r\n", &dat_file );
                    sprintf( str, "%s;%u;%u\r\n", GetDateTimeStr(), GetData( INSTVAL_TARIFF1 ), GetData( INSTVAL_TARIFF2 ) );
                    f_puts( str, &dat_file );
                    f_close( &dat_file );
                   }
                else err_file++;
                //сохраняем тарифные данные в годовом файле
                memset( path, 0x00, sizeof( path ) );
                strcat( path, GetDateY() );
                strcat( path, "_tar.csv" );
                file_result = f_open( &dat_file, path, FA_OPEN_ALWAYS | FA_WRITE );
                if ( file_result == FR_OK ) {
                    f_lseek( &dat_file, dat_file.fsize );
                    if ( !dat_file.fsize )
                        f_puts( "Date;Time;Tariff1;Tariff2\r\n", &dat_file );
                    sprintf( str, "%s;%u;%u\r\n", GetDateTimeStr(), GetData( INSTVAL_TARIFF1 ), GetData( INSTVAL_TARIFF2 ) );
                    f_puts( str, &dat_file );
                    f_close( &dat_file );
                   }
                else err_file++;
               }
          }
      }
 }

//****************************************************************************************************************
// Возвращает кол-во ошибок записи данных
// uint8_t id_error - идентификатор типа ошибки
// return           - кол-во ошибок
//****************************************************************************************************************
uint16_t DataLogerError( uint8_t id_error ) {

    if ( id_error == GET_ERROR_MAKE_DIR )
        return err_mkdir;
    if ( id_error == GET_ERROR_OPEN_FILE )
        return err_file;
    return 0;
 }
