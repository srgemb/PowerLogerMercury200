/**
  ******************************************************************************
  * @file   fatfs.c
  * @brief  Code for fatfs applications
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2019 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under Ultimate Liberty license
  * SLA0044, the "License"; You may not use this file except in compliance with
  * the License. You may obtain a copy of the License at:
  *                             www.st.com/SLA0044
  *
  ******************************************************************************
  */

#include "fatfs.h"
#include "xtime.h"

uint8_t retUSER;    /* Return value for USER */
char USERPath[4];   /* USER logical drive path */
FATFS USERFatFS;    /* File system object for USER logical drive */
FIL USERFile;       /* File object for USER */

/* USER CODE BEGIN Variables */

/* USER CODE END Variables */    

void MX_FATFS_Init(void) 
{
  /*## FatFS: Link the USER driver ###########################*/
  retUSER = FATFS_LinkDriver(&USER_Driver, USERPath);

  /* USER CODE BEGIN Init */
  /* additional user code for init */     
  /* USER CODE END Init */
}

/**
  * @brief  Gets Time from RTC 
  * @param  None
  * @retval Time in DWORD
  */
DWORD get_fattime(void) {

    /* USER CODE BEGIN get_fattime */
    //Функция вернет время, упакованное в величину типа DWORD. Битовые поля этого значения следующие:
    //bit31:25 Год (Year), начиная с 1980 (0..127)
    //bit24:21 Месяц (Month, 1..12)
    //bit20:16 День месяца (Day in month 1..31)
    //bit15:11 Час (Hour, 0..23)
    //bit10:5  Минута (Minute, 0..59)
    //bit4:0   Количество секунд (Second), поделенное пополам (0..29)  
    timedate tm;
    uint32_t result;
    
    GetTimeDate( &tm );
    result  = ( (uint32_t)tm.td_year - 1980 ) << 25;
    result |= ( (uint32_t)tm.td_month ) << 21;
    result |= ( (uint32_t)tm.td_day ) << 16;
    result |= ( (uint32_t)tm.td_hour ) << 11;
    result |= ( (uint32_t)tm.td_min ) << 5;
    result |= ( (uint32_t)tm.td_sec / 2 );
    
    return result;
    /* USER CODE END get_fattime */  
 }

/* USER CODE BEGIN Application */
     
/* USER CODE END Application */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
