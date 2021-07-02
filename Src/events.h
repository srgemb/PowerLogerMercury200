

#ifndef __EVENTS_H
#define __EVENTS_H

#include "cmsis_os.h"

//Флаги сигналов управления
#define EVN_KEY_ENTER           0x0001      //нажатие клавиши ENTER
#define EVN_KEY_ESC             0x0002      //нажатие клавиши ESC
#define EVN_KEY_UP              0x0004      //нажатие клавиши UP
#define EVN_KEY_DOWN            0x0008      //нажатие клавиши DOWN
#define EVN_KEY_ANY             0x0000      //нажатие любой клавиши

#define EVN_SEC_TIMER           0x0010      //секундный сигнал от таймера RTC

#define EVN_DISP_UPDATE         0x0020      //обновим информацию на дисплее

#define EVN_FIRST_ELEMENT       0x0040      //включает режим редактирования параметров
#define EVN_NEXT_ELEMENT        0x0080      //переход на следующий элемент редактирования

#define EVN_BEEP_SHORT          0x0100      //подача короткого звукового сигнала
#define EVN_BEEP_LONG           0x0200      //подача длиного звукового сигнала
#define EVN_BEEP_ANY            0x0000      //подача звукового сигнала

#define EVN_SEL_YES             0x0400      //в меню подтверждения записи выделить выбор "Да"
#define EVN_SEL_NO              0x0800      //в меню подтверждения записи выделить выбор "Нет"

#define EVN_LOG_DATA            0x1000      //сохранение текущих данных (V,I,P)
#define EVN_LOG_TARIFF          0x2000      //сохранение текущих значений тарифов
#define EVN_LOG_ANY             0x0000      //сохранение данных

#define EVN_485_RECV            0x4000      //
#define EVN_485_TIMER           0x8000      //

#endif
