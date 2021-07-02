
//****************************************************************************************************************
//
// Функционал управления отображением информации и меню на LCD индикаторе
//
//****************************************************************************************************************

#include <math.h>
#include <ctype.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "lcd.h"
#include "data.h"
#include "main.h"
#include "param.h"
#include "events.h"
#include "display.h"
#include "xtime.h"
#include "dataloger.h"

#include "cmsis_os.h"
#include "stm32f1xx_hal.h"

//****************************************************************************************************************
// Внешние переменные
//****************************************************************************************************************
extern IWDG_HandleTypeDef hiwdg;

//****************************************************************************************************************
// Локальные константы
//****************************************************************************************************************
#define DIRECTION_UP            1           //направление изменения значения - увеличение
#define DIRECTION_DN            2           //направление изменения значения - уменьшение

#define BEEP_SHORT_TIME         300         //длительность короткого звукового сигнала
#define BEEP_LONG_TIME          600         //длительность длинного звукового сигнала

#define TIME_LOGO_MODE          3000        //длительность отображения заставки после включения уст-ва
#define TIME_ENTER_EDIT         3000        //длительность нажатия клавиши ENTER для входа в режим редактирования параметров
#define TIME_EXIT_EDIT          60000       //~ 26 сек выход из режима редактирования параметров

#define CONFIRM_OFF             0           //режим подтверждения сохранения нового значения параметра не включен
#define CONFIRM_NO              1           //режим подтверждения сохранения параметра: "Нет"
#define CONFIRM_YES             2           //режим подтверждения сохранения параметра: "Да"

#define ELEMENT_HOUR            0           //индекс получения часов из маски ввода
#define ELEMENT_MIN             1           //индекс получения минут из маски ввода
#define ELEMENT_SEC             2           //индекс получения секунд из маски ввода

#define ELEMENT_DAY             0           //индекс получения дня из маски ввода
#define ELEMENT_MONTH           1           //индекс получения месяца из маски ввода
#define ELEMENT_YEAR            2           //индекс получения года из маски ввода

#define TEMPLATE_STR            20          //максимальный размер буфера для размещения строки меню
#define TEMPLATE_MASK           12          //максимальный размер буфера для размещения маски параметра

//Основные режимы отображения
#define DISPLAY_MODE_LOGO       0           //сообщение при включении контроллера
#define DISPLAY_MODE_INFO       1           //режим вывода "текущих значений"
#define DISPLAY_MODE_PARAM      2           //режим вывода "параметров настроек"

//код вывода значений для режима DISPLAY_MODE_INFO
#define DISPLAY_INFO_TIME       1           //вывод текущего времени и даты встроенных часов
#define DISPLAY_INFO_INSTVAL    2           //вывод мгновенных значений счетчика
#define DISPLAY_INFO_TARIFF     3           //вывод значений тариф день/ночь
#define DISPLAY_INFO_LINKSTAT   4           //состояние связи со счетчиком
#define DISPLAY_INFO_SDSTAT     5           //ошибки записи файлов
#define DISPLAY_INFO_FIRST      6           //переход на первый элемент

//код вывода значений для режима DISPLAY_MODE_PARAM
#define DISPLAY_PARAM_MERCNUMB  1           //вывод номера счетчика
#define DISPLAY_PARAM_MERCSPEED 2           //скорость обмена данными со счетчиком
#define DISPLAY_PARAM_SETTIME   3           //установка времени
#define DISPLAY_PARAM_SETDATE   4           //установка даты
#define DISPLAY_PARAM_DEVID     5           //номер уст-ва в сети MODBUS
#define DISPLAY_PARAM_DEVSPEED  6           //скорость обмена в сети MODBUS
#define DISPLAY_PARAM_LOGGING   7           //признак логирования информации с счетчика на SD карту
#define DISPLAY_PARAM_INTVLOG   8           //интервал логирования на SD карту (в секундах)
#define DISPLAY_PARAM_FIRST     9           //переход на первый элемент

//структура для описания меню параметров
typedef struct {
    uint8_t x1;                         //позиция в строке для вывода первой строки
    uint8_t y1;                         //номер строки для вывода первой строки
    char str1[TEMPLATE_STR];            //первая строка для вывода
    uint8_t x2;                         //позиция в строке для вывода второй строки
    uint8_t y2;                         //номер строки для вывода второй строки
    char str2[TEMPLATE_STR];            //строка для вывода
    uint8_t x3;                         //позиции вывода изменяемого значения
    uint8_t y3;                         //вторая строка
    char delim[2];                      //разделитель используемый в маске для выделения групп элементов (значений)
    char mask[TEMPLATE_MASK];           //маска позиционирования курсора, если маска не задана курсор не перемещается 
                                        //при изменении значениия параметра
    uint8_t min;                        //минимальное значение параметра
    uint32_t max;                       //максимальное значение параметра для любого элемента в маске, если маска задана
                                        //если маска не задана - кол-во вариантов выбора "Да"/"Нет" = 2 варианта
    bool preset_grp;                    //признак наличия минимального и максимального значений для групп (элементов)
                                        //значения лежат в min_grp и max_grp. При этом в min и в max лежат минимальное 
                                        //и максимальное значение для каждого числа в любом разряде
    uint16_t min_grp[3];                //минимальные значения для каждой группы в маске
    uint16_t max_grp[3];                //макимальные значения для каждой группы в маске
 } DISPLAY;

//****************************************************************************************************************
// Локальные переменные
//****************************************************************************************************************
bool beep_enable = false;

static osTimerId id_Timer1, id_Timer2, id_Timer3, id_Timer4;
osThreadId tid_ThreadKey, tid_ThreadDisplay, tid_ThreadDisplayOut; 
osThreadId tid_ThreadEdit, tid_ThreadBeep, tid_ThreadSel, tid_ThreadLed;

static bool mode_edit = false;
static uint8_t ind_pos, ind_grp, display_mode, display_subm, val_pos[12], flg_cls = 0, confirm = 0;

//описания меню параметров
static const DISPLAY display[DISPLAY_PARAM_FIRST] = {
    { 0, 0, "",                  0, 0, "",                  0,  0, "",  "",            0, 0                                 }, //не используется
    { 2, 1, "Номер счетчика",    6, 2, "%06u",              6,  2, "",  "______",      0, 999999                            }, //номер счетчика
    { 1, 1, "Скорость  обмена",  1, 2, "счетчика: %u ",     11, 2, "",  "",            0, 5                                 }, //скорость обмена с счетчиком
    { 2, 1, "Уст-ка времени",    5, 2, "%02d:%02d:%02d",    5,  2, ":", "__:__:__",    0, 9, true, {0,0,0},    {23,59,59}   }, //установка времени
    { 2, 1, "Установка даты",    4, 2, "%02d.%02d.%04d",    4,  2, ".", "__.__.____",  1, 9, true, {1,1,2000}, {31,12,2100} }, //установка даты
    { 1, 1, "Номер устройства",  1, 2, "ModBus: %03u ",     9,  2, "",  "___",         1, 247                               }, //номер уст-ва в сети ModBus
    { 1, 1, "Скорость обмена",   1, 2, "ModBus: %u ",       9,  2, "",  "",            0, 11                                }, //скорость уст-ва в сети ModBus
    { 3, 1, "Логирование",       3, 2, "данных: %s",        11, 2, "",  "",            0, 2                                 }, //признак логирования данных
    { 1, 1, "Интервал лог-ния",  1, 2, "данных: %03u сек",  9,  2, "",  "___",         5, 255                               }, //интервал логирования данных
 };
        
//****************************************************************************************************************
// Локальные прототипы функций
//****************************************************************************************************************
static void DisplaySubMode( uint8_t direction );
static void DisplayMode( uint8_t mode );
static void ChangeValue( uint8_t direction );
static void SetDataEdit( char *str );
static void DataEditOut( void );
static uint32_t GetDataEdit( uint8_t element );
static uint8_t DataEditSave( void );

//****************************************************************************************************************
// Локальные прототипы функций потоков и таймеров
//****************************************************************************************************************
static void ThreadLed( void const *arg );
static void ThreadKey( void const *arg );
static void ThreadDisplay( void const *arg );
static void ThreadDisplayOut( void const *arg );
static void ThreadEdit( void const *arg );
static void ThreadBeep( void const *arg );
static void ThreadSelect( void const *arg );

static void Timer1_Callback( void const *arg );
static void Timer2_Callback( void const *arg );
static void Timer3_Callback( void const *arg );
static void Timer4_Callback( void const *arg );

osThreadDef( ThreadLed, osPriorityNormal, 1, 0 );
osThreadDef( ThreadKey, osPriorityNormal, 1, 512 );
osThreadDef( ThreadDisplay, osPriorityNormal, 1, 0 ); 
osThreadDef( ThreadDisplayOut, osPriorityNormal, 1, 512 ); 
osThreadDef( ThreadEdit, osPriorityNormal, 1, 0 ); 
osThreadDef( ThreadBeep, osPriorityNormal, 1, 0 ); 
osThreadDef( ThreadSelect, osPriorityNormal, 1, 0 ); 

osTimerDef( Timer1, Timer1_Callback );
osTimerDef( Timer2, Timer2_Callback );
osTimerDef( Timer3, Timer3_Callback );
osTimerDef( Timer4, Timer4_Callback );

//****************************************************************************************************************
// Создаем поток управления выводом информации на дисплей
//****************************************************************************************************************
void DisplayInit( void ) {

    DisplayMode( DISPLAY_MODE_LOGO );
    
    tid_ThreadLed = osThreadCreate( osThread( ThreadLed ), NULL );
    tid_ThreadKey = osThreadCreate( osThread( ThreadKey ), NULL );
    tid_ThreadDisplay = osThreadCreate( osThread( ThreadDisplay ), NULL );
    tid_ThreadDisplayOut = osThreadCreate( osThread( ThreadDisplayOut ), NULL );
    tid_ThreadEdit = osThreadCreate( osThread( ThreadEdit ), NULL );
    tid_ThreadBeep = osThreadCreate( osThread( ThreadBeep ), NULL );
    tid_ThreadSel = osThreadCreate( osThread( ThreadSelect ), NULL );
    
    id_Timer1 = osTimerCreate( osTimer( Timer1 ), osTimerOnce, NULL );
    id_Timer2 = osTimerCreate( osTimer( Timer2 ), osTimerOnce, NULL );
    id_Timer3 = osTimerCreate( osTimer( Timer3 ), osTimerOnce, NULL );
    id_Timer4 = osTimerCreate( osTimer( Timer4 ), osTimerOnce, NULL );

    osTimerStart( id_Timer3, TIME_LOGO_MODE );
    osSignalSet( tid_ThreadDisplayOut, EVN_DISP_UPDATE );
 }

//****************************************************************************************************************
// Смена режимов отображения информации на дисплее
// uint8_t mode - установка режима отображения информации на дисплее "значения"/"настройка параметров"
//****************************************************************************************************************
static void DisplayMode( uint8_t mode ) {

    flg_cls = 0;
    mode_edit = false;
    display_mode = mode;
    display_subm = 1;
    //обновим информацию на дисплее
    osSignalSet( tid_ThreadDisplayOut, EVN_DISP_UPDATE );
 }
 
//****************************************************************************************************************
// Обработка нажатия кнопок
//****************************************************************************************************************
static void ThreadKey( void const *arg ) {

    osEvent event;
    
    while ( true ) {
        //бесконечно ждем любое нажатие клавиши
        event = osSignalWait( EVN_KEY_ANY, osWaitForever );
        if ( event.status == osEventSignal ) {
            flg_cls = 0; //разрешим очистку экрана дисплея при отображении значений
            //любое нажатие кнопок - останавливаем таймер входа в режим настроек
            osTimerStop( id_Timer1 );
            osTimerStop( id_Timer2 );
            osTimerStart( id_Timer2, TIME_EXIT_EDIT );
            //проверим маску сигнала
            if ( event.value.signals & EVN_KEY_ENTER ) {
                //перезапуск таймера входа в режим настроек
                osTimerStop( id_Timer1 );
                osTimerStart( id_Timer1, TIME_ENTER_EDIT );
                //редактирование выключено, выход из режима просмотра параметров
                if ( confirm == CONFIRM_OFF && mode_edit == false && display_mode == DISPLAY_MODE_PARAM ) {
                    //первое нажатие ENTER - включает режим редактирования параметров
                    mode_edit = true;
                    osSignalSet( tid_ThreadEdit, EVN_FIRST_ELEMENT );
                    continue;
                   }
                if ( confirm == CONFIRM_OFF && mode_edit == true && display_mode == DISPLAY_MODE_PARAM ) {
                    //следующее нажатие ENTER - перевод курсор в следующую 
                    //позицию маски или выход из режима редактирования
                    osSignalSet( tid_ThreadEdit, EVN_NEXT_ELEMENT );
                   }
                if ( confirm != CONFIRM_OFF && display_mode == DISPLAY_MODE_PARAM ) {
                    //подтверждаем сохранение параметров
                    if ( confirm == CONFIRM_YES ) {
                        if ( DataEditSave() != HAL_OK )
                            osDelay( 5000 );
                       }
                    confirm = CONFIRM_OFF;
                    //обновим информацию на дисплее
                    osSignalSet( tid_ThreadDisplayOut, EVN_DISP_UPDATE );
                   }
               }
            if ( event.value.signals & EVN_KEY_ESC ) {
                if ( mode_edit == true ) {
                    //в режиме редактирования первое нажатие - 
                    //только выход из режима редактирования
                    LCDCurOff();
                    mode_edit = false;
                    continue;
                   }
                //редактирование выключено, выход из режима просмотра параметров
                if ( mode_edit == false && display_mode == DISPLAY_MODE_PARAM )
                    DisplayMode( DISPLAY_MODE_INFO ); 
               }
            if ( event.value.signals & EVN_KEY_UP ) {
                if ( confirm != CONFIRM_OFF )
                    osSignalSet( tid_ThreadSel, EVN_SEL_NO );
                else {
                    DisplaySubMode( DIRECTION_UP ); //переключаем тип отображаемой информации
                    ChangeValue( DIRECTION_UP );
                   }
               }
            if ( event.value.signals & EVN_KEY_DOWN ) {
                if ( confirm != CONFIRM_OFF )
                    osSignalSet( tid_ThreadSel, EVN_SEL_YES );
                else {
                    DisplaySubMode( DIRECTION_DN ); //переключаем тип отображаемой информации
                    ChangeValue( DIRECTION_DN );
                   }
               }
           }
      }
 }

//****************************************************************************************************************
// Управление перемещением курсора в пределах маски изменяемого параметра
//****************************************************************************************************************
static void ThreadEdit( void const *arg ) {

    osEvent evnt;
    
    while ( true ) {
        evnt = osSignalWait( EVN_FIRST_ELEMENT, 0 );
        if ( evnt.status == osEventSignal ) {
            LCDCurOn(); //включаем и позиционируем курсор в соответствии с настройками
            ind_pos = ind_grp = 0;
            LCDGotoXY( display[display_subm].x3, display[display_subm].y3 );
           }
        evnt = osSignalWait( EVN_NEXT_ELEMENT, 0 );
        if ( evnt.status == osEventSignal ) {
            if ( ind_pos < strlen( display[display_subm].mask ) ) {
                ind_pos++; //размер и содержание маски позволяет перемещать курсор вправо
                if ( display[display_subm].mask[ind_pos] == '_' )
                    LCDGotoXY( ind_pos + display[display_subm].x3, display[display_subm].y3 );
                else {
                    ind_pos++; //возможно в маске символ который надо пропустить ":."
                    ind_grp++; 
                   }
                //повторно проверим положение курсора в маске
                if ( ind_pos < strlen( display[display_subm].mask ) )
                    LCDGotoXY( ind_pos + display[display_subm].x3, display[display_subm].y3 );
                else {
                    //вышли за пределы маски, редактирование завершено
                    LCDCurOff();
                    mode_edit = false;
                    osSignalSet( tid_ThreadSel, EVN_SEL_NO );
                   }
               }
            else {
                //вышли за пределы маски, редактирование завершено
                LCDCurOff();
                mode_edit = false;
                osSignalSet( tid_ThreadSel, EVN_SEL_NO );
               }
           }
       }
 }
 
//****************************************************************************************************************
// Обновлении информации на дисплее в режиме "текущих значений"
//****************************************************************************************************************
static void ThreadDisplay( void const *arg ) {

    while ( true ) {
        //обновляем информацию ежесекундно только в режиме просмотра текущих значений
        if ( display_mode != DISPLAY_MODE_INFO ) {
            osSignalClear( tid_ThreadDisplay, EVN_SEC_TIMER );
            continue;
           }
        osSignalWait( EVN_SEC_TIMER, osWaitForever );
        //обновим информацию на дисплее
        osSignalSet( tid_ThreadDisplayOut, EVN_DISP_UPDATE );
       }
 }

//****************************************************************************************************************
// Таймер определения длительного нажатия ENTER и переход в режим просмотра/редактирования параметров
//****************************************************************************************************************
static void Timer1_Callback( void const *arg ) {

    if ( HAL_GPIO_ReadPin( KEY_ENTER_GPIO_Port, KEY_ENTER_Pin ) == GPIO_PIN_RESET ) {
        DisplayMode( DISPLAY_MODE_PARAM ); //включаем режим просмотра параметров
        osSignalSet( tid_ThreadBeep, EVN_BEEP_SHORT );
       }
 }

//****************************************************************************************************************
// Таймер определения длительной паузы между нажатиями клавиш и переход из 
// режима просмотра/редактирования параметров в режим просмотра текущих значений
//****************************************************************************************************************
static void Timer2_Callback( void const *arg ) {

    if ( display_mode == DISPLAY_MODE_PARAM ) {
        LCDCurOff();
        mode_edit = false;
        DisplayMode( DISPLAY_MODE_INFO ); //возвращаемся в режим просмотра текущих значений
       }
 }

//****************************************************************************************************************
// Таймер завершения вывода сообщения "Data logger..." и переход в режим текущих значений
//****************************************************************************************************************
static void Timer3_Callback( void const *arg ) {

    DisplayMode( DISPLAY_MODE_INFO );
 }

//****************************************************************************************************************
// Вывод информации на дисплей
// Вызов из DisplayMode(), ThreadDisplay(), DisplaySubMode()
//****************************************************************************************************************
static void ThreadDisplayOut( void const *arg ) {

    uint8_t pos;
    timedate tm;
    char str1[20], str2[20];

    while ( true ) {
        //ждем сигнала для обновления информации
        osSignalWait( EVN_DISP_UPDATE, osWaitForever );
        if ( display_mode == DISPLAY_MODE_LOGO ) {
            //вывод заставки
            LCDCls();
            LCDGotoXY( 2, 1 );
            LCDPuts( "Mercury  200.2 Data logger V2.0" );
           }
        if ( display_mode == DISPLAY_MODE_INFO ) {
            if ( !flg_cls ) {
                LCDCls();
                flg_cls = 1; //блокируем очистку экрана
               }
            if ( display_subm == DISPLAY_INFO_TIME ) {
                //вывод время/дата
                GetTimeDate( &tm );
                LCDGotoXY( 5, 1 );
                sprintf( str1, "%02u:%02u:%02u", tm.td_hour, tm.td_min, tm.td_sec );
                LCDPuts( str1 );
                LCDGotoXY( 4, 2 );
                sprintf( str2, "%02u.%02u.%04u", tm.td_day, tm.td_month, tm.td_year );
                LCDPuts( str2 );
               }
            if ( display_subm == DISPLAY_INFO_INSTVAL ) {
                //вывод мгновенных значений счетчика
                LCDGotoXY( 1, 1 );
                sprintf( str1, "U=%05.1fV", ((float)GetData( INSTVAL_VOLTAGE ))/10 );
                LCDPuts( str1 );
                LCDGotoXY( 10, 1 );
                sprintf( str1, "I=%.2fA ", ((float)GetData( INSTVAL_CURRENT ))/100 );
                LCDPuts( str1 );
                sprintf( str2, "P=%05uW", GetData( INSTVAL_POWER ) );
                LCDGotoXY( 1, 2 );
                LCDPuts( str2 );
               }
            if ( display_subm == DISPLAY_INFO_TARIFF ) {
                //вывод накопленных значений тарифов
                LCDGotoXY( 1, 1 );
                sprintf( str1, "День:%08.2fkWh", ((float)GetData( INSTVAL_TARIFF1 ))/100 );
                LCDPuts( str1 );
                LCDGotoXY( 1, 2 );
                sprintf( str2, "Ночь:%08.2fkWh", ((float)GetData( INSTVAL_TARIFF2 ))/100 );
                LCDPuts( str2 );
               }
            if ( display_subm == DISPLAY_INFO_LINKSTAT ) {
                //вывод состояние связи со счетчиком
                LCDGotoXY( 1, 1 );
                LCDPuts( "Состояние связи" );
                //центрирование строки состояния
                sprintf( str2, "%s", GetStatus() );
                pos = ( 16 - strlen( str2 ) )/2;
                if ( !pos )
                    pos = 1;
                LCDGotoXY( pos, 2 );
                LCDPuts( str2 );
               }
            if ( display_subm == DISPLAY_INFO_SDSTAT ) {
                //вывод кол-ва ошибок записи данных в файлы
                LCDGotoXY( 2, 1 );
                LCDPuts( "Ошибки файлов:" );
                LCDGotoXY( 1, 2 );
                sprintf( str2, "MD:%04u FO:%05u", DataLogerError( GET_ERROR_MAKE_DIR ), DataLogerError( GET_ERROR_OPEN_FILE ) );
                LCDPuts( str2 );
               }
           }
        //*********************************************************************************************
        // вывод значений параметров настройки
        //*********************************************************************************************
        if ( display_mode == DISPLAY_MODE_PARAM ) {
            flg_cls = 0;
            LCDCls();
            LCDGotoXY( display[display_subm].x1, display[display_subm].y1 );
            LCDPuts( (char *)display[display_subm].str1 );
            LCDGotoXY( display[display_subm].x2, display[display_subm].y2 );
            if ( display_subm == DISPLAY_PARAM_MERCNUMB )
                sprintf( str2, display[display_subm].str2, GlbParamGet( GLB_MERCURY_NUMB, GLB_PARAM_VALUE ) );
            if ( display_subm == DISPLAY_PARAM_MERCSPEED )
                sprintf( str2, display[display_subm].str2, GlbParamGet( GLB_MERCURY_SPEED, GLB_PARAM_VALUE ) );
            if ( display_subm == DISPLAY_PARAM_SETTIME ) {
                GetTimeDate( &tm );
                sprintf( str2, display[display_subm].str2, tm.td_hour, tm.td_min, tm.td_sec );
               }
            if ( display_subm == DISPLAY_PARAM_SETDATE ) {
                GetTimeDate( &tm );
                sprintf( str2, display[display_subm].str2, tm.td_day, tm.td_month, tm.td_year );
               }
            if ( display_subm == DISPLAY_PARAM_DEVID )
                sprintf( str2, display[display_subm].str2, GlbParamGet( GLB_MBUS_ID, GLB_PARAM_VALUE ) );
            if ( display_subm == DISPLAY_PARAM_DEVSPEED )
                sprintf( str2, display[display_subm].str2, GlbParamGet( GLB_MBUS_SPEED, GLB_PARAM_VALUE ) );
            if ( display_subm == DISPLAY_PARAM_LOGGING )
                sprintf( str2, display[display_subm].str2, GlbParamGet( GLB_DATA_LOG, GLB_PARAM_VALUE ) ? "Да " : "Нет" );
            if ( display_subm == DISPLAY_PARAM_INTVLOG )
                sprintf( str2, display[display_subm].str2, GlbParamGet( GLB_LOG_INTERVAL, GLB_PARAM_VALUE ) );
            SetDataEdit( str2 );
            LCDPuts( str2 );
           }
       }
 }

//****************************************************************************************************************
// Смена режимов отображения информации на дисплее
//****************************************************************************************************************
static void DisplaySubMode( uint8_t direction ) {

    if ( mode_edit == true )
        return; //в режиме редактирования ничего не переключаем
    if ( display_mode == DISPLAY_MODE_INFO ) {
        //отображение текущих значений
        if ( direction == DIRECTION_UP ) {
            display_subm++;
            if ( display_subm >= DISPLAY_INFO_FIRST )
                display_subm = DISPLAY_INFO_TIME;
           }
        if ( direction == DIRECTION_DN ) {
            display_subm--;
            if ( !display_subm )
                display_subm = DISPLAY_INFO_FIRST - 1;
           }
       }
    if ( display_mode == DISPLAY_MODE_PARAM ) {
        //отображение параметров
        if ( direction == DIRECTION_UP ) {
            display_subm++;
            if ( display_subm >= DISPLAY_PARAM_FIRST )
                display_subm = DISPLAY_PARAM_MERCNUMB;
           }
        if ( direction == DIRECTION_DN ) {
            display_subm--;
            if ( !display_subm )
                display_subm = DISPLAY_PARAM_FIRST - 1;
           }
       }
    //обновим информацию на дисплее
    osSignalSet( tid_ThreadDisplayOut, EVN_DISP_UPDATE );
 }

//****************************************************************************************************************
// Заполняет массив uint8_t val_po[] цифровыми значениями для последующей корректировки
//****************************************************************************************************************
static void SetDataEdit( char *str ) {

    uint8_t i, ch;

    for ( i = 0; i < sizeof( val_pos ); i++ )
        val_pos[i] = 0;
    if ( strlen( display[display_subm].mask ) ) {
        //маска определена, заполним массив uint8_t значениями для коррекции
        for ( i = 0; i < strlen( display[display_subm].mask ); i++ ) {
            ch = *( str + i + ( display[display_subm].x3 - display[display_subm].x2 ) );
            if ( isdigit( ch ) )
                val_pos[i] = ch - 0x30; //число
            else val_pos[i] = ch;       //символ
           }
       }
    else {
        //маски нет, корректируем только индекс значения параметра
        if ( display_subm == DISPLAY_PARAM_MERCSPEED )
            val_pos[0] = GlbParamGet( GLB_MERCURY_SPEED, GLB_PARAM_INDEX );
        if ( display_subm == DISPLAY_PARAM_DEVSPEED )
            val_pos[0] = GlbParamGet( GLB_MBUS_SPEED, GLB_PARAM_INDEX );
        if ( display_subm == DISPLAY_PARAM_LOGGING )
            val_pos[0] = GlbParamGet( GLB_DATA_LOG, GLB_PARAM_VALUE );
       }
 }

//****************************************************************************************************************
// Изменяем значения параметра с контролем предельных значений
// uint8_t direction - признак: уменьшение/увеличение значения
//****************************************************************************************************************
static void ChangeValue( uint8_t direction ) {

    timedate dchk;
    uint8_t min_dig, max_dig;
    uint32_t min_val, max_val, curr_value;
    
    if ( display_mode != DISPLAY_MODE_PARAM || mode_edit != true )
        return;
    if ( display[display_subm].preset_grp == true ) {
        //определена группа, предельные значения для каждой группы берем из массива display[].min_grp[] display[].max_grp[]
        //предельные значения для каждого разряда фиксированы
        min_dig = 0;
        max_dig = 9;
        min_val = display[display_subm].min_grp[ind_grp];
        max_val = display[display_subm].max_grp[ind_grp];
       }
    else {
        if ( strlen( display[display_subm].mask ) ) {
            //группа не определена, но есть маска шаблона
            min_dig = 0;
            max_dig = 9;
            min_val = display[display_subm].min;
            max_val = display[display_subm].max - 1;
           }
        else {
            //группа не определена и нет шаблона, предельные значения берем из массива display[].min display[].max
            min_dig = min_val = display[display_subm].min;
            max_dig = max_val = display[display_subm].max - 1;
           }
       }
    //проверим предельные значения для одного разряда
    if ( direction == DIRECTION_UP && val_pos[ind_pos] < max_dig )
        val_pos[ind_pos]++;
    if ( direction == DIRECTION_DN && val_pos[ind_pos] > min_dig )
        val_pos[ind_pos]--;
    //проверим предельные значения для группы после изменения
    curr_value = GetDataEdit( ind_grp );
    GetTimeDate( &dchk );
    if ( display_subm == DISPLAY_PARAM_SETTIME ) {
        //проверка значений "время" перед записью
        dchk.td_hour = GetDataEdit( ELEMENT_HOUR );
        dchk.td_min = GetDataEdit( ELEMENT_MIN );
        dchk.td_sec = GetDataEdit( ELEMENT_SEC );
       }
    if ( display_subm == DISPLAY_PARAM_SETDATE ) {
        //проверка значений "дата" перед записью
        dchk.td_day = GetDataEdit( ELEMENT_DAY );
        dchk.td_month = GetDataEdit( ELEMENT_MONTH );
        dchk.td_year = GetDataEdit( ELEMENT_YEAR );
       }
    if ( display_subm == DISPLAY_PARAM_SETTIME || display_subm == DISPLAY_PARAM_SETDATE  ) {
        if ( direction == DIRECTION_UP && ( curr_value > max_val || RTCCheckDate( &dchk ) ) ) {
            val_pos[ind_pos]--; //возвращаем значение назад
            osSignalSet( tid_ThreadBeep, EVN_BEEP_LONG );
           }
        if ( direction == DIRECTION_DN && ( curr_value < min_val || RTCCheckDate( &dchk ) ) ) {
            val_pos[ind_pos]++; //возвращаем значение назад
            osSignalSet( tid_ThreadBeep, EVN_BEEP_LONG );
           }
       }
    else {
        //проверка для всех остальных значений
        if ( direction == DIRECTION_UP && curr_value > max_val ) {
            val_pos[ind_pos]--; //возвращаем значение назад
            osSignalSet( tid_ThreadBeep, EVN_BEEP_LONG );
           }
        if ( direction == DIRECTION_DN && curr_value < min_val ) {
            val_pos[ind_pos]++; //возвращаем значение назад
            osSignalSet( tid_ThreadBeep, EVN_BEEP_LONG );
           }
       }
    DataEditOut(); //обновим информацию на дисплее
 }

//****************************************************************************************************************
// Выводит на дисплей изменные значения из массива uint8_t val_pos[]
//****************************************************************************************************************
static void DataEditOut( void ) {
 
    uint8_t i;
    char out[20];
    
    memset( out, 0x00, sizeof( out ) );
    //курсор в позицию вывода всего значения
    LCDGotoXY( display[display_subm].x3, display[display_subm].y2 );
    //вывод измененного значения
    if ( strlen( display[display_subm].mask ) ) {
        for ( i = 0; i < strlen( display[display_subm].mask ); i++ ) {
            if ( val_pos[i] < 10 )
                *( out + i ) = val_pos[i] + 0x30; //число
            else *( out + i ) = val_pos[i];       //символ
           }
        LCDPuts( out );
       }
    else {
        //маски нет, выводим значение по индексу
        if ( display_subm == DISPLAY_PARAM_LOGGING )
            sprintf( out, "%s", val_pos[0] ? "Да " : "Нет" );
        else sprintf( out, "%u ", GlbValueIndex( val_pos[0] ) );
        LCDPuts( out );
       }
    //курсор на место корректировки
    LCDGotoXY( display[display_subm].x3 + ind_pos, display[display_subm].y2 );
 }
 
//****************************************************************************************************************
// Возвращает значения из массива uint8_t val_pos[] с учетом элементов между разделителями
// uint8_t element - номер элемента 0...N
//****************************************************************************************************************
static uint32_t GetDataEdit( uint8_t element ) {

    uint8_t i, dig;
    uint32_t offset, value = 0;
    char *elm_str, temp[TEMPLATE_MASK];
    
    if ( !strlen( display[display_subm].mask ) )
        return val_pos[0]; //если маска не задана, всегда возвращаем значение из "0" элемента
    memset( temp, 0x00, sizeof( temp ) );
    strcpy( temp, display[display_subm].mask );
    //проверим наличие разделителя для маски
    if ( strlen( display[display_subm].delim ) ) {
        //указан разделитель, разберем по элементам, поиск разделителя в маске ввода
        i = 0;
        elm_str = strtok( temp, display[display_subm].delim ); //адрес до первого разделителя
        while ( elm_str != NULL ) {
            if ( i == element ) {
                offset = elm_str - temp; //расчет смещение (индекса) для выборки из val_pos[]
                break;
               }
            //следущий элемент
            elm_str = strtok( NULL, display[display_subm].delim );
            i++;
           }
        dig = strlen( elm_str ) - 1;
        for ( i = 0; i < strlen( elm_str ); i++, dig-- )
            value += val_pos[offset + i] * (uint32_t)pow( 10, dig );
       }
    else {
        //разделителя нет, преобразуем как одно число
        dig = strlen( temp ) - 1;
        for ( i = 0; i < strlen( temp ); i++, dig-- )
            value += val_pos[i] * (uint32_t)pow( 10, dig );
       }
    return value;
 }

//****************************************************************************************************************
// Запись нового значения после редактирования
// Запись выполняется если предыдущее значение не совпадает с текущим
// Параметр для записи определется по значению display_subm
// return = HAL_OK      - запись выполнена
//        = ERR_FLASH_* - маска ошибки 
//****************************************************************************************************************
static uint8_t DataEditSave( void ) {

    timedate tm;
    char *err;
    uint8_t pos, result = HAL_OK;
    uint32_t old_val, new_val;
    uint16_t val1, val2, val3;

    //номер счетчика
    if ( display_subm == DISPLAY_PARAM_MERCNUMB ) {
        old_val = GlbParamGet( GLB_MERCURY_NUMB, GLB_PARAM_VALUE );
        new_val = GetDataEdit( 0 );
        if ( old_val != new_val )
            result = GlbParamSave( GLB_MERCURY_NUMB, new_val );
       }
    //скорость обмена данными со счетчиком
    if ( display_subm == DISPLAY_PARAM_MERCSPEED ) {
        old_val = GlbParamGet( GLB_MERCURY_SPEED, GLB_PARAM_VALUE );
        new_val = GetDataEdit( 0 );
        if ( old_val != new_val )
            result = GlbParamSave( GLB_MERCURY_SPEED, new_val );
       }
    //установка времени
    if ( display_subm == DISPLAY_PARAM_SETTIME ) {
        GetTimeDate( &tm );
        val1 = GetDataEdit( ELEMENT_HOUR ); //измененное значение часов
        val2 = GetDataEdit( ELEMENT_MIN );  //измененное значение минут
        val3 = GetDataEdit( ELEMENT_SEC );  //измененное значение секунд
        if ( tm.td_hour != val1 || tm.td_min != val2 || tm.td_sec != val3 ) {
            //значения не совпали, сохраним
            tm.td_hour = val1;
            tm.td_min = val2;
            tm.td_sec = val3;
            result = SetTimeDate( &tm );
            if ( result != HAL_OK ) {
                LCDCls();
                LCDGotoXY( 2, 1 );
                LCDPuts( "Ошибка записи" );
                err = RTCDescErr( result );
                //центрирование строки с ошибкой
                pos = ( 16 - strlen( err ) ) / 2;
                if ( !pos )
                    pos = 1;
                LCDGotoXY( pos, 2 );
                LCDPuts( err );
                return result;
               }
           }
       }
    //установка даты
    if ( display_subm == DISPLAY_PARAM_SETDATE ) {
        //т.к. SetTimeDate сохранит и время и дату одновременно, сначала получим текущие значения время/даты
        GetTimeDate( &tm );
        val1 = GetDataEdit( ELEMENT_DAY );   //измененное значение день
        val2 = GetDataEdit( ELEMENT_MONTH ); //измененное значение месяц
        val3 = GetDataEdit( ELEMENT_YEAR );  //измененное значение год
        if ( tm.td_day != val1 || tm.td_month != val2 || tm.td_year != val3 ) {
            //значения не совпали, сохраним
            tm.td_day = val1;
            tm.td_month = val2;
            tm.td_year = val3;
            result = SetTimeDate( &tm );
            if ( result != HAL_OK ) {
                LCDCls();
                LCDGotoXY( 2, 1 );
                LCDPuts( "Ошибка записи" );
                err = RTCDescErr( result );
                //центрирование строки с ошибкой
                pos = ( 16 - strlen( err ) ) / 2;
                LCDGotoXY( pos, 2 );
                LCDPuts( err );
                return result;
               }
           }
       }
    //номер устройства в сети ModBus
    if ( display_subm == DISPLAY_PARAM_DEVID ) {
        old_val = GlbParamGet( GLB_MBUS_ID, GLB_PARAM_VALUE );
        new_val = GetDataEdit( 0 );
        if ( old_val != new_val )
            result = GlbParamSave( GLB_MBUS_ID, new_val );
       }
    //скорость обмена в сети ModBus
    if ( display_subm == DISPLAY_PARAM_DEVSPEED ) {
        old_val = GlbParamGet( GLB_MBUS_SPEED, GLB_PARAM_VALUE );
        new_val = GetDataEdit( 0 );
        if ( old_val != new_val )
            result = GlbParamSave( GLB_MBUS_SPEED, new_val );
       }
    //логирование данных счетчика
    if ( display_subm == DISPLAY_PARAM_LOGGING ) {
        old_val = GlbParamGet( GLB_DATA_LOG, GLB_PARAM_VALUE );
        new_val = GetDataEdit( 0 );
        if ( old_val != new_val )
            result = GlbParamSave( GLB_DATA_LOG, new_val );
       }
    //логирование данных счетчика
    if ( display_subm == DISPLAY_PARAM_INTVLOG ) {
        old_val = GlbParamGet( GLB_LOG_INTERVAL, GLB_PARAM_VALUE );
        new_val = GetDataEdit( 0 );
        if ( old_val != new_val )
            result = GlbParamSave( GLB_LOG_INTERVAL, new_val );
       }
    if ( result != HAL_OK ) {
        LCDCls();
        LCDGotoXY( 2, 1 );
        LCDPuts( "Ошибка записи" );
        err = FlashDescErr( result );
        //центрирование строки с ошибкой
        pos = ( 16 - strlen( err ) )/2;
        LCDGotoXY( pos, 2 );
        LCDPuts( err );
       }
    return result;
 }

//****************************************************************************************************************
// Вызов меню запроса подтверждения записи данных
//****************************************************************************************************************
static void ThreadSelect( void const *arg ) {

    osEvent event;
    
    while ( true ) {
        event = osSignalWait( EVN_KEY_ANY, osWaitForever );
        if ( event.status == osEventSignal ) {
            LCDCls();
            LCDGotoXY( 3, 1 );
            LCDPuts( "Сохранить ?" );
            //проверим маску сигнала
            if ( event.value.signals & EVN_SEL_YES ) {
                confirm = CONFIRM_YES;
                LCDGotoXY( 2, 2 );
                LCDPuts( "<Да>     Нет" );
               }
            if ( event.value.signals & EVN_SEL_NO ) {
                confirm = CONFIRM_NO;
                LCDGotoXY( 3, 2 );
                LCDPuts( "Да     <Нет>" );
               }
           }
      }
 }

//****************************************************************************************************************
// Поток управления звуковым сигналом
//****************************************************************************************************************
static void ThreadBeep( void const *arg ) {

    osEvent event;
    
    while ( true ) {
        event = osSignalWait( EVN_BEEP_ANY, osWaitForever );
        if ( event.status == osEventSignal ) {
            //проверим маску сигнала
            if ( event.value.signals & EVN_BEEP_SHORT ) {
                beep_enable = true;
                osTimerStart( id_Timer4, BEEP_SHORT_TIME );
               }
            if ( event.value.signals & EVN_BEEP_LONG ) {
                beep_enable = true;
                osTimerStart( id_Timer4, BEEP_LONG_TIME );
               }
           }
      }
 }
 
//****************************************************************************************************************
// Таймер выключение звукового сигнала после окончания заданной длительности
//****************************************************************************************************************
static void Timer4_Callback( void const *arg ) {

    beep_enable = false; //выключаем звуковой сигнал
    HAL_GPIO_WritePin( BUZ_GPIO_Port, BUZ_Pin, GPIO_PIN_RESET );
 }

//****************************************************************************************************************
// Индикация состояния контроллера
//****************************************************************************************************************
void ThreadLed( void const *arg ) {

    while ( true ) {
        HAL_IWDG_Refresh( &hiwdg );
        osDelay( 1000 );
        HAL_GPIO_WritePin( LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_RESET );
        osDelay( 200 );
        HAL_GPIO_WritePin( LED_CTRL_GPIO_Port, LED_CTRL_Pin, GPIO_PIN_SET );
      } 
 }

