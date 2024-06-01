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

#include "MSF60decode.h"
#include "console.h"




// Indexed by the day-of-week number received from the radio
const char *days[] = { "Sun", "Mon", "Tue", "Wed", "Thr", "Fri", "Sat" };

// Millisecond timer tick to measure the radio signal timing. Needed by the decoder.
volatile uint32_t    g_msSysTick = 0;

// CPU clock frequency in MHz. Accessed by the decoder library if the debug UART is enabled for logging.
uint32_t        g_SysClockSpeed;

// DateTime received from the radio will be copied here by the decoder.
static sMSFDateTime    msf_DateTime;





/**
 * Free running millisecond counter needed by the decoder
 */
void SysTickIntHandler(void)
{
    // Update the millisecond interrupt counter.
    g_msSysTick++;
}



/**
 * Setup the millisecond counter
 */
static void InitSystemTick( void )
{
    SysTickPeriodSet(g_SysClockSpeed / 1000);
    SysTickIntRegister( SysTickIntHandler );
    SysTickIntEnable();
    SysTickEnable();
}



/**
 *  Print the received date/time to the console formatted as:  DD-MM-YY HH:MM [DOW]
 */
void PrintDateTime(void)
{
char buffer[32];

    snprintf(buffer, 32, "%02d-%02d-%02d %02d:%02d %s",
                          msf_DateTime.Day, msf_DateTime.Month, msf_DateTime.Year,
                          msf_DateTime.Hour, msf_DateTime.Minute,
                          days[ msf_DateTime.DOW ] );

    Console_puts(buffer);
}



/**
 * NOTE:  Called in an interrupt context!
 *
 * In multi-threaded or multi-core environments you can fire off an event or signal from here to
 * wake up a thread when the time updates or SYNC is lost etc.
 *
 */
static void __attribute__((unused))
EventCallback( eMSFEventType ev )
{
char str[ 32 ];

    snprintf(str, 32, "Event 0x%04X", ev);
    Console_puts(str);
}



void main(void)
{
    char msg[ 64 ];

    uint32_t msSecondTimer = g_msSysTick;
    uint32_t nSeconds = 0;

    // First set the CPU clock to 120 MHz
    g_SysClockSpeed = SysCtlClockFreqSet(
                            (SYSCTL_XTAL_25MHZ | SYSCTL_OSC_MAIN | SYSCTL_USE_PLL | SYSCTL_CFG_VCO_240),
                            120000000 );


    InitSystemTick();                       // Start a 1 ms counter
    Console_InitUART();                     // Initialise the UART connected to the Stellaris virtual COM port on the dev board

    MSF_InitDecoder( &msf_DateTime );       // Initialise the decoder with a ptr to our struct
    MSF_EnableRadio( true );                // Assert the radio enable pin & start decoding the signal

    Console_puts("Looping for date/time updates...");

    IntMasterEnable();

    while (true)
    {

        // Show some status info every second
        if ((g_msSysTick - msSecondTimer) >= 1000)
        {
            snprintf(msg, 64, "%d seconds, SYNC=%d\n", nSeconds++, MSF_GetSyncState());
            Console_puts(msg);
            msSecondTimer = g_msSysTick;
        }


        /*
         * Poll the bDateTimeUpdated flag to check if the time changed.
         * Could instead use event callbacks and get notified that way.
         */


        // Display the received date & time if it's valid & has updated.
        if ((msf_DateTime.bHasValidTime) && (msf_DateTime.bDateTimeUpdated))
        {
            PrintDateTime();
            msf_DateTime.bDateTimeUpdated = false;
        }

        // Read away any unexpected characters received on the console
        while (Console_RxBufferCount()>0)
            Console_getchar();

    }
}


