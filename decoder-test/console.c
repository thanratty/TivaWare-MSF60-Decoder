/*
 * uart.c
 *
 *  Created on: 29 May 2024
 *      Author: tony
 */


#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/sysctl.h"
#include "driverlib/systick.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"

#include "console.h"






//*****************************************************************************
// Transmit Buffer Macros
//*****************************************************************************

#define TX_BUFFER_EMPTY()                   (Console_TxBufferCount()==0)
#define TX_BUFFER_FULL()                    (Console_TxBufferCount()==CONSOLE_TX_BUFFER_SIZE)

#define ADVANCE_TX_BUFFER_INDEX(Index)      do {(Index) = ((Index) + 1) % CONSOLE_TX_BUFFER_SIZE;} while(0)



//*****************************************************************************
// Receive Buffer Macros
//*****************************************************************************

#define RX_BUFFER_EMPTY()                   (Console_RxBufferCount()==0)
#define RX_BUFFER_FULL()                    (Console_RxBufferCount()==CONSOLE_RX_BUFFER_SIZE)

#define ADVANCE_RX_BUFFER_INDEX(Index)      do {(Index) = ((Index) + 1) % CONSOLE_RX_BUFFER_SIZE;} while(0)




// CPU clock frequency in MHz
extern uint32_t g_SysClockSpeed;





/**
 * Output buffer. Buffer is full if TxReadIndex is one ahead of
 * TxWriteIndex. Buffer is empty if the two indices are the same.
 */
static uint8_t TxBuffer[ CONSOLE_TX_BUFFER_SIZE ];
static volatile uint32_t TxWriteIndex = 0;
static volatile uint32_t TxReadIndex = 0;

/**
 * Input buffer. Buffer is full if TxReadIndex is one ahead of
 * TxWriteIndex. Buffer is empty if the two indices are the same.
 */
static uint8_t RxBuffer[ CONSOLE_RX_BUFFER_SIZE ];
static volatile uint32_t RxWriteIndex = 0;
static volatile uint32_t RxReadIndex = 0;











void
Console_FlushTxBuffer( void )
{
    // The remaining data should be discarded, so temporarily turn off interrupts.
    bool MasterintStatus = IntMasterDisable();

    // Flush the buffer indices
    TxReadIndex = TxWriteIndex = 0;

    // If interrupts were enabled when we turned them off, turn them back on again.
    if(!MasterintStatus)
        IntMasterEnable();
}



void
Console_FlushRxBuffer(void)
{
    // Temporarily turn off interrupts.
    bool MasterintStatus = IntMasterDisable();

    // Flush the receive buffer.
    RxReadIndex = RxWriteIndex = 0;

    // If any interrupts were enabled when we turned them off, turn them back on again.
    if(!MasterintStatus)
        IntMasterEnable();
}



uint32_t
Console_TxBufferCount( void )
{
    uint32_t ui32Write = TxWriteIndex;
    uint32_t ui32Read  = TxReadIndex;

    return (ui32Write >= ui32Read)
           ? (ui32Write - ui32Read)
           : (CONSOLE_TX_BUFFER_SIZE - (ui32Read - ui32Write));
}



uint32_t
Console_RxBufferCount( void )
{
    uint32_t ui32Write = RxWriteIndex;
    uint32_t ui32Read  = RxReadIndex;

    return (ui32Write >= ui32Read)
           ? (ui32Write - ui32Read)
           : (CONSOLE_RX_BUFFER_SIZE - (ui32Read - ui32Write));
}






//*****************************************************************************
//
// Take as many bytes from the transmit buffer as we have space for and move
// them into the UART transmit FIFO.
//
// Returns # bytes written to the TX FIFO
//
//*****************************************************************************

static void
PrimeTheTransmitFIFO( void )
{
    // Do we have any data to transmit?
    if(!TX_BUFFER_EMPTY())
    {
        // Disable the UART interrupt while we're playing with the buffer indexes
        IntDisable(CONSOLE_UART_INT);

        // Yes. Take some characters out of the transmit buffer and feed them to the UART transmit FIFO.
        while(UARTSpaceAvail(CONSOLE_UART_BASE) && !TX_BUFFER_EMPTY())
        {
            UARTCharPutNonBlocking(CONSOLE_UART_BASE, TxBuffer[ TxReadIndex ]);
            ADVANCE_TX_BUFFER_INDEX( TxReadIndex );
        }

        IntEnable(CONSOLE_UART_INT);
    }
}



/*****************************************************************************\
 *
 * UART TX and RX interrupt handler
 *
\*****************************************************************************/
static void
ConsoleUARTIntHandler(void)
{
    uint32_t int_status;
    uint8_t  aChar;

    // Get and clear the current interrupt source(s)
    int_status = UARTIntStatus(CONSOLE_UART_BASE, true);
    UARTIntClear(CONSOLE_UART_BASE, int_status);


    // TX FIFO has space available
    if(int_status & UART_INT_TX)
    {
        // If the output buffer is now empty, turn off the transmit interrupt.
        if(TX_BUFFER_EMPTY())
        {
            UARTIntDisable(CONSOLE_UART_BASE, UART_INT_TX);
        }
        else
        {
            // Write as many bytes as we can into the transmit FIFO.
            PrimeTheTransmitFIFO();
            UARTIntEnable(CONSOLE_UART_BASE, UART_INT_TX);
        }
    }


    // RX interrupt?
    if(int_status & (UART_INT_RX | UART_INT_RT))
    {
        // Get all the available characters from the UART.
        while(UARTCharsAvail(CONSOLE_UART_BASE))
        {
            // Read a character
            aChar = UARTCharGetNonBlocking(CONSOLE_UART_BASE);

            // If there is space in the receive buffer, save the character otherwise throw it away.
            if(!RX_BUFFER_FULL())
            {
                // Store the new character in the receive buffer
                RxBuffer[ RxWriteIndex ] = aChar;
                ADVANCE_RX_BUFFER_INDEX( RxWriteIndex );
            }
        }
    }
}





/**
 * Configure the integrated JTAG/USB UART
 */
void Console_InitUART( void )
{
    SysCtlPeripheralEnable(CONSOLE_UART_SYS_PERIPH);
    while(!SysCtlPeripheralReady( CONSOLE_UART_SYS_PERIPH ));

    SysCtlPeripheralEnable(CONSOLE_UART_GPIO_PERIPH);
    while(!SysCtlPeripheralReady( CONSOLE_UART_GPIO_PERIPH ));

    GPIOPinConfigure(CONSOLE_UART_RX_PIN_CONFIG);
    GPIOPinConfigure(CONSOLE_UART_TX_PIN_CONFIG);

    GPIOPinTypeUART(CONSOLE_UART_GPIO_BASE, CONSOLE_UART_RX_PIN | CONSOLE_UART_TX_PIN);

    UARTEnable(CONSOLE_UART_BASE);

    Console_FlushTxBuffer();
    Console_FlushRxBuffer();

    // Configure the UART, typically 115200-8-N-1
    UARTConfigSetExpClk( CONSOLE_UART_BASE,
                         g_SysClockSpeed,
                         CONSOLE_UART_BAUD,
                         (UART_CONFIG_WLEN_8 | UART_CONFIG_PAR_NONE | UART_CONFIG_STOP_ONE) );


    // Don't enable the TX interrupt in the UART till data has been written to the TX FIFO
    UARTIntDisable(CONSOLE_UART_BASE, 0xFFFFFFFF);
    IntRegister(CONSOLE_UART_INT, ConsoleUARTIntHandler);
    UARTIntEnable(CONSOLE_UART_BASE, UART_INT_RX | UART_INT_RT);
    IntEnable(CONSOLE_UART_INT);

}









//*****************************************************************************
//
//! Writes a string of characters to the console UART
//!
//*****************************************************************************
uint32_t
Console_write(const char *pBuffer, uint32_t len)
{
uint32_t nWriteCount = 0;
uint32_t index;

    for(index=0 ; index < len ; index++)
    {
        // Send the character to the UART output.
        if(!TX_BUFFER_FULL())
        {
            TxBuffer[ TxWriteIndex ] = pBuffer[ index ];
            ADVANCE_TX_BUFFER_INDEX( TxWriteIndex );
            nWriteCount++;
        }
        else
        {
            // Buffer is full - discard remaining characters and return.
            break;
        }
    }


    // If we have anything in the buffer, make sure that the UART is setup to transmit it.
    if(!TX_BUFFER_EMPTY())
    {
        PrimeTheTransmitFIFO();
        UARTIntEnable(CONSOLE_UART_BASE, UART_INT_TX);
    }

    // Return the number of characters written.
    return nWriteCount;
}





bool
Console_putchar( uint8_t achar )
{
    if (TX_BUFFER_FULL())
        return false;

    TxBuffer[ TxWriteIndex ] = achar;
    ADVANCE_TX_BUFFER_INDEX( TxWriteIndex );

    return true;
}



uint8_t
Console_getchar( void )
{
uint8_t achar;

    while(RX_BUFFER_EMPTY());

    achar = RxBuffer[ RxReadIndex ];
    ADVANCE_RX_BUFFER_INDEX( RxReadIndex );

    return achar;
}


// Output everything up to a terminating NULL. Followed by CR/LF
void
Console_puts( char const *pstr )
{
    while(*pstr)
        Console_putchar(*pstr++);

    Console_write("\n\r", 1);
}

