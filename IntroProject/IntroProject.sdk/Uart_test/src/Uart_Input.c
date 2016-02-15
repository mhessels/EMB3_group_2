//Simple code that writes a text string on the UART, and
//allows the user to give input. The program exits when
//the user have given an input.

//Created by: JCLarsen - 11/02/2016

#include <stdio.h>
#include "platform.h"
#include "xuartps.h"
#include "xgpio.h"
#include "xstatus.h"

typedef int bool;
#define true 1
#define false 0

#define UART_BASEADDR XPAR_PS7_UART_1_BASEADDR			/* Assigns the UART_BASEADDR to the define */
#define GPIO_DEVICE_ID  XPAR_AXI_GPIO_0_DEVICE_ID	/* GPIO device that LEDs are connected to */
#define LED_CHANNEL 1								/* GPIO port for LEDs */
XGpio Gpio;											/* GPIO Device driver instance */

void xil_print(char *str);

int main()
{
	int Status;
	int led = 0; /* Hold current LED value. Initialise to LED definition */

	/* GPIO driver initialisation */
	Status = XGpio_Initialize(&Gpio, GPIO_DEVICE_ID);
	if (Status != XST_SUCCESS) {
		return XST_FAILURE;
	}

	/*Set the direction for the LEDs to output. */
	XGpio_SetDataDirection(&Gpio, LED_CHANNEL, 0x0);

	char userInput = 0;

	init_platform();

	bool running = true;

	while(running){
		/* Flush UART FIFO */
		while (XUartPs_IsReceiveData(UART_BASEADDR))
		{
			XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
		}

		/* Wait for data on UART */
		while (!XUartPs_IsReceiveData(UART_BASEADDR))
		{}

		if (XUartPs_IsReceiveData(UART_BASEADDR))
		{
			userInput = XUartPs_ReadReg(UART_BASEADDR, XUARTPS_FIFO_OFFSET);
		}

		switch(userInput){
		  case '1': print("1\n"); led = 0b1000; break;
		  case '2': print("2\n"); led = 0b0100; break;
		  case '3': print("3\n"); led = 0b0010; break;
		  case '4': print("4\n"); led = 0b0001; break;
		  case 't': print("Terminating\n"); led = 0b1111; running = false; break;
		}
		
		XGpio_DiscreteWrite(&Gpio, LED_CHANNEL, led);
	}
	cleanup_platform();
	return 0;
}
