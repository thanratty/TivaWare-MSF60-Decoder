
/********************************************************************************
 * @file    hardware.h
 * @author  Tony Hanratty
 * @date    19-May-2024
 *
 * @brief   Functions for optional LED and debug UART
 ********************************************************************************/

#ifndef _HARDWARE_H_
#define _HARDWARE_H_


// System includes
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Our includes
#include "config.h"





#if (HW_ENABLE_DEBUG_UART==1) && defined(DEBUG)

bool Debug_InitUART( void );
void Debug_FlushTxBuffer(void);
uint32_t Debug_write(const char *pcBuf, uint32_t ui32Len);
void Debug_printf(const char *pcString, ...);
void Debug_vprintf(const char *pcString, va_list vaArgP);

#else

#define     Debug_InitUART()
#define     Debug_FlushTxBuffer()
#define     Debug_write(buf, len)
#define     Debug_printf(format, ...)
#define     Debug_vprintf(format, valist)


#endif  // (HW_ENABLE_DEBUG_UART==1) && defined(DEBUG)






#if (HW_ENABLE_LED == 1)

void InitLED( void );
void LEDon( void );
void LEDoff( void );
void SetLED( bool state );

#else

#define     InitLED()
#define     LEDon()
#define     LEDoff()
#define     SetLED( dummy )

#endif  // (HW_ENABLE_LED == 1)



#endif  // _HARDWARE_H_

