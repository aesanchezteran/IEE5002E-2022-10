/*
 * axi_timer_pwm_student.c
 *
 *  Created on: 	12 november 2021
 *      Author: 	Alberto Sanchez
 *     Version:		1.0
 */

/********************************************************************************************
* VERSION HISTORY
********************************************************************************************

*	v1.0 - 14 Apr 2021
*
*******************************************************************************************/

/********************************************************************************************
 * This code produces a sine pwm (spwm) wave on pin T14 of the Zybo Board at 1 Hz, 60Hz, 100Hz, 500Hz, and 1Khz. The HW
 * platform requires:
 *
 * 1. LEDs in AXI-GPIO Ch1
 * 2. SW in AXI-GPIO Ch2
 * 3. AXI Timer with PWM enabled and output in pin T14.
 *
 * This is a template for lab project to students to fill in.
 *
 * The PWM signal is generated using an AXI timer set as PWM. The PWM is generated at a constant frequency of 80KHz. The high time is updated using 
 * interruptions.
 *
 * The sine wave is stored in a table which is read at different intervals using a Private Timer interruption.
 * The Private Timer interruption asserts a Private_Timer_Hit flag. This flags is read in the main program and
 * if asserted the sine table is read.
 *
 * Switches are used to select Sine frequencies. The frequencies are synthesized by reading more or less data
 * from the table. This is necessary due to execution time restrictions. If the waveform is not undersampled then
 * the processor does not has enougth time at the high frequencies waveforms.
 *
 *
 ********************************************************************************************/

/* Include Files */
#include "xparameters.h"
#include "xgpio.h"
#include "xstatus.h"
#include "xil_printf.h"
#include "xscutimer.h" // API library for the Private Timer
#include "xtmrctr.h"  //AXI Timer API
#include "xscugic.h"
#include "xil_exception.h"

/* Definitions */
#define GPIO_DEVICE_ID  	XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device for leds and sw */
#define LED_CHANNEL 		1				/* GPIO port 1 for LEDs */
#define SW_CHANNEL 		2				/* GPIO port 2 for SWITCHES */
#define printf 			xil_printf			/* smaller, optimized printf */

#define TIMER_DEVICE_ID		XPAR_XSCUTIMER_0_DEVICE_ID 	/* Device ID for Private Timer */

#define TMRCTR_DEVICE_ID 	XPAR_TMRCTR_0_DEVICE_ID     	/* AXI TMR device ID */
#define TMRCTR_0 		0            		    	/* AXI Timer 0 ID */
#define TMRCTR_1 		1            		    	/* AXI Timer 1 ID */

/* Interruption ID definitions */
#define SW_INT_MASK		XGPIO_IR_CH2_MASK
#define IntC_DEVICE_ID		XPAR_PS7_SCUGIC_0_DEVICE_ID 	// GIC device ID
#define IntC_GPIO_INTERRUPT_ID	XPAR_FABRIC_AXI_GPIO_0_IP2INTC_IRPT_INTR 	// GPIO Interrupt ID
#define TMRCTR_INTERRUPT_ID     XPAR_FABRIC_AXI_TIMER_0_INTERRUPT_INTR		// AXI Timer Interruption ID
#define PTIMER_INTERRUPT_ID	XPAR_SCUTIMER_INTR          	// Private Timer Interruption ID

/* Some constant definitions */
#define PWM_PERIOD              12500    /* PWM period of 80Khz in ns */
#define SINE_TABLE_SIZE		50 	/* Size of Sine Table */


/* ***************************************** 
* STUDENT WORK #1
* 
* The Private Timer should interrupt the processor when its time to read the table,
* thus if you are going to generate a 60 Hz waveform using all 50 points in the table
* you need to interrupt the processor 3000 times per second.
* 
* Justify these value with calculations
*
* Remember that the PERIPHCLK = 667 MHz/2 = 333.5 MHz (Zynq TRM pp.239, 8.2.1 Clocking)
* Use a TIMER_PRESCALER of 1 
*
********************************************
*/
#define TIMER_LOAD_VALUE_60	55582  /* 60 Hz sine wave - synthesyed with 50 points  */
#define TIMER_LOAD_VALUE_100	XXXXX  /* 100 Hz sine wave - synthesyed with 50 points  */
#define TIMER_LOAD_VALUE_500	XXXXX  /* 500 Hz sine wave - synthesyed with 10 points  */
#define TIMER_LOAD_VALUE_1K	XXXXX  /* 1 KHz sine wave - synthesyed with 10 points */
#define TIMER_LOAD_VALUE_1	XXXXX  /* 1 Hz sine wave - synthesyed with 50 points  */


#define TIMER_PRESCALER		1


/************************** Hardware Instances  ******************************/

XScuGic IntC;			/* Instance of the Interrupt Controller */
XGpio GpioInst; 		/* Instance of the AXI GPIO */
XTmrCtr TimerCounterInst;	/* Instance of the AXI Timer */
XScuTimer PrivateTimerInstance;	/* Cortex A9 Scu Private Timer Instance */


/* Variables shared between non-interrupt processing and interrupt processing functions. */
static volatile int   PrivateTimerLoadValue;
static volatile int   SwitchValue;
static volatile int   PeriodTimerHit = FALSE;
static volatile int   HighTimerHit = FALSE;
static volatile int   PrivateTimerHit = FALSE;


/* ***************************************** 
* STUDENT WORK #2
* Calculate 50 points of the PWM High Time in ns for a PWM running at 80 KHz.
*
* You can generate this table by filling an excel table with the following
* columns:
*
*  n | theta       |  D=0.5+0.5*sin(theta)  |  High Time(ns)
* ---|-------------|------------------------|----------------
*  0 | 0           |  0.5                   | 500000
*  1 | 0,125663706 | 0,562666617            | 562667
*
* The following table has been filled with values at 1 KHz
*
********************************************
*/

/* pwm high time SINE_TABLE_SIZE sine values @f_pwm = 1 KHz  */
const int sine[SINE_TABLE_SIZE]={500000,562667,624345,684062,740877,793893,842274,
				885257,922164,952414,975528,991144,999013,999013,991144,
				975528,952414,922164,885257,842274,793893,740877,684062,
				624345,562667,500000,437333,375655,315938,259123,206107,
				157726,114743,77836,47586,24472,8856,987,987,8856,24472,
				47586,77836,114743,157726,206107,259123,315938,375655,437333};


/**************  Function Prototypes **************/

/* Interrupt handler for the SW */
static void SW_Intr_Handler(void *InstancePtr);

/* Interrupt handler for the Private Timer */
static void PrivateTimerIntrHandler(void *InstancePtr);

/* Interrupt configuration routines */
static int IntCInitFunction(XScuGic *IntCtrlPtr,u16 DeviceId, XGpio *GpioInstancePtr, XTmrCtr *AxiTmrInstancePtr,XScuTimer *TimerInstancePtr);

/* Gpio configuration for leds and switches  */
int LedSwConfig(XGpio *GpioPtr, u16 DeviceId);

/* Axi timer configuration  */
int AxiTmrConfig(XTmrCtr *TmrCtrInstancePtr, u16 DeviceId);

/* Private timer configuration  */
int ScuTimerConfig(XScuTimer *TimerInstancePtr, u16 TimerDeviceId);

/*************** Main function ********************/
int main(void){
	int Status;
	u8 pos;  	// table index
	u32 HighTime;   // PWM HighTime
	u8 DutyCycle;   // PWM DutyCycle

	/* Configure Gpio for leds and switches */
	Status = LedSwConfig(&GpioInst, GPIO_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("GPIO Config failed!\r\n");
	}
	xil_printf("GPIO Config Success!\r\n");

	/* Make the first SW reading, output to the LEDS and load Private Timer Value */
	SwitchValue = XGpio_DiscreteRead(&GpioInst,SW_CHANNEL);
	XGpio_DiscreteWrite(&GpioInst, LED_CHANNEL, SwitchValue);


	switch(SwitchValue){
		case 0x0:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_60;  // 60 Hz sine wave - Read all 50 points in the table
			break;
		case 0x1:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_100;  // 100 Hz sine wave - Read all 50 points in the table
			break;
		case 0x2:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_500;  // 500 Hz sine wave 33349 - Read 10 points in the table
			break;
		case 0x3:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_1K;  // 1k Hz sine wave  33349 - Read 10 points in the table
			break;
		default:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_1;  // 1 Hz sine wave - Read all 50 points in the table
	}

	/* Configure Private Timer */
	Status = ScuTimerConfig(&PrivateTimerInstance,TIMER_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("Private Timer Config Failed \r\n");
	}
	xil_printf("Private Timer Config Success \r\n");


	/* Setup interrupt controller and handler connection for Gpio, AxiTmr */
	Status = IntCInitFunction(&IntC, IntC_DEVICE_ID, &GpioInst, &TimerCounterInst, &PrivateTimerInstance);
	if(Status != XST_SUCCESS) {
		  xil_printf("GPIO or AXI Tmr Interruption configuration failed!\r\n");
		  return XST_FAILURE;
	}
	xil_printf("Interruption configuration success!\r\n");


	/* Configure the AXI Timer and selftest */
	Status = AxiTmrConfig(&TimerCounterInst, TMRCTR_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		xil_printf("AXI Timer Config failed!\r\n");
	}
	xil_printf("AXI Timer Config Success!\r\n");


	/* Start the Private Timer */
	XScuTimer_Start(&PrivateTimerInstance);

	pos = 0;

	 while(1){

			if(PrivateTimerHit){

/* ***************************************** 
* STUDENT WORK #3
*
* Read from table on each hit of the Private Timer
* 
* Write code to:
* 1. Disable the AXI TmrCtr PWM mode
* 2. Update the PWM HighTime
* 3. Enable the PWM
* 4. Clear the PrivateTimerHit flag
* 5. Read the table according to pos and the SW (make sure to keep pos within the table boudaries)
* 6. 
*
********************************************
*/

WRITE YOUR CODE HERE


/********************************************/

			} /* End of PrivateTimerHit */

	 }
	return 0;
} /* End of main */



/***********************************************************
 *
 *                Function implementations
 *
 ***********************************************************/


int ScuTimerConfig(XScuTimer * TimerInstancePtr, u16 TimerDeviceId)
{
	int Status;

	XScuTimer_Config *ConfigPtr;

/* ***************************************** 
* STUDENT WORK #4
*
* Write the Private Timer Configuration Routine
*
********************************************
*/

	return XST_SUCCESS;
}


/********** Gpio configuration for leds and switches **********/
int LedSwConfig(XGpio *GpioPtr, u16 DeviceId){
	int Status;

		/* GPIO driver initialisation */
		Status = XGpio_Initialize(GpioPtr, DeviceId);
		if (Status != XST_SUCCESS){
			return XST_FAILURE;
		}

		/*Set the direction for the LEDs to output. */
		XGpio_SetDataDirection(GpioPtr, LED_CHANNEL, 0x0);

		/*Set the direction for the SWITCHES to input. */
		XGpio_SetDataDirection(GpioPtr, SW_CHANNEL, 0xf);

		return XST_SUCCESS;

}/* End of LedSwConfig */



/******** Axi Timer Configuration and SelfTest ********/
int AxiTmrConfig(XTmrCtr *TmrCtrInstancePtr, u16 DeviceId){
	int Status;
	u32 HighTime;
	u8 DutyCycle;


	/* Initialize the axi timer counter */
	Status = XTmrCtr_Initialize(TmrCtrInstancePtr, DeviceId);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/* Perform a self-test on TMR0 to ensure that the hardware was built
	 * correctly. */
	Status = XTmrCtr_SelfTest(TmrCtrInstancePtr, TMRCTR_0);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}


/* ***************************************** 
* STUDENT WORK #5
*
* Configure the AXI Timer
* 
* Write code to:
* 1. Disable the AXI TmrCtr PWM mode
* 2. Read the first value from the sine table and assign to HighTime
* 3. Configure the PWM using XTmrCtr_PwmConfigure()
* 4. Print to console the value of DutyCycle aquired from step 3.
* 5. Enable the AXI TmrCtr PWM mode
*
********************************************
*/

WRITE YOUR CODE HERE


/********************************************/


	return XST_SUCCESS;
} /* End of AxiTmrConfig */



/********** Interrupt setup and handler connection **********/
static int IntCInitFunction(XScuGic *IntCtrlPtr, u16 DeviceId, XGpio *GpioInstancePtr, XTmrCtr *AxiTmrInstancePtr, XScuTimer *TimerInstancePtr)
{
	XScuGic_Config *IntCConfig;
	int status;

	/* Interrupt controller initialization and success check */
	IntCConfig = XScuGic_LookupConfig(DeviceId);

	status = XScuGic_CfgInitialize(IntCtrlPtr, IntCConfig, IntCConfig->CpuBaseAddress);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}



	/* Gpio Handler connection */
	status = XScuGic_Connect(IntCtrlPtr,
				IntC_GPIO_INTERRUPT_ID,
				(Xil_ExceptionHandler)SW_Intr_Handler,
				(void *)GpioInstancePtr);
	if(status != XST_SUCCESS){
		return XST_FAILURE;
	}



	/* Private Timer Handler connection */
	status = XScuGic_Connect(IntCtrlPtr, PTIMER_INTERRUPT_ID,
				(Xil_ExceptionHandler)PrivateTimerIntrHandler,
				(void *)TimerInstancePtr);
	if (status != XST_SUCCESS) {
		return status;
	}

	/* Enable GPIO interrupts */
	XGpio_InterruptEnable(GpioInstancePtr, SW_INT_MASK);
	XGpio_InterruptGlobalEnable(GpioInstancePtr);


	/* Enable Private Timer interrupts */
	XScuTimer_EnableInterrupt(TimerInstancePtr);


	/* Enable GIC */
	XScuGic_Enable(IntCtrlPtr, PTIMER_INTERRUPT_ID);
	XScuGic_Enable(IntCtrlPtr, IntC_GPIO_INTERRUPT_ID);


	/* Initialize the exception table */
	Xil_ExceptionInit();

	/* Enable Exception handlers */
	Xil_ExceptionRegisterHandler(XIL_EXCEPTION_ID_INT,
			 	 	 	 	 	 (Xil_ExceptionHandler)XScuGic_InterruptHandler,
								 IntCtrlPtr);
	Xil_ExceptionEnable();


	return XST_SUCCESS;
} /*End of IntCInitFunction */


void SW_Intr_Handler(void *InstancePtr)
{
	/******************************************************************
	 * The interrupt handler performs the following actions:
	 *
	 * STEP 1: Disable the interrupt
	 * STEP 2: Service the interrupt
	 * STEP 3: Clear the interrupt
	 * STEP 4: Enable the interrupt
	 *
	 *******************************************************************/

	/* STEP 1: Disable Gpio Ch2 interrupts */
	XGpio_InterruptDisable(&GpioInst, SW_INT_MASK);

	/* Ignore additional button presses in Ch2 */
	if ((XGpio_InterruptGetStatus(&GpioInst) & SW_INT_MASK) !=
			SW_INT_MASK) {
			return;
		}

	/* STEP 2: Identify on SWs and light the leds */
	/* Make the first sw reading, output to the leds and load Private Timer Value */
	SwitchValue = XGpio_DiscreteRead(&GpioInst,SW_CHANNEL);
	XGpio_DiscreteWrite(&GpioInst, LED_CHANNEL, SwitchValue);

	switch(SwitchValue){
		case 0x0:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_60;  // 60 Hz sine wave
			break;
		case 0x1:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_100;  // 100 Hz sine wave
			break;
		case 0x2:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_500;  // 500 Hz sine wave
			break;
		case 0x3:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_1K;  // 1 kHz sine wave
			break;
		default:
			PrivateTimerLoadValue =  TIMER_LOAD_VALUE_1;  // 1 Hz sine wave
	}

	/* Stop the Private Timer */
	XScuTimer_Stop(&PrivateTimerInstance);

	XScuTimer_LoadTimer(&PrivateTimerInstance, PrivateTimerLoadValue);

	/* Start the Private Timer */
	XScuTimer_Start(&PrivateTimerInstance);

    /* STEP 3: Clear the interrupt flag in Gpio Ch2*/
    (void)XGpio_InterruptClear(&GpioInst, SW_INT_MASK);

    /* STEP 4: Enable GPIO interrupts in Gpio Ch2 */
    XGpio_InterruptEnable(&GpioInst, SW_INT_MASK);
} /* End of SW_Intr_Handler*/



/* Private Timer Interrupt Handler  */

static void PrivateTimerIntrHandler(void *InstancePtr)
{
	PrivateTimerHit = TRUE;
}


