
/********************************************************************************
 * @file    config.h
 * @author  Tony Hanratty
 * @date    18-May-2024
 *
 * @brief   Configure compile time hardware & software settings.
 *
 * @note    Include this file first after system and platform includes.
 *
 ********************************************************************************/

#ifndef _CONFIG_H_
#define _CONFIG_H_



/**
 * It's handy to have all static functions & variables visible in the map file in a Debug build.
 */
#if defined(DEBUG)
#define     STATIC
#else
#define     STATIC                  static
#endif



/**
 * Hardware configuration options.
 */
#define     HW_ENABLE_LED                   1                   // 0 to disable LED flash when MSF carrier signal toggles
#define     HW_ENABLE_DEBUG_UART            1                   // 0 to disable logging



/**
 * Define the I/O Interface to the MSF radio.
 */
#define     RADIO_GPIO_SYSCTL_PERIPH        SYSCTL_PERIPH_GPIOB
#define     RADIO_PORT_BASE                 GPIO_PORTB_BASE
#define     RADIO_ENABLE_BIT                GPIO_PIN_2
#define     RADIO_DATA_BIT                  GPIO_PIN_3
#define     RADIO_INT_GPIO                  INT_GPIOB



/**
 * (optional) Use UART 6 for Debug/Logging. You can configure a different one if you want.
 */
#define     DEBUG_UART_BASE                 UART6_BASE
#define     DEBUG_UART_INT                  INT_UART6
#define     DEBUG_UART_SYS_PERIPH           SYSCTL_PERIPH_UART6
#define     DEBUG_UART_GPIO_PERIPH          SYSCTL_PERIPH_GPIOP
#define     DEBUG_UART_GPIO_BASE            GPIO_PORTP_BASE
#define     DEBUG_UART_BAUD                 115200
#define     DEBUG_UART_RX_PIN_CONFIG        GPIO_PP0_U6RX
#define     DEBUG_UART_TX_PIN_CONFIG        GPIO_PP1_U6TX
#define     DEBUG_UART_RX_PIN               GPIO_PIN_0
#define     DEBUG_UART_TX_PIN               GPIO_PIN_1



/**
 * The (optional) The LED is on GPIO port N bit 0 on my eval board
 */
#define     LED_GPIO_SYSCTL_PERIPH          SYSCTL_PERIPH_GPION
#define     LED_GPIO_BASE                   GPIO_PORTN_BASE
#define     LED_GPIO_PIN                    GPIO_PIN_0



#endif // _CONFIG_H_

