
//****************************************************************************************************************
//
// Функционал управления LCD индикатором
//
//****************************************************************************************************************

#include <stdint.h>
#include <stdbool.h>

#include "lcd.h"
#include "lcd_def.h"
#include "stm32f1xx_hal.h"
#include "data.h"
#include "main.h"

//****************************************************************************************************************
// Локальные константы
//****************************************************************************************************************
#define DATA_SHIFT          3       //кол-во сдвигов в право для получения данных в младшей тетраде
#define DURATION_LCD_E      10      //длительность сигнала "E" в тактах

//****************************************************************************************************************
// Локальные переменные
//****************************************************************************************************************
static char lcd_ptr;                //текущее кол-во символов выведенных на дисплей

//Таблица перекодировки WIN1251 -> HD44780
static const uint8_t WinToLcd[] = {
    0x41, 0xA0, 0x42, 0xA1, 0xE0, 0x45, 0xA3, 0xA4, 0xA5, 0xA6, 0x4B, 0xA7, 0x4D, 0x48, 0x4F, 0xA8,
    0x50, 0x43, 0x54, 0xA9, 0xAA, 0x58, 0xE1, 0xAB, 0xAC, 0xE2, 0xAD, 0xAE, 0x62, 0xAF, 0xB0, 0xB1,
    0x61, 0xB2, 0xB3, 0xB4, 0xE3, 0x65, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xBB, 0xBC, 0xBD, 0x6F, 0xBE,
    0x70, 0x63, 0xBF, 0x79, 0xE4, 0x78, 0xE5, 0xC0, 0xC1, 0xE6, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7
 };

//8 графаческихх символов для отображения гистограммы
static const uint8_t UserFont[8][8] = {
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x10 },
    { 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18 },
    { 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C, 0x1C },
    { 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E, 0x1E },
    { 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F, 0x1F },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 },
    { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }
 };

//****************************************************************************************************************
// Локальные прототипы функций
//****************************************************************************************************************
static void lcd_write( uint8_t ch );
static void lcd_write_4bit( uint8_t ch );
static uint8_t lcd_rd_stat( void );
static void lcd_wr_cmd( uint8_t ch );
static void lcd_wr_data( uint8_t dat );
static void lcd_wait_busy( void );
static uint8_t lcd_rus_decode( uint8_t ch );
static void LCDLoad( uint8_t *fp, uint32_t cnt );
static void delay( uint32_t cnt );

//****************************************************************************************************************
// Инициализация LCD индикатора
//****************************************************************************************************************
void LCDInit( void ) {

    //начальное состояние выходов управления
    HAL_GPIO_WritePin( LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET );
    HAL_GPIO_WritePin( GPIOB, LCD_D4_Pin | LCD_D5_Pin | LCD_D6_Pin | LCD_D7_Pin, GPIO_PIN_RESET );

    lcd_write_4bit( LCD_MAIN_MODE | LCD_DATA8 );
    delay( 100000 );
    lcd_write_4bit( LCD_MAIN_MODE | LCD_DATA8 );
    delay( 10000 );
    lcd_write_4bit( LCD_MAIN_MODE | LCD_DATA8 );
    lcd_write_4bit( LCD_MAIN_MODE | LCD_DATA4 );
    //2 lines, 5x8 character matrix
    lcd_wr_cmd( LCD_MAIN_MODE | LCD_2LINE | LCD_FONT_5X8 );
    //Display ctrl:Disp/Curs/Blnk=ON
    lcd_wr_cmd( LCD_CURS_MODE | LCD_DISPLAY_ON | LCD_CURSOR_OFF | LCD_BLINK_OFF );
    //Entry mode: Move right, no shift
    lcd_wr_cmd( LCD_CNT_SHIFT | LCD_INC_ADDR );
    //загрузка символов гистограммы
    LCDLoad( (uint8_t*)&UserFont, sizeof( UserFont ) );
    LCDCls();
 }

//****************************************************************************************************************
// Очистка LCD дисплея, курсор в позицию 1/1
//****************************************************************************************************************
void LCDCls( void ) {
    
    lcd_wr_cmd( LCD_CLS );
    LCDGotoXY( 1, 1 );
 }

//****************************************************************************************************************
// Включить вывод на LCD дисплей (курсор выключен)
//****************************************************************************************************************
void LCDOn( void ) {

    lcd_wr_cmd( LCD_CURS_MODE | LCD_DISPLAY_ON );
 }

//****************************************************************************************************************
// Выключение курсора LCD дисплея
//****************************************************************************************************************
void LCDCurOff( void ) {

    lcd_wr_cmd( LCD_CURS_MODE | LCD_DISPLAY_ON | LCD_CURSOR_OFF );
 }

//****************************************************************************************************************
// Включение курсора LCD дисплея
//****************************************************************************************************************
void LCDCurOn( void ) {

    lcd_wr_cmd( LCD_CURS_MODE | LCD_DISPLAY_ON | LCD_CURSOR_ON | LCD_BLINK_ON );
 }

//****************************************************************************************************************
// Установка курсора на дисплее позиция/строка.
// uint8_t x - позиция в строке (1-16)
// uint8_t y - номер строки (1-2)
//****************************************************************************************************************
void LCDGotoXY( uint8_t x, uint8_t y ) {

    uint8_t c;

    c = --x;
    if ( --y )
        c |= 0x40;
    lcd_wr_cmd( LCD_SET_CURSOR | c );
    lcd_ptr = y*16 + x;
 }

//****************************************************************************************************************
// Вывод символа на LCD дисплей с перекодировкой WIN1251 -> HD44780
// uint8_t ch - код символа
//****************************************************************************************************************
void LCDPutc( uint8_t ch ) { 

    if ( lcd_ptr == 16 )
        lcd_wr_cmd( 0xC0 ); //перевод на новую строку
    lcd_wr_data( lcd_rus_decode( ch ) );
    lcd_ptr++;
 }

//****************************************************************************************************************
// Вывод строки на LCD дисплей с перекодировкой WIN1251 -> HD44780
// char *str - адрес строки для отображения
//****************************************************************************************************************
void LCDPuts( char *str ) {

    while( *str )
        LCDPutc( *str++ );
 }

//****************************************************************************************************************
// Отображение гистограммы на индикаторе
// uint32_t val  - значение гистограммы 0..100 % 
// uint32_t size - кол-во позиций (знакомест) для отображения гистограммы 1..16
//****************************************************************************************************************
void LCDBarGraph( uint32_t val, uint32_t size ) {
  
    uint32_t i;

    val = val * size / 20;
    for ( i = 0; i < size; i++ ) {
        if ( val > 5 ) {
            LCDPutc( 5 );
            val -= 5;
           }
        else {
            LCDPutc( val );
            break;
        }
    }
}

//****************************************************************************************************************
// Запись символа в LCD контроллер
//****************************************************************************************************************
static void lcd_write( uint8_t ch ) {

    lcd_write_4bit( ch >> 4 );
    lcd_write_4bit( ch );
 }

//****************************************************************************************************************
// Чтение статуса LCD контроллера
// result - код статуса контроллера
//****************************************************************************************************************
static uint8_t lcd_rd_stat( void ) {

    uint8_t stat;
    GPIO_InitTypeDef GPIO_InitStruct;
    
    //перенастраиваем шину DATA на ввод
    GPIO_InitStruct.Pin = LCD_D4_Pin | LCD_D5_Pin | LCD_D6_Pin | LCD_D7_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );
    //
    HAL_GPIO_WritePin( LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET );  //RS = 0;
    HAL_GPIO_WritePin( LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_SET );    //RW = 1;
    delay( DURATION_LCD_E );
    HAL_GPIO_WritePin( LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_SET );      //E = 1;
    delay( DURATION_LCD_E );
    //читаем старшею тетраду
    stat = ( GPIOB->IDR << 1 ) & 0xF0;
    HAL_GPIO_WritePin( LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET );    //E = 0;
    delay( DURATION_LCD_E );
    HAL_GPIO_WritePin( LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_SET );      //E = 1;
    delay( DURATION_LCD_E );
    //читаем младшую тетраду
    stat |= ( GPIOB->IDR >> DATA_SHIFT ) & 0x0F;
    HAL_GPIO_WritePin( LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET );    //E = 0;
    return stat;
 }

//****************************************************************************************************************
// Ожидание готовности LCD контроллера
//****************************************************************************************************************
static void lcd_wait_busy( void ) {

    uint8_t stat;

    do {
        stat = lcd_rd_stat();
    } while ( stat & 0x80 ); //ожидание флага готовности '0' в 7 разряде
 }

//****************************************************************************************************************
// Запись команды в контроллер LCD дисплея
// uint8_t ch - код команды
//****************************************************************************************************************
static void lcd_wr_cmd( uint8_t ch ) {

    lcd_wait_busy();
    HAL_GPIO_WritePin( LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_RESET ); //RS = 0;
    lcd_write( ch );
 }

//****************************************************************************************************************
// Запись данных в контроллер LCD дисплея
// uint8_t dat - код данных
//****************************************************************************************************************
static void lcd_wr_data( uint8_t dat ) {

    lcd_wait_busy();
    HAL_GPIO_WritePin( LCD_RS_GPIO_Port, LCD_RS_Pin, GPIO_PIN_SET ); //RS = 1;
    lcd_write( dat );
 }

//****************************************************************************************************************
// Запись 4-битной команды в LCD контроллер
// uint8_t ch - код команды (вссегда используется младшая тетрада)
//****************************************************************************************************************
static void lcd_write_4bit( uint8_t ch ) {

    GPIO_InitTypeDef GPIO_InitStruct;
    
    //перенастраиваем шину DATA на вывод                                 //IODIR &= ~LCD_DATA;
    GPIO_InitStruct.Pin = LCD_D4_Pin | LCD_D5_Pin | LCD_D6_Pin | LCD_D7_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init( GPIOB, &GPIO_InitStruct );

    HAL_GPIO_WritePin( LCD_RW_GPIO_Port, LCD_RW_Pin, GPIO_PIN_RESET );  //IOCLR = LCD_RW | LCD_DATA;
    if ( ch & 0x01 )
        HAL_GPIO_WritePin( LCD_D4_GPIO_Port, LCD_D4_Pin, GPIO_PIN_SET );
    else HAL_GPIO_WritePin( LCD_D4_GPIO_Port, LCD_D4_Pin, GPIO_PIN_RESET );
    if ( ch & 0x02 )
        HAL_GPIO_WritePin( LCD_D5_GPIO_Port, LCD_D5_Pin, GPIO_PIN_SET );
    else HAL_GPIO_WritePin( LCD_D5_GPIO_Port, LCD_D5_Pin, GPIO_PIN_RESET );
    if ( ch & 0x04 )
        HAL_GPIO_WritePin( LCD_D6_GPIO_Port, LCD_D6_Pin, GPIO_PIN_SET );
    else HAL_GPIO_WritePin( LCD_D6_GPIO_Port, LCD_D6_Pin, GPIO_PIN_RESET );
    if ( ch & 0x08 )
        HAL_GPIO_WritePin( LCD_D7_GPIO_Port, LCD_D7_Pin, GPIO_PIN_SET );
    else HAL_GPIO_WritePin( LCD_D7_GPIO_Port, LCD_D7_Pin, GPIO_PIN_RESET );

    HAL_GPIO_WritePin( LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_SET );      //IOSET = LCD_E;
    delay( DURATION_LCD_E );
    HAL_GPIO_WritePin( LCD_E_GPIO_Port, LCD_E_Pin, GPIO_PIN_RESET );    //IOCLR = LCD_E;
    delay( DURATION_LCD_E );
 }

//****************************************************************************************************************
// Перекодировать символ WIN1251 -> HD44780
//****************************************************************************************************************
static uint8_t lcd_rus_decode( uint8_t ch ) {

    if ( ch < 0xC0 )
        return ch;
    else return WinToLcd[ch-0xC0];
 }

//****************************************************************************************************************
// Загрузка символов гистограммы в CGRAM память индикатора
// uint8_t *fp  - адрес массива кодов
// uint32_t cnt - кол-во кодов для загрузки
//****************************************************************************************************************
static void LCDLoad( uint8_t *fp, uint32_t cnt ) {

    uint32_t i;

    lcd_wr_cmd( 0x40 );
    for ( i = 0; i < cnt; i++, fp++ )
        lcd_wr_data( *fp );
 }

//****************************************************************************************************************
// Выполнение задержки
//****************************************************************************************************************
static void delay( uint32_t cnt ) {

    while ( cnt-- );
 }

