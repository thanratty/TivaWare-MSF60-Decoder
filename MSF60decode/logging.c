
/********************************************************************************
 * @file    logging.c
 * @author  Tony Hanratty
 * @date    24-May-2024
 *
 * @brief   Logging support functions
 *
 * @notes   Logging support is only included if it's a Debug build _AND_
 *          the debug UART is enabled.
 *
 ********************************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "config.h"
#include "hardware.h"

#include "logging.h"



// Don't need logging functions there's no debug UART or it's a Relase build.
#if (HW_ENABLE_DEBUG_UART==1) && defined(DEBUG)


// These externs will be visible in a Debug build
extern bool getBit(uint8_t* bitarray, int bitnum);
extern uint8_t A_bits[ 8 ];
extern uint8_t B_bits[ 8 ];



#if (SHOW_LOG_BIT_DUMP == 1)

/**
 * Dump out the entire A and B arrays with the bit numbers
 *
 */
void
dumpBits( void )
{
    int i;

    Console_write("  ", 2);
    for( i=1 ; i<=59 ; i++)
        Console_printf("%c", '0' + i/10);
    Console_printf("\n  ");
    for( i=1 ; i<=59 ; i++)
        Console_printf("%c", '0' + (i % 10) );
    Console_printf("\n");

    Console_write("  ", 2);
    for( i=0 ; i<59 ; i++)
        Console_printf("%c", '-' );
    Console_printf("\n");

    Console_write("A ", 2);
    for( i=1 ; i<=59 ; i++)
        Console_printf("%c", (getBit(A_bits, i)) ? '1' : '0');
    Console_printf("\nB ");
    for( i=1 ; i<=59 ; i++)
        Console_printf("%c", (getBit(B_bits, i)) ? '1' : '0');
    Console_printf("\n");

}

#endif





void
LOGprintf( eMSFLogType type, const char *pcString, ...)
{
    va_list args;

    // Start varargs processing.
    va_start(args, pcString);

#if (SHOW_LOG_INFO == 1)
    if (type == LOG_INFO)
        Debug_vprintf(pcString, args);
    else
#endif

#if (SHOW_LOG_SYNC_MSGS == 1)
    if (type == LOG_SYNC_MSG)
        Debug_vprintf(pcString, args);
    else
#endif

#if (SHOW_LOG_EVENTS == 1)
    if (type == LOG_EVENT)
        Debug_vprintf(pcString, args);
    else
#endif

#if (SHOW_LOG_EDGE_ERRORS == 1)
    if (type == LOG_EDGE_ERROR)
        Debug_vprintf(pcString, args);
    else
#endif

#if (SHOW_LOG_BCD_ERRORS == 1)
    if (type == LOG_BCD_ERROR)
        Debug_vprintf(pcString, args);
    else
#endif

#if (SHOW_LOG_BIT_DUMP == 1)
    if (type == LOG_BIT_DUMP)
        dumpBits();
#endif


    va_end(args);
}


#endif // (HW_ENABLE_DEBUG_UART==1) && defined(DEBUG)



