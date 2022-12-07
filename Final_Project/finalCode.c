#define SAFETY_AREA 10
#include "st_basic.h"

//Keypad variables
const char keypad[16] = { '1', '2', '3', 'A',
	                        '4', '5', '6', 'B',
	                        '7', '8', '9', 'C',
	                        '*', '0', '#', 'D' };

const char password[4] = { '7', '7', '7', '7' };
unsigned char isPushed[16] = { };
unsigned int sequence = 0;

//Timer variables
unsigned char flagCaptureMiss = 0;

//Stepper variables
const int stepperPin[4] = { 2, 3, 6, 7 };
const unsigned int stepperFullState[4][4] = { { 1, 1, 0, 0 },
                                              { 0, 1, 1, 0 },
																							{ 0, 0, 1, 1 },
																							{ 1, 0, 0, 1 } };
const unsigned int stepperHalfState[8][4] = { { 1, 0, 0, 0 },
                                              { 1, 1, 0, 0 },
																			    		{ 0, 1, 0, 0 },
																			    		{ 0, 1, 1, 0 },
																			    		{ 0, 0, 1, 0 },
																				    	{ 0, 0, 1, 1 },
																				    	{ 0, 0, 0, 1 },
																				    	{ 1, 0, 0, 1 } };
int currentStep = 0;
int fullStep = 0;
//Global variable to know whether the motor is moving or stopped
unsigned char isMotorMove = 0;
																							
//For loop variables																					
int i,j ;
																							
//Function Declaration

//Handle the password processing with the given button input
void PasswordInput(char character);
//Initialize timer2 by input capture
void Timer2_Init(void);
//Use an ultrasonic sensor with input capture
unsigned int InputCapture(void);
//Calculate the distance with the timer capture value
unsigned int DistanceCalc(unsigned int value);
//Drive the stepper motor by a given amount of steps
void Step(int step);
//Create an interruption with center joystick
void EXTI0_IRQHandler(void);
																							

																							
int main(void)
{
	ClockInit();
	USART2_Init();
	Timer2_Init();
	
	//Keypad GPIO initialization
	for (i = 0; i <= 3; i++) GPIO_Init(GPIOA, i, GPIO_INPUT_PULLDOWN);
	
	//Ultrasonic GPIO initialization
	GPIO_Init(GPIOC, 14, GPIO_OUTPUT);
	
	//Green LED GPIO initialization
	GPIO_Init(GPIOE, 8, GPIO_OUTPUT);
	
	//Joystick (1, 2, 3, 5) GPIO initialization
	GPIO_Init(GPIOA, 5, GPIO_INPUT_PULLDOWN);
	
	//Interrupt button (center) 
	GPIO_Init(GPIOA, 0, GPIO_INPUT_PULLDOWN);
	
	//Stepper GPIO initialization
	for (i = 0; i < 4; i++)
		GPIO_Init(GPIOB, stepperPin[i], GPIO_OUTPUT);
	
	while (1)
	{
		//Motor is moving
		//Read the distance between the machine and the obstacle
		//If there is a person nearby, stop the motor and send an error message via USART
		if (isMotorMove)
		{
			//Read the distance and check whether the value is too short
			//If the distance is too short, send an error message and stop the motor
			if (DistanceCalc(InputCapture()) < SAFETY_AREA)
			{
				USART2_TX_String("ERROR: worker is too close to the operating machine\n");
				isMotorMove = 0;
			}
			//If the distance is short, send a message and blink the green led
			else if (DistanceCalc(InputCapture()) < 300) 
			{
					USART2_TX_String("Attention: worker is close to the operating machine\n");
				  
				  //The Green LED blink frquency in proportional to the distance
          while(1) 
					{
						//Green LED ON
						GPIOE->BSRR = GPIO_BSRR_BS8;
						Delay(2*DistanceCalc(InputCapture()));
						//Green LED OFF
						GPIOE->BSRR = GPIO_BSRR_BR8;
						Delay(2*DistanceCalc(InputCapture()));
					}
			}
			//If the distance is long enough, continue spinning
			//Controle the motor with the joystick or create an interrupt 
			else 
			{
				//USART2_TX_String("Use the joystick to run the motor\n");
				if (GPIOA->IDR & GPIO_IDR_ID1) {
					USART2_TX_String("motor turn clockwisely half_step\n");
					fullStep = 0;
					Step(-200);	}
				else if (GPIOA->IDR & GPIO_IDR_ID2) {
					USART2_TX_String("motor turn counter clockwisely half_step\n");
					fullStep = 0;
					Step(200);			}
				else if (GPIOA->IDR & GPIO_IDR_ID3) {
					USART2_TX_String("motor turn clockwisely full_step\n");
					fullStep = 1;
					Step(-200);}
				else if (GPIOA->IDR & GPIO_IDR_ID5) {
					USART2_TX_String("motor turn counter clockwisely half_step\n");
					fullStep = 1;
					Step(200);			}
				
				else Step(1);
					
				NVIC->ISER[EXTI0_IRQn / 32] = (1 << (EXTI0_IRQn % 32));
				EXTI->IMR1 |= EXTI_IMR1_IM0;
				EXTI->RTSR1 |= EXTI_RTSR1_RT0;
    	}
		}
		
		//Motor is stopped
		//Scan the keypad and check the password
		//If the password matches, activate the motor
		else
		{
			//Scan all the rows
			for (i = 0; i < 4; i++)
			{
				//Clear the currently activated output during a short period
				//After discharging the capacitor, set the line to analog mode
				GPIOE->BSRR |= (1 << ((i - 1 + 4) % 4 + 12 + 16));
				Delay(1);
				GPIO_Init(GPIOE, (i - 1 + 4) % 4 + 12, GPIO_ANALOG);
				
				//Set the next row to output mode and set the output to high
				//Wait for the capacitor to be charged
				GPIO_Init(GPIOE, i + 12, GPIO_OUTPUT);
				GPIOE->BSRR |= (1 << (i + 12));
				Delay(1);
				
				//Scan all the columns
				for (j = 0; j < 4; j++)
				{
					//Check if the input is high
					if (GPIOA->IDR & (1 << j))
					{
						//If the button is pushed for the first time, get it as a true input
						//Mark the flag not to get the input continuously
						if (!isPushed[4 * i + j])
						{
							PasswordInput(keypad[4 * i + j]);
							isPushed[4 * i + j] = 1;
							USART2_TX(keypad[4 * i + j]);
							USART2_TX('\n');
						}
					}
					
					//If the button is not pushed, clear the flag to get the next button push
					else isPushed[4 * i + j] = 0;
				}
			}
		}
	}
}

//Interruption with a message 
void EXTI0_IRQHandler(void)
{
	USART2_TX_String("I got a button input!!!\n");
	EXTI->PR1 |= EXTI_PR1_PIF0;
}

void PasswordInput(char character)
{
	//Check if the input character matches the password
	if (password[sequence] == character)
	{
		//Check if the input character is the last character
		if (sequence == 3)
		{
			//If so, start rotating the motor and reset the sequence index
			isMotorMove = 1;
			sequence = 0;
		}
		
		//If the input character is not the last character, keep getting input
		else sequence++;
	}
	
	//If the password is wrong, reset the sequence index
	else sequence = 0;
}

void Timer2_Init(void)
{
	//Enable the clock to the timer
	RCC->APB1ENR1 |= RCC_APB1ENR1_TIM2EN;
	//Set PA5 to the timer channel pin
	GPIO_Init(GPIOA, 5, GPIO_ALTERNATIVE);
	GPIOA->AFR[0] |= (1 << GPIO_AFRL_AFSEL5_Pos);
	
	//Set the digital filter to sample 8 times and make the channel as an input
	TIM2->CCMR1 |= (3 << TIM_CCMR1_IC1F_Pos) | TIM_CCMR1_CC1S_0;
	//Make the input capture unit sensitive to both rising and falling edges
	TIM2->CCER |= (TIM_CCER_CC1NP | TIM_CCER_CC1P | TIM_CCER_CC1E);
	
	//Enable the timer
	TIM2->CR1 |= TIM_CR1_CEN;
}

unsigned int InputCapture(void)
{
	//Check flagCaptureMiss
	switch (flagCaptureMiss)
	{
		//1 when the capture is timed out and waiting for the rising edge
		case 1:
			//Check if the rising edge has occured
			//In that case, clear the input capture flag and set the state to 2
			if (TIM2->SR & TIM_SR_CC1IF)
			{
				TIM2->SR &= ~TIM_SR_CC1IF;
				flagCaptureMiss = 2;
			}
			
			//After that, return the value as there is no obstacle
			return 500 * 80 * 58;
			
		//2 when the input is high and waiting for the falling edge
		case 2:
			//Check if the falling edge has occured
			//In that case, reset all the flag and get ready for the next input capture
			if (TIM2->SR & TIM_SR_CC1IF)
			{
				TIM2->SR &= ~TIM_SR_CC1IF;
				flagCaptureMiss = 0;
			}
			
			//If not, just return the value as there is no obstacle
			else return 500 * 80 * 58;
			break;
			
		//0 when the capturing is ready
		//In that case, do nothing and proceed the input capture
		default: break;
	}
	
	//Send a pulse to the ultrasonic sensor to get a distance value
	GPIOC->BSRR = GPIO_BSRR_BS14;
	Delay(1);
	GPIOC->BSRR = GPIO_BSRR_BR14;
	
	//Reset the counter value and wait for the rising edge
	//If the rising edge does not appear until 20ms, abort the input capture
	//That is regarded as there is no obstacle
	TIM2->CNT = 0;
	while (!(TIM2->SR & TIM_SR_CC1IF)) if (TIM2->CNT >= 20 * 80 * 1000)
	{
		flagCaptureMiss = 1;
		return 500 * 80 * 58;
	}
	//Ignore the rising edge by clearing the flag
	TIM2->SR &= ~TIM_SR_CC1IF;
	
	//Reset the counter value and wait for the falling edge
	TIM2->CNT = 0;
	while (!(TIM2->SR & TIM_SR_CC1IF));
	
	//Read and return the captured value
	return TIM2->CCR1;
}

unsigned int DistanceCalc(unsigned int value)
{
	//Captured value is based on 80MHz, so divide it by 80 to make it 1us time base
	//According to the datasheet, the value divided by 58 returns the length in the unit of 'cm'
	USART2_TX(((value / (80 * 58)) / 100) % 10 + '0');
	USART2_TX(((value / (80 * 58)) / 10) % 10 + '0');
	USART2_TX((value / (80 * 58)) % 10 + '0');
	USART2_TX('\n');
	return value / (80 * 58);
}

void Step(int step)
{
	//Inspect the direction by the sign of the step
	int direction = (step > 0) ? 1 : -1;
	
	//Repeat until the remaining step becomes 0
	while (step != 0)
	{
		//Increment or decrement 1 step according to the direction
		if (fullStep) currentStep = (currentStep + direction + 4) % 4; 
		else currentStep = (currentStep + direction + 8) % 8;

		//Set or clear the GPIO outputs according to the step sequence
		for (i = 0; i < 4; i++)
		{
			if (fullStep) {
			GPIOB->BSRR = (stepperFullState[currentStep][i] << stepperPin[i]);
			GPIOB->BSRR = (!stepperFullState[currentStep][i] << (stepperPin[i] + 16)); }
			else {
			GPIOB->BSRR = (stepperHalfState[currentStep][i] << stepperPin[i]);
			GPIOB->BSRR = (!stepperHalfState[currentStep][i] << (stepperPin[i] + 16)); }
		}
		//Decrease the remaining step
		step -= direction;
		//Wait for the rotor to rotate
		Delay(2);
		
	}
}