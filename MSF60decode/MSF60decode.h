
/********************************************************************************
 * @file    MSF60decode.h
 * @author  Tony Hanratty
 * @date    28-May-2024
 *
 * @brief   Public data structures and API for the MSF decoder.
 ********************************************************************************/

#ifndef _MSF60DECODE_H_
#define _MSF60DECODE_H_

#include <stdint.h>
#include <stdbool.h>



/**
 * The decoder returns the date/time information in this structure
 */
typedef struct
{
    volatile bool     bHasValidTime;        // 'true' if the members below are valid
    volatile bool     bDateTimeUpdated;     // set 'true' every time these values are updated
    uint8_t  Year;                          // 0-99
    uint8_t  Month;                         // 1-12
    uint8_t  Day;                           // 1-31
    uint8_t  Hour;                          // 0-23
    uint8_t  Minute;                        // 0-59
    uint8_t  DOW;                           // 0-6 Day Of Week. Sunday = 0
    uint8_t  DST;                           // Daylight Savings Time active, 0 or 1
} sMSFDateTime;



/**
 * Decoder event types a client can be notified about via a callback.
 */
typedef enum {
    MSF_EVENT_SYNC              = 0x0001,
    MSF_EVENT_SYNC_LOST         = 0x0002,
    MSF_EVENT_DATETIME_UPDATED  = 0x0004
} eMSFEventType;



/**
 * Function type for event notifications
 */
typedef void (*MSF_EVENT_CALLBACK)( eMSFEventType );




/****************************************************************************
 *
 *                          Decoder API
 *
 ****************************************************************************/

void MSF_InitDecoder( sMSFDateTime* pdata );
void MSF_EnableEventNotifications( MSF_EVENT_CALLBACK pfunc, uint32_t enable_mask );
void MSF_EnableRadio( bool state );
bool MSF_GetSyncState( void );


#endif // _MSF60DECODE_H_

