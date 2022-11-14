/*
 * kypd_leds.c
 *
 *  Created on: 	23 March 2021
 *      Author: 	Alberto Sanchez
 *     Version:		1.0
 */

/**************************************************************
*
*    SECTION: VERSION HISTORY
*
***************************************************************
*
*	v1.0 - 23 March 2021
*		First version created, modified from Zynq Book tutorial
*
***************************************************************/

/**************************************************************
*
*    SECTION: DESCRIPTION
*
***************************************************************
*
* This file contains an example of using the GPIO driver to
* decode a keypad in JA Pmod in the Zybo Z7-20 Board.
* The system connects to the keypad by using AXI GPIO 0, rows ch1
* cols ch 2. The AXI GPIO 1 is connected to the LEDs (CH1).
*
* The provided code scans the keypad and shows the binary value
* in the leds
*
**************************************************************/

/**************************************************************
*
*    SECTION: LIBRARIES
*
***************************************************************/
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"
#include "xil_printf.h"

/**************************************************************
*
*    SECTION: DEFINITIONS
*
***************************************************************/
#define KEYPAD_GPIO_DEVICE_ID  XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device connected to keypad */
#define LEDS_GPIO_DEVICE_ID  XPAR_AXI_GPIO_1_DEVICE_ID /* GPIO device connected to leds*/
#define LED_CHANNEL 1					/* GPIO port 1 for LEDs */
#define KEYPAD_ROWS_CH 1				/* GPIO channel for rows */
#define KEYPAD_COLS_CH 2				/* GPIO channel for cols */
#define printf xil_printf				/* smaller, optimised printf */

/**************************************************************
*
*    SECTION: DEVICE INSTANCES
*
***************************************************************/
XGpio Kypd_Gpio;		/* GPIO Device driver instance for keypad */
XGpio Leds_Gpio;		/* GPIO Device driver instance for leds */

/**************************************************************
*
*     SECTION: FUNCTION PROTOTYPES
*
***************************************************************/

int KEYPDLEDOutputExample(void);
void Delay(void);


/* Main function. */
int main(void){
	int Status;

	/* Execute the LED output. */
	Status = KEYPDLEDOutputExample();
	if (Status != XST_SUCCESS) {
		xil_printf("GPIO output to the LEDs failed!\r\n");
	}

	return 0;
}

/**************************************************************
*
* SECTION: PROTOTYPE FUNCTION IMPLEMENTATIONS
*
**************************************************************/

int KEYPDLEDOutputExample(void){
	int Status;
	int led; 	/* Create variable to pass on to AXI to lightup LEDs */
	int cols = 0xe; 	/* Create variable to sweep columns */
	int rows = 0x0;   /* Create a variable to scan rows */
	int cols_msb = 0x0;

		/* KEYPAD GPIO driver initialization */
		Status = XGpio_Initialize(&Kypd_Gpio, KEYPAD_GPIO_DEVICE_ID);
		if (Status != XST_SUCCESS){
			return XST_FAILURE;
		}

		/*LEDS GPIO driver initialization */
		Status = XGpio_Initialize(&Leds_Gpio, LEDS_GPIO_DEVICE_ID);
		if (Status != XST_SUCCESS){
			return XST_FAILURE;
		}

		/*Set the direction for the keypad rows to inputs. */
		XGpio_SetDataDirection(&Kypd_Gpio, KEYPAD_ROWS_CH, 0xf);

		/*Set the direction for the keypad rows to outputs. */
		XGpio_SetDataDirection(&Kypd_Gpio, KEYPAD_COLS_CH, 0x0);

		/*Set the direction for the leds to outputs. */
		XGpio_SetDataDirection(&Leds_Gpio, LED_CHANNEL, 0x0);

		/* Loop forever */
			while (1) {
				/* Write output to the Columns */
				XGpio_DiscreteWrite(&Kypd_Gpio, KEYPAD_COLS_CH, cols);
				
				Delay();
				
				// Read the rows
				rows = XGpio_DiscreteRead(&Kypd_Gpio, KEYPAD_ROWS_CH);


				switch((cols & 0x0000000f)){
					case 0xe:
						switch(rows){
						case 0xf:
							led = 0x0;
							break;
						case 0xe:
							led = 0x1;
							break;
						case 0xd:
							led = 0x4;
							break;
						case 0xb:
							led = 0x7;
							break;
						case 0x7:
							led = 0xe;
							break;
						}
						break;
						case 0xd:
							switch(rows){
							case 0xf:
								led = 0x0;
								break;
							case 0xe:
								led = 0x2;
								break;
							case 0xd:
								led = 0x5;
								break;
							case 0xb:
								led = 0x8;
								break;
							case 0x7:
								led = 0x0;
								break;
							}
							break;
						case 0xb:
							switch(rows){
							case 0xf:
								led = 0x0;
								break;
							case 0xe:
								led = 0x3;
								break;
							case 0xd:
								led = 0x6;
								break;
							case 0xb:
								led = 0x9;
								break;
							case 0x7:
								led = 0xf;
								break;
							}
							break;
						case 0x7:
							switch(rows){
							case 0xf:
								led = 0x0;
								break;
							case 0xe:
								led = 0xa;
								break;
							case 0xd:
								led = 0xb;
								break;
							case 0xb:
								led = 0xc;
								break;
							case 0x7:
								led = 0xd;
								break;
							}
							break;
						}

				/* Write output to the LEDs. */
				XGpio_DiscreteWrite(&Leds_Gpio, LED_CHANNEL, led);

					// Shift the '0' in the cols to the left
					cols_msb = (cols >> 3) & 1;  // Saving the msb of cols
					cols = (cols << 1) | cols_msb; // rotate the 4 bit so cols to the left
			}


void Delay(void){
	int counter = 50000;
	while(counter>0)
		counter -= counter;
}


