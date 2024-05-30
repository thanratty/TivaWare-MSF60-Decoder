
/********************************************************************************
 * @file    logging.h
 * @author  Tony Hanratty
 * @date    28-May-2024
 *
 * @brief   Logging prototypes and category definitions
 *
 ********************************************************************************/

#ifndef _LOGGING_H_
#define _LOGGING_H_



/**
 * NOTE: logging is only possible in a Debug build with the debug
 *       UART enabled. See config.h
 *       It is not compiled into a Release build.
 *
 * If you do enable a lot of logging make sure the debug UART
 * transmit buffer is large enough.
 */



/**
 *  Enable individual logging categories
 */
#define     SHOW_LOG_INFO               0
#define     SHOW_LOG_SYNC_MSGS          1
#define     SHOW_LOG_BIT_DUMP           0
#define     SHOW_LOG_EVENTS             0
#define     SHOW_LOG_BCD_ERRORS         1
#define     SHOW_LOG_EDGE_ERRORS        1



/**
 *  Log message categories.
 */
typedef enum tag_msf_log_type {
    LOG_INFO,
    LOG_SYNC_MSG,
    LOG_BIT_DUMP,
    LOG_CARRIER_EVENT,
    LOG_EDGE_ERROR,
    LOG_BCD_ERROR
} eMSFLogType;




#if defined(DEBUG) && (HW_ENABLE_DEBUG_UART==1)
void LOGprintf(eMSFLogType type, const char *pcString, ...);
#else
#define LOGprintf(type, format, ...)
#endif



#endif // _LOGGING_H_

