/*
 * BtnInt.c
 *
 *  Created on: 	7 April 2022
 *      Author: 	Alberto Sanchez
 *     Version:		1.0
 */

/**************************************************************
*
*    SECTION: VERSION HISTORY
*
***************************************************************
*
*	v1.0 - 7 April 2022
*		First version created, modified from Zynq Book tutorial
*
***************************************************************/

/**************************************************************
*
*    SECTION: DESCRIPTION
*
***************************************************************
*
* This file contains an example of using the GPIO driver with
* interruptions from the Zybo buttons and lighting the leds.
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
#include "xscugic.h"
#include "xil_exception.h"
#include "xil_printf.h"

/**************************************************************
*
*    SECTION: DEFINITIONS
*
***************************************************************/
#define GPIO_DEVICE_ID  XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device that LEDs are connected to */
#define LED_CHANNEL 2					/* GPIO port 1 for buttons */
#define BTN_CHANNEL 1				   /* GPIO port 2 for leds */
#define printf xil_printf				/* smaller, optimised printf */

#define INTC_DEVICE_ID 		XPAR_PS7_SCUGIC_0_DEVICE_ID // GIC device ID

#define INTC_GPIO_INTERRUPT_ID XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR // GPIO Interrupt ID

#define BTN_INT 			XGPIO_IR_CH1_MASK //Definition of the button interrupt mask

/**************************************************************
*
*    SECTION: DEVICE INSTANCES
*
***************************************************************/
XGpio Gpio;		/* GPIO Device driver instance */

XScuGic INTCInst; // Instance of the GIC


/**************************************************************
*
*     SECTION: FUNCTION PROTOTYPES
*
***************************************************************/

// Interrupt handler
static void BTN_Intr_Handler(void *baseaddr_p);

// Interrupt handler configuration
static int IntcInitFunction(u16 DeviceId, XGpio *GpioInstancePtr);

// Gpio configuration function
static int GpioInitFunction(u16 DeviceId, XGpio *GpioInstancePtr);

/* Main function. */
int main(void){
	int Status;

	/* Initialise Gpio */
	Status = GpioInitFunction(GPIO_DEVICE_ID,&Gpio);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to initialise Gpio\r\n");
	} else{
		xil_printf("Succesfully initialised Gpio\r\n");
	}

	/* Initialise Interrupt Controller */
	Status = IntcInitFunction(INTC_DEVICE_ID,&Gpio);
	if (Status != XST_SUCCESS) {
		xil_printf("Failed to initialize Interrupt Controller\r\n");
	} else{
		xil_printf("Successfully initialized Interrupt Controller\r\n");
	}

	while(1){    // Infinite loop
	}

	return 0;
}

/**************************************************************
*
* SECTION: PROTOTYPE FUNCTION IMPLEMENTATIONS
*
**************************************************************/

//----------------------------------------------------
// INTERRUPT HANDLER FUNCTIONS
// - called by button interrupt
//----------------------------------------------------


void BTN_Intr_Handler(void *InstancePtr)
{
	/******************************************************************
	 * The interrupt handler performs the following actions:
	 *
	 * 1. Disable the interrupt
	 * 2. Read the interrupt source
	 * 3. Perform an action depending on what it has read
	 * 4. Clear the interrupt
	 * 5. Enable the interrupt
	 * 6. End the interrupt handler
	 *
	 *******************************************************************/

	int btn_value;

	// Disable GPIO interrupts
	XGpio_InterruptDisable(&Gpio, BTN_INT);

	// Ignore additional button presses
	if ((XGpio_InterruptGetStatus(&Gpio) & BTN_INT) !=
			BTN_INT) {
			return;
		}
	btn_value = XGpio_DiscreteRead(&Gpio, BTN_CHANNEL);

    XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, btn_value);

    // Clear the interrupt flag
    (void)XGpio_InterruptClear(&Gpio, BTN_INT);

    // Enable GPIO interrupts
    XGpio_InterruptEnable(&Gpio, BTN_INT);
}


// Gpio configuration function
static int GpioInitFunction(u16 DeviceId, XGpio *GpioInstancePtr){

	int Status;

	/* GPIO driver initialization */
	Status = XGpio_Initialize(GpioInstancePtr, DeviceId);
	if (Status != XST_SUCCESS){
		return XST_FAILURE;
	}

	/*Set the direction for the LEDs to output. */
	XGpio_SetDataDirection(GpioInstancePtr, LED_CHANNEL, 0x0);

	/*Set the direction for the BTN to input. */
	XGpio_SetDataDirection(GpioInstancePtr, BTN_CHANNEL, 0xf);

	return Status;

}


int IntcInitFunction(u16 DeviceId, XGpio *GpioInstancePtr)
{
	// Pointer to Interruption Configuration
	XScuGic_Config *IntcConfig;
	int status;

	// Interrupt controller initialisation and success check
	IntcConfig = XScuGic_LookupConfig(DeviceId);

	status = XScuGic_CfgInitialize(&INTCInst, IntcConfig, IntcConfig->CpuBaseAddress);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Connect GPIO interrupt to handler and check for success
	status = XScuGic_Connect(&INTCInst,
						  	  	 INTC_GPIO_INTERRUPT_ID,
						  	  	 (Xil_ExceptionHandler)BTN_Intr_Handler,
						  	  	 (void *)GpioInstancePtr);
	if(status != XST_SUCCESS) return XST_FAILURE;

	// Enable GIC
	XScuGic_Enable(&INTCInst, INTC_GPIO_INTERRUPT_ID);

	// Enable GPIO interrupts in the button channel.
	XGpio_InterruptEnable(GpioInstancePtr, BTN_CHANNEL);
	XGpio_InterruptGlobalEnable(GpioInstancePtr);


	/*
	 * Initialize the exception table and register the interrupt
	 * controller handler with the exception table
	 */
	Xil_ExceptionInit();

	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 	 	 	 	 	 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
								 &INTCInst);

	/* Enable non-critical exceptions */
	Xil_ExceptionEnable();


	return XST_SUCCESS;
}





