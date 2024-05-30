
/********************************************************************************
 * @file    MSF60decode.c
 * @author  Tony Hanratty
 * @date    24-May-2024
 *
 * @brief   Decodes a MSF60 date/time bit stream
 *
 * @notes   This follows the National Physics Laboratory MSL specification at:
 *          http://www.pvelectronics.co.uk/rftime/msf/MSF_Time_Date_Code.pdf
 *
 ********************************************************************************/


/************************************************************************************************************
 * INCLUDES
 ************************************************************************************************************/

// System includes
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <inttypes.h>

// TI platform includes
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "driverlib/sysctl.h"
#include "driverlib/interrupt.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/uart.h"

// Our includes
#include "config.h"
#include "logging.h"
#include "hardware.h"

// This file's include
#include "MSF60decode.h"



/************************************************************************************************************
 *      PRIVATE MACROS AND DEFINES
 ************************************************************************************************************/

// Allow +/- 30ms on signal timings
#define     PULSE_MARGIN            30

// My cheapo AliExpress MSF60 receiver board inverts the sense of the carrier signal
#define     CARRIER_ON              0
#define     CARRIER_OFF             1



/************************************************************************************************************
 *      PRIVATE STRUCTS AND TYPEDEFS
 ************************************************************************************************************/

// These are all possible lengths of carrier ON->OFF or OFF->ON timing in milliseconds
typedef enum {
    eWidth_100 = 1,
    eWidth_200 = 2,
    eWidth_300 = 3,
    eWidth_500 = 5,
    eWidth_700 = 7,
    eWidth_800 = 8,
    eWidth_900 = 9,
    //
    eWidth_INVALID = 0
} eWidth;



/************************************************************************************************************
 *      EXTERN VARIABLES
 ************************************************************************************************************/

// We need access to a millisecond resolution uin32_t tick counter for timing.
extern volatile uint32_t g_msSysTick;



/************************************************************************************************************
 *      LOCAL STATIC VARIABLES
 ************************************************************************************************************/

// Radio signal SYNC has been detected & locked
STATIC bool bSyncedFlag = false;


// Each array needs to hold at least 59 bits. b0 is not used, numbering starts @ b1 to match the spec
STATIC uint8_t A_bits[8];
STATIC uint8_t B_bits[8];


// Local variable to save a valid date/time decode result.
STATIC sMSFDateTime  LocalDateTime = { 0 };         // Local decoded date/time

// Client supplied pointer to a date/time buffer
STATIC sMSFDateTime* pClientDateTime = NULL;

// Client supplied pointer to event notification handler function
STATIC MSF_EVENT_CALLBACK pfClientEventCallback = NULL;

// Client supplied mask of eMSFEventType values selectively enable event notifications
STATIC uint32_t ui32ClientEventMask = 0;



/************************************************************************************************************
 *      LOCAL STATIC FUNCTION PROTOTYPES
 ************************************************************************************************************/

STATIC void InitRadioInterface( void );



/************************************************************************************************************
 *      PUBLIC GLOBAL FUNCTIONS (API)
 ************************************************************************************************************/


/*******************************************************************
* NAME
*       MSF_EnableRadio()
*
* DESCRIPTION
*       Enable/Disable the MSF60 radio bit stream
*
* PARAMETERS
*       bool    state           true  : Enable radio output
*                               false : Disable radio output
* OUTPUTS
*       None
*
* RETURNS
*       Nothing
*
* NOTES
*
* Toggles a GPIO pin to control the radio. For Different hardware, edit
* this function to do whatever's necessary to enable the MSF bit stream.
* NB This pin is active low for my hardware.
*
********************************************************************/
void
MSF_EnableRadio( bool state )
{
    GPIOPinWrite(RADIO_PORT_BASE, RADIO_ENABLE_BIT, (state) ? 0 : RADIO_ENABLE_BIT );
}



/*******************************************************************
* NAME
*       MSF_InitDecoder()
*
* DESCRIPTION
*       Initialises the MSF bit stream decoder
*
* PARAMETERS
*       sMSFDateTime*       pdata       Buffer to receive the decoded date/time
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
MSF_InitDecoder( sMSFDateTime* pdata )
{
    // A debug UART or LED can be optionally enabled in config.h <== really?
    Debug_InitUART();                   // Init one of the CPU UARTs if enabled
    InitLED();                          // Optionally blink a LED when carrier changes

    InitRadioInterface();

    // If the client supplied a valid data ptr, save it & initialise the struct
    if (pdata)
    {
        pClientDateTime = pdata;
        memset(pdata, 0, sizeof(sMSFDateTime));
    }
}



/*******************************************************************
* NAME
*       MSF_EnableEventNotifications()
*
* DESCRIPTION
*       Configure event notifications back to the client
*
* PARAMETERS
*       MSF_EVENT_CALLBACK  pfunc       Client callback function for event notifications.
*
*       uint32_t            mask        Bitwise mask of event types to enable.
*
* OUTPUTS
*       None
*
* RETURNS
*       Nothing
*
* NOTES
*
* If pfunc is NULL or mask = 0, event callbacks are disabled.
*
********************************************************************/
void
MSF_EnableEventNotifications( MSF_EVENT_CALLBACK pCallbackFunc, uint32_t enable_mask )
{
    pfClientEventCallback = pCallbackFunc;
    ui32ClientEventMask   = enable_mask;
}



/*******************************************************************
* NAME
*       MSF_GetSyncState()
*
* DESCRIPTION
*       Determine if the decoder is currently SYNC'd to a valid MSF60 bit stream.
*
* PARAMETERS
*       None
*
* OUTPUTS
*       None
*
* RETURNS
*       bool            true    Decoder is SYNC'd to a valid date/time bit stream
*                       false   Decoder is not currently SYNC'd
*
* NOTES
*
********************************************************************/
bool
MSF_GetSyncState( void )
{
    return bSyncedFlag;
}






/************************************************************************************************************
 *      PRIVATE LOCAL FUNCTIONS
 ************************************************************************************************************/

/**
 * If a callback function is registered, and this particular event type is
 * unmasked, notify the client.
 */
STATIC void
ClientEventNotify( eMSFEventType ev )
{
    if ((pfClientEventCallback) && (ui32ClientEventMask & ev))
    {
        pfClientEventCallback( ev );
    }
}



/**
 * Set or clear a bit in the A or B array
 */
STATIC void
setBit(uint8_t* bitarray, unsigned bitnum, bool bSet)
{
  uint8_t mask = 1 << (bitnum & 0x7);

  if (bSet)
      bitarray[ bitnum>>3 ] |= mask;
  else
      bitarray[ bitnum>>3 ] &= ~mask;
}



/**
 * Read a bit from the A or B arrays
 */
STATIC bool
getBit(uint8_t* bitarray, unsigned bitnum)
{
  uint8_t mask = 1 << (bitnum & 0x7);

  return bitarray[ bitnum>>3 ] & mask;
}



/**
 * Extracts & decodes a BCD bitfield from the A or B data stream
 */
STATIC uint8_t
ExtractBCD( uint8_t *bitarray, uint8_t msbit, uint8_t lsbit  )
{
    static uint8_t BCD[] = { 1, 2, 4, 8, 10, 20, 40, 80 };

    uint8_t result = 0;
    uint8_t digit  = 0;
    uint8_t bit;

    for( bit = lsbit; bit >= msbit; bit--, digit++ )
        if( getBit( bitarray, bit ))
            result += BCD[ digit ];

    return result;
}



/**
 * Check the specified bitfield along with the 'parity' parameter have ODD parity.
 */
STATIC bool
CheckOddParity(uint8_t *bitarray, int from, int to, bool parity)
{
unsigned numset = 0;
unsigned bitnum;

    for (bitnum = from ; bitnum <= to ; bitnum++)
        if (getBit(bitarray, bitnum))
            numset++;

    if (parity)
        numset++;

    // Must be an odd number of set bits
    return (numset & 1) ? true : false;
}



/**
 * Validate the received bit stream as per the NPL specification
 *
 */
STATIC bool
ValidateBCD()
{
unsigned bitnum;

    // A52 must be 0
    if (getBit( A_bits, 52)) {
        LOGprintf(LOG_BCD_ERROR, "A52 is not zero!\n" );
        return false;
    }

    // A53 through A58 must be 1
    for(bitnum=53 ; bitnum<=58 ; bitnum++) {
        if (!getBit( A_bits, bitnum)) {
            LOGprintf(LOG_BCD_ERROR, "A%d is not set!\n", bitnum);
            return false;
        }
    }

    // A59 must be 0
    if (getBit( A_bits, 59)) {
        LOGprintf(LOG_BCD_ERROR, "A59 is not zero!\n");
        return false;
    }

    // A17 through A24 along with B54 must have odd parity
    if (!CheckOddParity( A_bits, 17, 24, getBit(B_bits, 54))) {
        LOGprintf(LOG_BCD_ERROR, "A17 to A24 fail parity check with B54!\n");
        return false;
    }

    // A25 through A35 along with B55 must have odd parity
    if (!CheckOddParity( A_bits, 25, 35, getBit(B_bits, 55)))  {
        LOGprintf(LOG_BCD_ERROR, "A25 to A35 fail parity check with B55!\n");
        return false;
    }

    // A36 through A38 along with B56 must have odd parity
    if (!CheckOddParity( A_bits, 36, 38, getBit(B_bits, 56)))  {
        LOGprintf(LOG_BCD_ERROR, "A36 to A38 fail parity check with B55!\n");
        return false;
    }

    // A39 through A51 along with B57 must have odd parity
    if (!CheckOddParity( A_bits, 39, 51, getBit(B_bits, 57))) {
        LOGprintf(LOG_BCD_ERROR, "A39 to A51 fail parity check with B57!\n");
        return false;
    }

    return true;
}



/**
 * Once we have received a full frame of 59 bits try to decode it.
 * If it's valid & the client supplied a buffer, copy in the date/time.
 *
 */
STATIC bool
DecodeFrame()
{

    bool bFrameValid = ValidateBCD();
    if (bFrameValid == true)
    {
        // Dump A and B bit buffers to the debug UART
        LOGprintf(LOG_BIT_DUMP, "");

        LocalDateTime.Year    = ExtractBCD( A_bits, 17, 24 );    // 0-99
        LocalDateTime.Month   = ExtractBCD( A_bits, 25, 29 );    // 1-12
        LocalDateTime.Day     = ExtractBCD( A_bits, 30, 35 );    // 1-31
        LocalDateTime.DOW     = ExtractBCD( A_bits, 36, 38 );    // 0-6
        LocalDateTime.Hour    = ExtractBCD( A_bits, 39, 44 );    // 0-23
        LocalDateTime.Minute  = ExtractBCD( A_bits, 45, 51 );    // 0-59
        LocalDateTime.DST     = ExtractBCD( B_bits, 58, 58 );    // 0 or 1

        LocalDateTime.bHasValidTime    = true;
        LocalDateTime.bDateTimeUpdated = true;

        // Copy to the client's data struct if it's valid
        if ((pClientDateTime) && (LocalDateTime.bHasValidTime))
        {
            *pClientDateTime = LocalDateTime;
        }

        // Notify the client
        ClientEventNotify( MSF_EVENT_DATETIME_UPDATED );
    }

    return bFrameValid;
}



/**
 * Check for all possible CARRIER_ON & CARRIER_OFF pulse width we may encounter in a valid signal.
 *
 */
STATIC eWidth
GetWidth( uint32_t width )
{
    if (abs(width-100) < PULSE_MARGIN)
        return eWidth_100;
    else if (abs(width-200) < PULSE_MARGIN)
        return eWidth_200;
    else if (abs(width-300) < PULSE_MARGIN)
        return eWidth_300;
    else if (abs(width-500) < PULSE_MARGIN)
        return eWidth_500;
    else if (abs(width-700) < PULSE_MARGIN)
        return eWidth_700;
    else if (abs(width-800) < PULSE_MARGIN)
        return eWidth_800;
    else if (abs(width-900) < PULSE_MARGIN)
        return eWidth_900;
    else
        return eWidth_INVALID;
}



/*******************************************************************
* NAME  HandleCarrierEvent
*
* DESCRIPTION
*       Processes changes in the carrier signal from the radio
*
* PARAMETERS
*       uint32_t    event_level     CARRIER_ON or CARRIER_OFF GPIO pin level
*
* OUTPUTS
*       If a valid time is decoded it is copied to the users pUserDateTime
*
* RETURNS
*       Nothing
*
* NOTES
*
* 1. Wait for a valid SYNC condition, 500ms CARRIER_OFF then 500 ms CARRIER_ON then CARRIER_OFF
* 2. Determine if the event_level & timing is valid for the current state
* 3. Extract the A and B bits from each second/cell and save them.
* 4. After 59 error-free seconds/cells, validate the received A & B bit stream.
* 5. If the frame is valid, decode & save the date/time information.
* 6. If the client registered a buffer, copy the date/time into it & set the update flag.
* 7. If the client registered an event callback, notify the date/time has updated.
*
* If at any point an unexpected or impossible state or timing is detected, ditch the entire
* frame and restart to SYNC the bit stream again.
*
*/
STATIC void
HandleCarrierEvent(uint32_t event_level)
{
static uint32_t T_LastOnStart  = 0;
static uint32_t T_LastOffStart = 0;
static eWidth eLastOnWidth  = eWidth_INVALID;
static eWidth eLastOffWidth = eWidth_INVALID;
static uint32_t T_CellStart = 0;

static unsigned nBitNum = 1;            // Current bit number (ie second) being decoded 1 - 59
static bool bHalfSync = false;          // 500ms CARRIER_OFF detected
static bool bResyncNeeded = true;

    uint32_t event_time = g_msSysTick;

    switch (event_level)
    {
        case CARRIER_OFF:
        {
            // Update state tracking variables
            T_LastOffStart = event_time;
            eLastOnWidth   = GetWidth(event_time - T_LastOnStart);

            // Log carrier is now OFF and the last ON duration
            LOGprintf(LOG_CARRIER_EVENT, "OFF %u\n", event_time - T_LastOnStart);

            // If we're not SYNCd, every CARRIER_OFF is potentially the start of a new second/cell
            if (bSyncedFlag == false)
                T_CellStart = T_LastOffStart;

            switch( eLastOnWidth )
            {
                case eWidth_500:
                    if (bHalfSync == true)
                    {
                        // We've had 500 ON after 500 OFF. This a good SYNC so we're at the start of the second #1 cell.
                        LOGprintf(LOG_SYNC_MSG, "SYNC\n");
                        ClientEventNotify( MSF_EVENT_SYNC );
                        bSyncedFlag = true;
                        T_CellStart = T_LastOffStart;
                        // This is definitely the start of a new frame. If we just received a full frame, try to decode it.
                        if (nBitNum==60)
                            bResyncNeeded = !DecodeFrame();
                        nBitNum = 1;
                    }
                    else
                    {
                        // 500ms CARRIER_ON without a preceding 500ms CARRIER_OFF shouldn't happen.
                        LOGprintf(LOG_SYNC_MSG, "Missing HALF SYNC\n");
                        bResyncNeeded = true;
                    }
                    break;

                // A & B both high followed by 700ms high
                case eWidth_900:
                    if (!bSyncedFlag) break;
                    setBit(A_bits, nBitNum, false);
                    setBit(B_bits, nBitNum++, false);
                    T_CellStart = T_LastOffStart;
                    break;

                // A low, B high followed by 700ms high
                case eWidth_800:
                    if (!bSyncedFlag) break;
                    setBit(A_bits, nBitNum, true);
                    setBit(B_bits, nBitNum++, false);
                    T_CellStart = T_LastOffStart;
                    break;

                // A & B both low followed by 700ms high
                case eWidth_700:
                    if (!bSyncedFlag) break;
                    setBit(B_bits, nBitNum++, true);
                    T_CellStart = T_LastOffStart;
                    break;

                // @ the end of A high, start of B low ?
                case eWidth_100:
                    if (!bSyncedFlag) break;
                    if (GetWidth(event_time - T_CellStart)==eWidth_200)
                    {
                        setBit(A_bits, nBitNum, false);
                        setBit(B_bits, nBitNum++, true);
                    }
                    else
                        bResyncNeeded = true;
                    break;

                default:
                    LOGprintf(LOG_EDGE_ERROR, "Bad CARRIER_ON width %d\n", event_time - T_LastOnStart);
                    bResyncNeeded = true;
                    break;

            } // switch(eLastOnWidth)

        } // case CARRIER_OFF
        break;


        case CARRIER_ON:
        {
            // Update state tracking variables
            T_LastOnStart = event_time;
            eLastOffWidth = GetWidth(event_time - T_LastOffStart);

            // Show carrier is now ON and the last OFF duration
            LOGprintf(LOG_CARRIER_EVENT, "ON %d\n", event_time - T_LastOffStart);

            // Where in the cell/second is this CARRIER_ON edge?
            switch (GetWidth(event_time - T_CellStart))
            {
                // Check for SYNC condition
                case eWidth_500:
                    if (eLastOffWidth == eWidth_500)
                    {
                        // This event is a CARRIER_ON 500ms from cell start immediately after a 500ms OFF.
                        // If the carrier stays ON for 500ms this will be a valid SYNC.
                        bHalfSync = true;
                    }
                    else
                    {
                        // We just had CARRIER_ON 500ms from cell start, but the preceding OFF wasn't 500ms.
                        // This is invalid & should never happen.
                        LOGprintf(LOG_SYNC_MSG, "Unexpected HALF SYNC\n");
                        bResyncNeeded = true;
                    }
                    break;

                case eWidth_100:
                    if (!bSyncedFlag) break;
                    setBit(A_bits, nBitNum, false);
                    break;

                case eWidth_200:
                    if (!bSyncedFlag) break;
                    setBit(A_bits, nBitNum, true);
                    setBit(B_bits, nBitNum, false);
                    break;

                case eWidth_300:
                    if (!bSyncedFlag) break;
                    setBit(B_bits, nBitNum, false);
                    if (eLastOffWidth == eWidth_100)
                        setBit(A_bits, nBitNum, false);
                    else if (eLastOffWidth == eWidth_300)
                        setBit(A_bits, nBitNum, true);
                    else
                        bResyncNeeded = true;
                    break;

                default:
                    LOGprintf(LOG_EDGE_ERROR, "Bad CARRIER_ON offset %d\n", event_time - T_CellStart);
                    bResyncNeeded = true;
                    break;
            } // switch
        } // case CARRIER_ON
        break;

        default:
            LOGprintf(LOG_EDGE_ERROR, "Unknown carrier event!\n");
            bResyncNeeded = true;
            break;

    } // switch event->level



    if (bResyncNeeded)
    {
        // If there were any errors we must reSYNC
        LOGprintf(LOG_SYNC_MSG, "SYNC lost\n");
        ClientEventNotify( MSF_EVENT_SYNC_LOST );

        nBitNum = 1;
        bHalfSync = false;
        bSyncedFlag = false;
        bResyncNeeded = false;
    }

}





/**
 * This MSF radio ISR just passes the new carrier signal level to
 * the HandleCarrierEvent() function.
 */
STATIC void
RadioGpioIntHandler( void )
{
uint32_t carrier_level;

    uint32_t int_status = GPIOIntStatus( RADIO_PORT_BASE, true );
    GPIOIntClear( RADIO_PORT_BASE, RADIO_DATA_BIT );

    if (int_status & RADIO_DATA_BIT)
    {
        carrier_level = (GPIOPinRead(RADIO_PORT_BASE, RADIO_DATA_BIT)) ? CARRIER_OFF : CARRIER_ON;
        HandleCarrierEvent( carrier_level );

        SetLED( carrier_level );
    }
}



/**
 * Configure the GPIO port to interface with the MSF radio card
 */
STATIC void
InitRadioInterface( void )
{
    // Enable the GPIO port
    SysCtlPeripheralEnable(RADIO_GPIO_SYSCTL_PERIPH);
    SysCtlPeripheralReset(RADIO_GPIO_SYSCTL_PERIPH);
    while(!SysCtlPeripheralReady( RADIO_GPIO_SYSCTL_PERIPH ));

    // Configure the required pins
    GPIOPinTypeGPIOOutput( RADIO_PORT_BASE, RADIO_ENABLE_BIT );
    GPIODirModeSet( RADIO_PORT_BASE, RADIO_ENABLE_BIT, GPIO_DIR_MODE_OUT );
    GPIOPadConfigSet(RADIO_PORT_BASE, RADIO_ENABLE_BIT, GPIO_STRENGTH_2MA, GPIO_PIN_TYPE_STD);
    GPIOPinTypeGPIOInput( RADIO_PORT_BASE, RADIO_DATA_BIT);
    GPIODirModeSet( RADIO_PORT_BASE, RADIO_DATA_BIT, GPIO_DIR_MODE_IN );

    // This pin must be low to enable the output bit stream. Leave it high (disabled) for now.
    GPIOPinWrite(RADIO_PORT_BASE, RADIO_ENABLE_BIT, RADIO_ENABLE_BIT );

    // Configure GPIO interrupt for both rising & falling edges on the input pin
    GPIOIntRegister( RADIO_PORT_BASE, RadioGpioIntHandler );
    GPIOIntTypeSet( RADIO_PORT_BASE, RADIO_DATA_BIT, GPIO_BOTH_EDGES );
    GPIOIntEnable( RADIO_PORT_BASE, RADIO_DATA_BIT );

    IntEnable( RADIO_INT_GPIO );
}








