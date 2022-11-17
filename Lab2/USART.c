
#include "st_basic.h"

int main(void)
{
	ClockInit();
	USART2_Init();
	
	USART2_TX_String("Welcome to Embedded System!\n");
	
	while (1)
	{
		USART2_TX(USART2_RX());
	}
}

