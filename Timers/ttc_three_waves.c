/*****************************************************************************/
/**
* ttc_polling.c
*
* This file contains a design example using the Triple Timer Counter hardware
* and driver in  polled mode.
*
* The example generates PWM signals in three pins connecting the PS through the EMIO.
*
* Author: Alberto Sanchez, PhD
*
*
******************************************************************************/

/***************************** Include Files *********************************/

#include "xparameters.h"
#include "xstatus.h"
#include "xttcps.h"
#include "xil_printf.h"

/************************** Constant Definitions *****************************/

/* Input freq Processor Frequency */
#define PCLK_FREQ_HZ		XPAR_XTTCPS_0_CLOCK_HZ

//Three counter of TTC0
#define TTC_NUM_DEVICES		3

//This will run for 256 cycles
#define MAX_LOOP_COUNT		0xFF

/**************************** Type Definitions *******************************/

/*****************************************************************************
 *
 * This structure allows to easily store the configuration parameters we
 * want to produce in the output pins.
 *
 * ***************************************************************************/

typedef struct {
	u32 OutputHz;		/* The frequency the timer should output on the
				   waveout pin */
	u8 OutputDutyCycle;	/* The duty cycle of the output wave as a
				   percentage */
	u8 PrescalerValue;	/* Value of the prescaler in the Count Control
				   register */
} TmrCntrSetup;




/********************************** Definitions *****************************/

/*
 * Convert from a 0-2 index to the correct timer counter number as defined in
 * xttcps_hw.h
 *
 * ttc0 Base Address is 0xF8001000:
 * registers for the three timer/counters offsett from this base address
 *
 * ttc1 Base Address is 0xF8002000
 * registers for the three timer/counters offsett from this base address
 *
 * The addresses for timer0, timer1, and timer 2 from ttc0 are
 *
 * XPAR_XTTCPS_0_BASEADDR = 0xF8001000     timer0 from ttc0
 * XPAR_XTTCPS_1_BASEADDR = 0xF8001004	   timer1 from ttc0
 * XPAR_XTTCPS_2_BASEADDR = 0xF8001008 	   timer2 from ttc0
 *
 * More information on register addresses in pp.1752 of the UG585 Zynq TRM
 *
 */

static u32 TimerCounterBaseAddr[] = {
	XPAR_XTTCPS_0_BASEADDR,
	XPAR_XTTCPS_1_BASEADDR,
	XPAR_XTTCPS_2_BASEADDR
};

/*
 * This table provides the prescaler setting based on the prescaler value from
 * 0-15. The setting is 2^(prescaler value + 1). Use a table to avoid doing
 * powers at run time.
 *
 * A Prescaler value of 16, means use no prescaler, or a prescaler value of 1.
 * Prescale value (N): if prescale is enabled, the count rate is divided by 2^(N+1)
 *
 * Prescaler value (N) are bits 4:1 of the clock control register of each timer
 *
 * pp.1754 ug585 Zynq TRM
 *
 */
static u32 PrescalerSettings[] = {
	2, 4, 8, 16, 32, 64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384,
	32768, 65536, 1
};

/*
 * This table contains different settings for frequency, duty cycle % and PrescalerSettings index
 * for the three timers
 */

static TmrCntrSetup SettingsTable[] = {
	/* Table offset of 0 */
	{10, 50, 6},
	{10, 25, 6},
	{10, 75, 6},

	/* Table offset of 3 */
	{100, 50, 3},
	{200, 25, 2},
	{400, 12, 1},

	/* Table offset of 6 */
	{500, 50, 1},
	{1000, 50, 0},
	{5000, 50, 16},

	/* Table offset of 9 */
	{10000, 50, 16},
	{50000, 50, 16},
	{100000, 50, 16},

	/* Table offset of 12 */
	{500000, 50, 16},
	{1000000, 50, 16},
	{5000000, 50, 16},
	/* Note: at greater than 1 MHz the timer reload is noticeable. */

};

/* Size of the Settings Table to check not to access invalid memory addresses */

#define SETTINGS_TABLE_SIZE  (sizeof(SettingsTable)/sizeof(TmrCntrSetup))



/************************** Function Prototypes *****************************
 *
 * This function will contain the actual execution of polling the timer
 *
 * **************************************************************************/
static int TmrCtrExample(u8 SettingsTableOffset);



/*****************************************************************************
* This function is the main function of the Timer/Counter example.
*
* @return	XST_SUCCESS to indicate success, else XST_FAILURE to indicate a Failure.
*
*****************************************************************************/
int main(void)
{
	int Status;

	xil_printf("TTC Example \r\n");

	Status = TmrCtrLowLevelExample(TABLE_OFFSET);

	if (Status != XST_SUCCESS) {
		xil_printf("TTC Lowlevel Example Test Failed\r\n");
		return XST_FAILURE;
	}

	xil_printf("Successfully ran TTC Example Test\r\n");
	return XST_SUCCESS;
}

/*****************************************************************************/
/*
* This function will generate three PWM signals in three pins connected
* to the EMIO.
*
* Each timer is configured in interval mode and will make use of the matching
* register to compare with the counter and flip the output bit.
*
* The matching register will generate an interrupt each time it the comparison
* matches
*
*
* @param	SettingsTableOffset is an offset into the settings table. This
*		allows multiple counter setups to be kept and swapped easily.
*
* @return	XST_SUCCESS to indicate success, else XST_FAILURE to indicate
*		a Failure.
*
* @note
*
* This function contains a loop which waits for the value of a timer counter
* to change.  If the hardware is not working correctly, this function may not
* return.
*
****************************************************************************/
int TmrCtrExample(u8 SettingsTableOffset)
{
	u32 RegValue;	//This variable will store register values before loading them
	u32 LoopCount;	//This variable will count the number of loops equal to the number of devices
	u32 TmrCtrBaseAddress;					//Timer/Counter Base Address
	u32 IntervalValue, MatchValue;	//Values to be loaded into the IntervalRegister and the MatchingRegister
	TmrCntrSetup *CurrSetup;				//Pointer to structure with the Current Setup


	//Just checking we are not outside the table boundaries
	if ((SettingsTableOffset + 2) > SETTINGS_TABLE_SIZE) {
		return XST_FAILURE;
	}

	// This loops around the timers from ttc0 configuring them
	for (LoopCount = 0; LoopCount < TTC_NUM_DEVICES; LoopCount++) {

		// Retrieve the timer base address from the TimerCounterBaseAddr Array
		TmrCtrBaseAddress = TimerCounterBaseAddr[LoopCount];

		// Make CurrSetup Pointer point to the desired setup in the SettingsTable
		CurrSetup = &SettingsTable[SettingsTableOffset + LoopCount];

		/*
		 * Set the Clock Control Register prescaler value
		 * Prescale value (N): if prescale is enabled, the count rate is divided by 2^(N+1)
		 *
		 * Bits 4:1 of the clock control register of each timer
		 *
		 * pp.1754 ug585 Zynq TRM
		 *
		 * This code assings to RegValue the PrescalerValue of the CurrSetup structure
		 * Since the PrescalerValue is bits 4:1 of the clock control register, then it is
		 * necessary to shift the bits 1 position to the left before assigning the value to
		 * RegValue. The label XTTCPS_CLK_CNTRL_PS_VAL_SHIFT contains the number of bits to
		 * shift so we dont have to remember.
		 *
		 * The AND operation with XTTCPS_CLK_CNTRL_PS_VAL_MASK (0x0000001E) leaves only bits 1:4
		 * and filter anything else before assigning.
		 *
		 */
		if (16 > CurrSetup->PrescalerValue) {

			/* Assign prescaler bits to RegValue to be loaded into clk control register */
			RegValue =
				(CurrSetup->
				 PrescalerValue <<
				 XTTCPS_CLK_CNTRL_PS_VAL_SHIFT) &
				XTTCPS_CLK_CNTRL_PS_VAL_MASK;

			/*
			 * This statement makes an OR with XTTCPS_CLK_CNTRL_PS_EN_MASK which stands for the enable bit
			 * of the clock control register, therefore we set the prescaler and enable bits at the same time
			 */
			RegValue |= XTTCPS_CLK_CNTRL_PS_EN_MASK;   //bitwise or a=a|b
		}
		else {
			/* Do not use the clock prescaler */
			RegValue = 0;
		}

		/* XTTCPS_CLK_CNTRL_OFFSET is the offset of the ttc0 timer Clock Control Register
		 * This statement writes the value of RegValue into the clock control register of the timer */
		XTtcPs_WriteReg(TmrCtrBaseAddress, XTTCPS_CLK_CNTRL_OFFSET,
				  RegValue);


		/*
		 * Set the Interval register. This determines the frequency of
		 * the waveform. The counter will be reset to 0 each time this
		 * value is reached.
		 *
		 * This code computes the interval (time) as,
		 *
		 * IntervalValue = (processor frequency)/(prescaler * OutputHz)
		 *
		 */
		IntervalValue = PCLK_FREQ_HZ /
			(u32) (PrescalerSettings[CurrSetup->PrescalerValue] *
			       CurrSetup->OutputHz);

		/*
		 * Make sure the value is not too large or too small
		 */
		if ((65535 < IntervalValue) || (4 > IntervalValue)) {
			return XST_FAILURE;
		}

		// Writes the IntervalValue into the Interval Register
		XTtcPs_WriteReg(TmrCtrBaseAddress,
				  XTTCPS_INTERVAL_VAL_OFFSET, IntervalValue);

		/*
		 * Set the Match register. This determines the duty cycle of the
		 * waveform. The waveform output will toggle each time this
		 * value is reached.
		 *
		 * Recall that the OutputDutyCycle is in percentage, thus the
		 * division by 100.
		 *
		 */
		MatchValue = (IntervalValue * CurrSetup->OutputDutyCycle) / 100;

		/*
		 * Make sure the value is not to large or too small
		 */
		if ((65535 < MatchValue) || (4 > MatchValue)) {
			return XST_FAILURE;
		}

		// Write into the match register
		XTtcPs_WriteReg(TmrCtrBaseAddress, XTTCPS_MATCH_0_OFFSET,
				  MatchValue);

		/*
		 * Set the Counter Control Register
		 *
		 * XTTCPS_CNT_CNTRL_DIS_MASK -> Disable counter
		 * XTTCPS_CNT_CNTRL_EN_WAVE_MASK -> Output waveform enable, active low.
		 * XTTCPS_CNT_CNTRL_INT_MASK -> When this bit is high, the timer is in Interval Mode
		 * XTTCPS_CNT_CNTRL_MATCH_MASK -> Register Match mode: when Match is set, an interrupt
		 * 								  is generated when the count value matches one of the
		 * 								  three match registers and the corresponding bit is
		 * 								  set in the Interrupt Enable register.
		 * XTTCPS_CNT_CNTRL_RST_MASK -> Setting this bit high resets the counter value and
		 *								restarts counting
		 *
		 * So the following statement calculates the register value so the timer:
		 * 1. Is enabled and outputs the waveform
		 * 2. Sets interval mode, Match mode and resets the counter
		 *
		 */
		RegValue =
			~(XTTCPS_CNT_CNTRL_DIS_MASK |
			  XTTCPS_CNT_CNTRL_EN_WAVE_MASK) &
			(XTTCPS_CNT_CNTRL_INT_MASK |
			 XTTCPS_CNT_CNTRL_MATCH_MASK |
			 XTTCPS_CNT_CNTRL_RST_MASK);

		// Write to counter control register
		XTtcPs_WriteReg(TmrCtrBaseAddress, XTTCPS_CNT_CNTRL_OFFSET,
				  RegValue);

		/*
		 * Write to the Interrupt enable register. The status flags are
		 * not active if this is not done.
		 */
		XTtcPs_WriteReg(TmrCtrBaseAddress, XTTCPS_IER_OFFSET,
				  XTTCPS_IXR_INTERVAL_MASK);
	}

	LoopCount = 0;
	while (LoopCount < MAX_LOOP_COUNT) {

		/*
		 * Read the status register for debugging
		 */
		RegValue =
			XTtcPs_ReadReg(TmrCtrBaseAddress, XTTCPS_ISR_OFFSET);

		/*
		 * Write the status register to clear the flags
		 */
		XTtcPs_WriteReg(TmrCtrBaseAddress, XTTCPS_ISR_OFFSET,
				  RegValue);

		if (0 != (XTTCPS_IXR_INTERVAL_MASK & RegValue)) {
			LoopCount++;
			/*
			 * Count the number of output cycles so the program will
			 * eventually exit. Otherwise it would stay in this loop
			 * indefinitely.
			 */
		}
	}

	return XST_SUCCESS;
}
