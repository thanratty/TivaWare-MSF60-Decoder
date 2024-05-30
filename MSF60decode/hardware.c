
/********************************************************************************
 * @file    hardware.c
 * @author  Tony Hanratty
 * @date    24-May-2024
 *
 * @brief   Support for the (optional) debug UART and LED
 *
 * @notes   The debug UART & LED are enabled/configured in config.h
 *
 ********************************************************************************/


// System includes
#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdio.h>

// TI platform
#include "inc/hw_ints.h"
#include "inc/hw_memmap.h"
#include "driverlib/gpio.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/sysctl.h"
#include "driverlib/uart.h"

// Our includes
#include "config.h"

// This file's include
#include "hardware.h"




/**
 * Exclude everything if it's not a Debug build or the debug UART isn't enabled
 */
#if (HW_ENABLE_DEBUG_UART==1) && defined(DEBUG)


#define     DEBUG_UART_TX_BUFFER_SIZE   1024





//*****************************************************************************
// Transmit Buffer Macros
//*****************************************************************************

#define TX_BUFFER_EMPTY()                   (Debug_TxBufferCount()==0)
#define TX_BUFFER_FULL()                    (Debug_TxBufferCount()==DEBUG_UART_TX_BUFFER_SIZE)

#define ADVANCE_TX_BUFFER_INDEX(Index)      do {(Index) = ((Index) + 1) % DEBUG_UART_TX_BUFFER_SIZE;} while(0)





// CPU clock frequency in MHz
extern uint32_t g_SysClockSpeed;


// To map an integer 0 and 15 to its ASCII character
STATIC const char* pcHexChars = "0123456789abcdef";


/**
 * Output ring buffer. Buffer is full if TxReadIndex is one ahead of
 * TxWriteIndex. Buffer is empty if the two indices are the same.
 */
STATIC uint8_t TxBuffer[ DEBUG_UART_TX_BUFFER_SIZE ];
STATIC volatile uint32_t TxWriteIndex = 0;
STATIC volatile uint32_t TxReadIndex = 0;













/*******************************************************************
* NAME
*       Debug_FlushTxBuffer()
*
* DESCRIPTION
*       Empty the debug UART transmit buffer
*
* PARAMETERS
*       None
*
* OUTPUTS
*       None
*
* RETURNS
*       Nothing
*
* NOTES
*
********************************************************************/
void
Debug_FlushTxBuffer( void )
{
    // The remaining data should be discarded, so temporarily turn off interrupts.
    bool MasterintStatus = IntMasterDisable();

    // Flush the buffer indices
    TxReadIndex = TxWriteIndex = 0;

    // If interrupts were enabled when we turned them off, turn them back on again.
    if(!MasterintStatus)
        IntMasterEnable();
}



/*******************************************************************\
 * NAME
 *       Debug_TxBufferCount()
 *
 * DESCRIPTION
 *       Get the number of bytes in the debug UART transmit buffer
 *
 * PARAMETERS
 *        None
 *
 * OUTPUTS
 *       None
 *
 * RETURNS
 *       uint32_t        Number of bytes in the transmit buffer
 *
 * NOTES
 *
\********************************************************************/
uint32_t
Debug_TxBufferCount( void )
{
    uint32_t ui32Write = TxWriteIndex;
    uint32_t ui32Read  = TxReadIndex;

    return (ui32Write >= ui32Read)
           ? (ui32Write - ui32Read)
           : (DEBUG_UART_TX_BUFFER_SIZE - (ui32Read - ui32Write));
}






/*****************************************************************************\
 *
 * Take as many bytes from the transmit buffer as we have space for and move
 * them into the UART transmit FIFO.
 *
 * Returns # bytes written to the TX FIFO
 *
\*****************************************************************************/
STATIC void
PrimeTheTransmitFIFO( void )
{
    // Do we have any data to transmit?
    if(!TX_BUFFER_EMPTY())
    {
        // Disable the UART interrupt while we're playing with the buffer indexes
        IntDisable(DEBUG_UART_INT);

        // Yes. Take some characters out of the transmit buffer and feed them to the UART transmit FIFO.
        while(UARTSpaceAvail(DEBUG_UART_BASE) && !TX_BUFFER_EMPTY())
        {
            UARTCharPutNonBlocking(DEBUG_UART_BASE, TxBuffer[ TxReadIndex ]);
            ADVANCE_TX_BUFFER_INDEX( TxReadIndex );
        }

        IntEnable(DEBUG_UART_INT);
    }
}



/*****************************************************************************\
 *
 * UART TX interrupt handler
 *
\*****************************************************************************/
STATIC void
DebugUARTIntHandler(void)
{
    uint32_t int_status;

    // Get and clear the current interrupt source(s)
    int_status = UARTIntStatus(DEBUG_UART_BASE, true);
    UARTIntClear(DEBUG_UART_BASE, int_status);

    // TX FIFO has space available
    if(int_status & UART_INT_TX)
    {
        // If the output buffer is now empty, turn off the transmit interrupt.
        if(TX_BUFFER_EMPTY())
        {
            UARTIntDisable(DEBUG_UART_BASE, UART_INT_TX);
        }
        else
        {
            // Write as many bytes as we can into the transmit FIFO.
            PrimeTheTransmitFIFO();
            UARTIntEnable(DEBUG_UART_BASE, UART_INT_TX);
        }
    }
}



/*****************************************************************************\
 *
 * Configures the debug/logging UART.
 *
 * Fails if the configured UART peripheral is not present.
 *
\*****************************************************************************/
bool
Debug_InitUART( void )
{
    // Check to make sure the UART peripheral is present.
    if(!SysCtlPeripheralPresent(DEBUG_UART_SYS_PERIPH))
        return false;

    // Enable the UART peripheral
    SysCtlPeripheralEnable(DEBUG_UART_SYS_PERIPH);
    SysCtlPeripheralReset(DEBUG_UART_SYS_PERIPH);
    while(!SysCtlPeripheralReady(DEBUG_UART_SYS_PERIPH));

    SysCtlPeripheralEnable(DEBUG_UART_GPIO_PERIPH);
    SysCtlPeripheralReset(DEBUG_UART_GPIO_PERIPH);
    while(!SysCtlPeripheralReady(DEBUG_UART_GPIO_PERIPH));

    // Configure pin mux
    GPIOPinConfigure(DEBUG_UART_RX_PIN_CONFIG);
    GPIOPinConfigure(DEBUG_UART_TX_PIN_CONFIG);

    GPIOPinTypeUART(DEBUG_UART_GPIO_BASE, DEBUG_UART_RX_PIN | DEBUG_UART_TX_PIN );

    // Configure the UART, typically 115200-8-N-1
    UARTConfigSetExpClk(DEBUG_UART_BASE,
                        g_SysClockSpeed,
                        DEBUG_UART_BAUD,
                        (UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE ) );

    // Set the UART to interrupt whenever the TX FIFO is almost empty or when any character is received.
    UARTFIFOLevelSet(DEBUG_UART_BASE, UART_FIFO_TX1_8, UART_FIFO_RX1_8);

    Debug_FlushTxBuffer();

    // Don't enable the TX interrupt in the UART till data has been written to the TX FIFO
    UARTIntDisable(DEBUG_UART_BASE, 0xFFFFFFFF);
    IntRegister(DEBUG_UART_INT, DebugUARTIntHandler);
    IntEnable(DEBUG_UART_INT);

    // Enable the UART operation.
    UARTEnable(DEBUG_UART_BASE);

    return true;
}



/*****************************************************************************\
 *
 * Writes a string of characters to the debug UART
 *
 *      pcBuf    Pointer to a buffer containing the string.
 *      ui32Len  Length of the string to transmit.
 *
 * This function will transmit the string to the UART output. The number of
 * characters to transmit is the ui32Len parameter.  This
 * function does no interpretation or translation of any characters.
 *
 * Returns the number of characters written.
 *
\*****************************************************************************/
uint32_t
Debug_write(const char *pBuffer, uint32_t len)
{
uint32_t nWriteCount = 0;
uint32_t index;

    // Write as many characters as we can to the transmit buffer
    for(index=0 ; index < len ; index++)
    {
        if(!TX_BUFFER_FULL())
        {
            TxBuffer[ TxWriteIndex ] = pBuffer[ index ];
            ADVANCE_TX_BUFFER_INDEX( TxWriteIndex );
            nWriteCount++;
        }
        else
        {
            // Buffer is full - ignore remaining characters and return.
            break;
        }
    }


    // If there's anything in the transmit buffer, ensure UART TX interrupts are enabled.
    if(!TX_BUFFER_EMPTY())
    {
        PrimeTheTransmitFIFO();
        UARTIntEnable(DEBUG_UART_BASE, UART_INT_TX);
    }

    // Return the number of characters written.
    return nWriteCount;
}



//*****************************************************************************
//
// A simple vprintf function supporting \%c, \%d, \%p, \%s, \%u, \%x, and \%X.
//
// \param pcString is the format string.
// \param vaArgP is a vararg list pointer
//
// Only the following formatting characters are supported:
//
// - \%c to print a character
// - \%d or \%i to print a decimal value
// - \%s to print a string
// - \%u to print an unsigned decimal value
// - \%x to print a hexadecimal value using lower case letters
// - \%X to print a hexadecimal value using lower case letters (not upper case)
// - \%p to print a pointer as a hexadecimal value
// - \%\% to print out a \% character
//
// For \%s, \%d, \%i, \%u, \%p, \%x, and \%X, an optional number may reside
// between the \% and the format character, which specifies the minimum number
// of characters to use for that value; if preceded by a 0 then the extra
// characters will be filled with zeros instead of spaces.  For example,
// ``\%8d'' will use eight characters to print the decimal value with spaces
// added to reach eight; ``\%08d'' will use eight characters as well but will
// add zeroes instead of spaces.
//
//*****************************************************************************
void
Debug_vprintf(const char *pcString, va_list vaArgP)
{
    uint32_t ui32Idx, ui32Value, ui32Pos, ui32Count, ui32Base, ui32Neg;
    char *pcStr, pcBuf[16], cFill;


    // Loop while there are more characters in the string.
    while(*pcString)
    {
        // Find the first non-% character, or the end of the string.
        for( ui32Idx = 0 ;
            (pcString[ui32Idx] != '%') && (pcString[ui32Idx] != '\0') ;
            ui32Idx++ );


        // Write this portion of the string.
        Debug_write(pcString, ui32Idx);

        // Skip the portion of the string that was written.
        pcString += ui32Idx;

        // See if the next character is a %.
        if(*pcString == '%')
        {
            // Skip the %.
            pcString++;

            // Set the digit count to zero, and the fill character to space (in other words, to the defaults).
            ui32Count = 0;
            cFill = ' ';

            // It may be necessary to get back here to process more characters.
            // Goto's aren't pretty, but effective.  I feel extremely dirty for
            // using not one but two of the beasts.

again:


            // Check the next character.
            switch(*pcString++)
            {
                // Handle digits
                case '0':
                case '1':
                case '2':
                case '3':
                case '4':
                case '5':
                case '6':
                case '7':
                case '8':
                case '9':
                {
                    // If this is a zero and it is the first digit, then the fill character is a zero instead of a space.
                    if((pcString[-1] == '0') && (ui32Count == 0))
                        cFill = '0';

                    // Update the digit count.
                    ui32Count *= 10;
                    ui32Count += pcString[-1] - '0';

                    goto again;
                }


                // Handle the %c command.
                case 'c':
                    // Get the CHAR from the varargs AND PRINT IT
                    ui32Value = va_arg(vaArgP, uint32_t);
                    Debug_write((char *) &ui32Value, 1);
                    break;


                // Handle the %d and %i commands.
                case 'd':
                case 'i':
                {
                    // Get the value from the varargs.
                    ui32Value = va_arg(vaArgP, uint32_t);

                    // Reset the buffer position.
                    ui32Pos = 0;

                    // If the value is negative, make it positive and indicate that a minus sign is needed.
                    if((int32_t)ui32Value < 0)
                    {
                        // Make the value positive BUT INDICATE IT IS ACTUALLY NEGATIVE
                        ui32Value = - (int32_t) ui32Value;
                        ui32Neg = 1;
                    }
                    else
                    {
                        // Indicate that the value is positive so that a minus sign isn't inserted.
                        ui32Neg = 0;
                    }

                    // Set the base to 10.
                    ui32Base = 10;

                    goto convert;
                }


                // Handle the %s command.
                case 's':
                {
                    // Get the string pointer from the varargs.
                    pcStr = va_arg(vaArgP, char *);

                    // Determine the length of the string and write it out
                    for( ui32Idx = 0 ; pcStr[ui32Idx] != '\0' ; ui32Idx++ );
                    Debug_write(pcStr, ui32Idx);

                    // Write any required padding spaces
                    if(ui32Count > ui32Idx)
                    {
                        ui32Count -= ui32Idx;
                        while(ui32Count--)
                            Debug_write(" ", 1);
                    }
                    break;
                }


                // Handle the %u command.
                case 'u':
                    ui32Value = va_arg(vaArgP, uint32_t);                               // Get the value from the varargs.
                    ui32Pos  = 0;                                                       // Reset the buffer position.
                    ui32Base = 10;                                                      // Set the base to 10.
                    ui32Neg  = 0;                                                       // Indicate that the value is positive so that a minus sign isn't inserted.
                    goto convert;                                                       // Convert the value to ASCII


                // Handle the %x and %X commands.  Note that they are treated
                // Also alias %p to %x.
                case 'x':
                case 'X':
                case 'p':
                {
                    ui32Value = va_arg(vaArgP, uint32_t);                               // Get the value from the varargs.
                    ui32Pos   = 0;                                                      // Reset the buffer position.
                    ui32Base  = 16;                                                     // Set the base to 16.
                    ui32Neg   = 0;                                                      // Indicate that the value is positive so that a minus sign isn't inserted.

                    // Determine the number of digits in the string version of the value.
convert:
                    for( ui32Idx = 1;
                        (((ui32Idx * ui32Base) <= ui32Value) && (((ui32Idx * ui32Base) / ui32Base) == ui32Idx)) ;
                        ui32Idx *= ui32Base, ui32Count-- );

                    // If the value is negative, reduce the count of padding characters needed.
                    if(ui32Neg)
                        ui32Count--;

                    // If the value is negative and the value is padded with zeros, then place the minus sign before the padding.
                    if(ui32Neg && (cFill == '0'))
                    {
                        // Place the minus sign in the output buffer and clear the neg flag
                        pcBuf[ ui32Pos++ ] = '-';
                        ui32Neg = 0;
                    }

                    // Provide additional padding at the beginning of the string conversion if needed.
                    if((ui32Count > 1) && (ui32Count < 16)) {
                        for(ui32Count-- ; ui32Count ; ui32Count--) {
                            pcBuf[ ui32Pos++ ] = cFill;
                        }
                    }

                    // If the value is negative, insert a minus sign in the output buffer
                    if(ui32Neg) {
                        pcBuf[ ui32Pos++ ] = '-';
                    }

                    // Convert the value into a string.
                    for( ; ui32Idx ; ui32Idx /= ui32Base) {
                        pcBuf[ ui32Pos++ ] = pcHexChars[(ui32Value / ui32Idx) % ui32Base];
                    }

                    // Write the string.
                    Debug_write(pcBuf, ui32Pos);

                    break;
                }


                // Handle the %% command.
                case '%':
                    // Simply write a single %.
                    Debug_write("%", 1);
                    break;


                // Handle all other formatting commands.
                default:
                    // Indicate an error.
                    Debug_write("ERROR", 5);
                    break;
            }
        }
    }
}



//*****************************************************************************
//
//! A simple UART based printf function supporting \%c, \%d, \%p, \%s, \%u,
//! \%x, and \%X.
//!
//! \param pcString is the format string.
//! \param ... are the optional arguments, which depend on the contents of the
//! format string.
//!
//! This function is very similar to the C library <tt>fprintf()</tt> function.
//! All of its output will be sent to the UART.  Only the following formatting
//! characters are supported:
//!
//! - \%c to print a character
//! - \%d or \%i to print a decimal value
//! - \%s to print a string
//! - \%u to print an unsigned decimal value
//! - \%x tor \%X o print a hexadecimal value using lower case letters
//! - \%p to print a pointer as a hexadecimal value
//! - \%  to print out a \% character
//!
//! For \%s, \%d, \%i, \%u, \%p, \%x, and \%X, an optional number may reside
//! between the % and the format character, which specifies the minimum number
//! of characters to use for that value; if preceded by a 0 then the extra
//! characters will be filled with zeros instead of spaces.  For example,
//! "\%8d" will use eight characters to print the decimal value with spaces
//! added to reach eight; "\%08d" will use eight characters as well but will
//! add zeroes instead of spaces.
//!
//*****************************************************************************
void
Debug_printf(const char *pcString, ...)
{
    va_list vaArgP;

    // Start varargs processing.
    va_start(vaArgP, pcString);

    Debug_vprintf(pcString, vaArgP);

    // Finished with the varargs. Tidy up.
    va_end(vaArgP);
}


#endif // (HW_ENABLE_DEBUG_UART==1) && defined(DEBUG)








/****************************************************************************************
 *
 * If the blinky LED is not enabled (see config.h) the following functions aren't needed.
 *
 ****************************************************************************************/

#if (HW_ENABLE_LED == 1)

/**
 *  Configure the LED GPIO pin as output
 */
void InitLED( void )
{
    SysCtlPeripheralEnable( LED_GPIO_SYSCTL_PERIPH );
    SysCtlPeripheralReset(LED_GPIO_SYSCTL_PERIPH);
    while(!SysCtlPeripheralReady( LED_GPIO_SYSCTL_PERIPH ));

    GPIOPinTypeGPIOOutput( LED_GPIO_BASE, LED_GPIO_PIN );
    GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_PIN, 0 );
}

void LEDon( void )
{
    GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_PIN, LED_GPIO_PIN);
}

void LEDoff( void )
{
    GPIOPinWrite(LED_GPIO_BASE, LED_GPIO_PIN, 0);
}

void SetLED( bool state )
{
    (state) ? LEDon() : LEDoff();
}

#endif  // HW_ENABLE_LED

