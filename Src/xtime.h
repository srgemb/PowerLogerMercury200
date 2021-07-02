
#ifndef __XTIME_H
#define __XTIME_H

#include "stm32f1xx.h"

#define RTC_ERROR_HOUR          4   //превышение значения часов > 23
#define RTC_ERROR_MIN           5   //превышение значения минут > 59
#define RTC_ERROR_SEC           6   //превышение значения секунд > 59

#define RTC_ERROR_DAY           7   //превышение значения даты > 31
#define RTC_ERROR_DAYOFMONTH    8   //превышение значения даты в пределах месяца 28,29,30,31
#define RTC_ERROR_MONTH         9   //превышение значения месяца > 12
#define RTC_ERROR_YEAR          10  //превышение значение года > 2100

//****************************************************************************************************************
// Структура для хранения время/дата
//****************************************************************************************************************
typedef struct {
	uint8_t	    td_sec;         //секунды
	uint8_t     td_min;         //минуты
    uint8_t     td_hour;        //часы
	uint8_t     td_day;         //день
	uint8_t     td_month;       //месяц
	uint16_t    td_year;        //год
    uint8_t     td_dow;         //день недели
} timedate;

//****************************************************************************************************************
// Прототипы функций
//****************************************************************************************************************
void RTCInit( void );
void GetTimeDate( timedate *ptr );
uint8_t SetTimeDate( timedate *ptr );
uint8_t RTCCheckDate( timedate *ptr );
char *GetDateTimeStr( void );
char *GetDateY( void );
char *GetDateYM( void );
char *GetDateYMD( void );
char *RTCDescErr( uint8_t error );

#endif
