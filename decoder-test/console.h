/*
 * uart.h
 *
 *  Created on: 29 May 2024
 *      Author: tony
 */

#ifndef _CONSOLE_H_
#define _CONSOLE_H_

#include <stdint.h>
#include <stdbool.h>


#define     CONSOLE_RX_BUFFER_SIZE      1024
#define     CONSOLE_TX_BUFFER_SIZE      1024



/**
 * Hardware Definitions for the Stellaris virtual JTAG UART
 * on the EK-TM4C1294XL Launchpad Eval board. Used for stdio.
 */
#define     CONSOLE_UART_SYS_PERIPH       SYSCTL_PERIPH_UART0
#define     CONSOLE_UART_GPIO_PERIPH      SYSCTL_PERIPH_GPIOA
#define     CONSOLE_UART_GPIO_BASE        GPIO_PORTA_BASE
#define     CONSOLE_UART_INT              INT_UART0
#define     CONSOLE_UART_BASE             UART0_BASE
#define     CONSOLE_UART_BAUD             115200
#define     CONSOLE_UART_RX_PIN_CONFIG    GPIO_PA0_U0RX
#define     CONSOLE_UART_TX_PIN_CONFIG    GPIO_PA1_U0TX
#define     CONSOLE_UART_RX_PIN           GPIO_PIN_0
#define     CONSOLE_UART_TX_PIN           GPIO_PIN_1




void Console_InitUART( void );
void Console_FlushTxBuffer( void );
void Console_FlushRxBuffer(void);
uint32_t Console_TxBufferCount( void );
uint32_t Console_RxBufferCount( void );
uint32_t Console_write(const char *pBuffer, uint32_t len);
bool Console_putchar( uint8_t achar );
uint8_t Console_getchar( void );
void Console_puts( const char *pstr );


#endif /* _CONSOLE_H_ */
