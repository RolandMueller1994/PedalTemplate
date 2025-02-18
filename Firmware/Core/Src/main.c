/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "midi.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdbool.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

// Main loop delay of 1ms
#define LOOP_DELAY 1
// Switch debounce delay in ms
#define DEBOUNCE_DELAY 5
#define LONG_DELAY 250
// Number of switches
#define NO_SWITCHES 3

// MIDI
#define MIDI_CHANNEL 0
#define MIDI_CONTROL 20

// Flash
#define FLASH_START_ADDR 0x0800FC00

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

// Array to store switches state change flags
bool swEdgesPush[NO_SWITCHES];
bool swEdgesRelease[NO_SWITCHES];

// Array to store switches states
bool swStates[NO_SWITCHES];
bool swLongState[NO_SWITCHES];
bool swLongRelease[NO_SWITCHES];

// Array for debounce counters and flags
uint8_t swCount[NO_SWITCHES];
bool swDoCount[NO_SWITCHES];
uint16_t swLongCount[NO_SWITCHES];

// Array stating the GPIOs used for switches
GPIO_TypeDef *switchBank[NO_SWITCHES] = { GPIOA, GPIOB, GPIOC };
uint16_t switches[NO_SWITCHES] = { SW_Pin, ExtSW_Pin, InConn_Pin };

// Array stating which switches are latched and which are momentary switches
// false 	-> momentary
// true 	-> latched
bool swLatches[NO_SWITCHES] = { false, true, true };

// Effect on flag
bool effectOn = false;

// Midi Learn
bool midiLearn = false;

// Midi
struct midiMsg msg;
bool uartError = false;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

void readSwitches() {

	for (int i = 0; i < NO_SWITCHES; i++) {
		GPIO_PinState pinState = HAL_GPIO_ReadPin(switchBank[i], switches[i]);

		swEdgesPush[i] = false;
		swEdgesRelease[i] = false;
		swLongRelease[i] = false;

		// Switches are active low
		if (pinState == GPIO_PIN_SET && swStates[i]) {
			// Switch open
			if (!swDoCount[i]) {
				// Start debounce
				swDoCount[i] = true;
			} else {
				if (swCount[i] == DEBOUNCE_DELAY) {
					swEdgesRelease[i] = true;
					swCount[i]++;
				} else if (swCount[i] == DEBOUNCE_DELAY + 1) {
					swDoCount[i] = false;
					swStates[i] = false;
					swCount[i] = 0;
				} else {
					swCount[i]++;
				}
			}
		} else if (pinState == GPIO_PIN_RESET && !swStates[i]) {
			// Switch closed
			if (!swDoCount[i]) {
				// Start debounce
				swDoCount[i] = true;
			} else {
				if (swCount[i] == DEBOUNCE_DELAY) {
					swEdgesPush[i] = true;
					swCount[i]++;
				} else if (swCount[i] == DEBOUNCE_DELAY + 1) {
					swDoCount[i] = false;
					swStates[i] = true;
					swCount[i] = 0;
				} else {
					swCount[i]++;
				}
			}
		} else {
			swDoCount[i] = false;
			swCount[i] = 0;
		}

		if (swStates[i] && !swLatches[i]) {
			if (swLongCount[i] < LONG_DELAY) {
				swLongCount[i]++;
			} else if (swLongCount[i] == LONG_DELAY) {
				swLongState[i] = true;
				swLongCount[i]++;
			}
		} else if (!swLatches[i]) {
			if (swLongState[i]) {
				swLongRelease[i] = true;
			}
			swLongState[i] = false;
			swLongCount[i] = 0;
		}
	}
}

void updateState() {
	bool swLong = false;
	bool swPushEdge = false;
	bool inConn = false;
	for (int i = 0; i < NO_SWITCHES; i++) {
		if ((switches[i] == SW_Pin) && swEdgesPush[i]) {
			if (!midiLearn)
				effectOn = !effectOn;
			swPushEdge = true;
		} else if ((switches[i] == SW_Pin) && swLongRelease[i] && effectOn) {
			if (!midiLearn)
			effectOn = false;
		} else if (switches[i] == ExtSW_Pin && !effectOn) {
			if (swEdgesPush[i])
				effectOn = true;
			else if (swEdgesRelease[i])
				effectOn = false;
		}

		if (!midiLearn) {
			if (switches[i] == SW_Pin && swLongState[i])
				swLong = true;
			else if (switches[i] == InConn_Pin && swStates[i])
				inConn = true;
		}
	}

	if (swLong && inConn) {
		midiLearn = true;
		effectOn = false;
	} else if (midiLearn && swPushEdge) {
		midiLearn = false;
	}
}

void saveChannelControlFlash(uint8_t channel, uint8_t control) {
	HAL_FLASH_Unlock();
	FLASH_EraseInitTypeDef EraseInitStruct;
	EraseInitStruct.TypeErase = FLASH_TYPEERASE_PAGES;
	EraseInitStruct.PageAddress = FLASH_START_ADDR;
	EraseInitStruct.NbPages = 1;

	uint32_t PageError;
	if (HAL_FLASHEx_Erase(&EraseInitStruct, &PageError) != HAL_OK) { //Erase the Page Before a Write Operation
		HAL_FLASH_Lock();
		return;
	}

	HAL_Delay(50);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_START_ADDR,
			(uint64_t) channel);
	HAL_Delay(50);
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, FLASH_START_ADDR + 4,
			(uint64_t) control);
	HAL_Delay(50);
	HAL_FLASH_Lock();
}

void startUartInterrupts(uint8_t* buffer) {
	HAL_StatusTypeDef status = HAL_UARTEx_ReceiveToIdle_IT(&huart2, buffer, 3);
	if (status != HAL_OK || (huart2.Instance->ISR & (1 << 3))) {
		uartError = true;
	} else {
		uartError = false;
	}
}

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {

	/* USER CODE BEGIN 1 */

	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	for (int i = 0; i < NO_SWITCHES; i++) {
		swEdgesPush[i] = false;
		swEdgesRelease[i] = false;
		swStates[i] = false;
		swLongState[i] = false;
		swLongRelease[i] = false;
		swCount[i] = 0;
		swLongCount[i] = 0;
		swDoCount[i] = false;
	}

	midiInit();

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_USART2_UART_Init();
	/* USER CODE BEGIN 2 */

	bool reset = true;

	for (int i = 0; i < 20; i++) {
		if (HAL_GPIO_ReadPin(GPIOA, SW_Pin) == GPIO_PIN_SET) {
			reset = false;
			break;
		}
		HAL_Delay(100);
	}

	uint8_t channel, control;
	if (reset) {
		saveChannelControlFlash(MIDI_CHANNEL, MIDI_CONTROL);
		uint16_t blinkCount = 0;
		for (uint8_t i = 0; i < 4; ) {
			HAL_GPIO_WritePin(GPIOB, LED_Pin,
					blinkCount % 2 == 0 ? GPIO_PIN_SET : GPIO_PIN_RESET);
			HAL_Delay(250);
			// Wait till main switch is release to not get into midi learn immediately
			if (HAL_GPIO_ReadPin(GPIOA, SW_Pin) == GPIO_PIN_SET)
				i++;
			blinkCount++;
		}
		channel = MIDI_CHANNEL;
		control = MIDI_CONTROL;
	} else {
		channel = (uint8_t) *(__IO uint32_t*) FLASH_START_ADDR;
		control = (uint8_t) *(__IO uint32_t*) (FLASH_START_ADDR + 0x4);
	}

	startUartInterrupts(midiGetCurrentBuffer());

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	int loopCount = 0;
	while (1) {
		readSwitches();
		updateState();

		if (uartError || (huart2.Instance->ISR & (1 << 3))) {
			startUartInterrupts(midiGetCurrentBuffer());
		}

		HAL_GPIO_WritePin(GPIOB, Relay_Pin,
				effectOn & !midiLearn ? GPIO_PIN_SET : GPIO_PIN_RESET);
		if (midiLearn && loopCount == (500 / LOOP_DELAY)) {
			HAL_GPIO_TogglePin(GPIOB, LED_Pin);
			loopCount = 0;
		} else if (!midiLearn){
			HAL_GPIO_WritePin(GPIOB, LED_Pin,
					effectOn ? GPIO_PIN_SET : GPIO_PIN_RESET);
		}

		do {
			midiGetMessage(&msg);
			if (midiLearn) {
				loopCount += 1;
				if (msg.msgType == MIDI_CC) {
					channel = msg.channel;
					control = msg.val1;
					saveChannelControlFlash(channel, control);
					midiLearn = false;
					loopCount = 0;
				}
			} else if (msg.msgType == MIDI_CC) {
				if (msg.channel == channel && msg.val1 == control)
					effectOn = msg.val2 > 63;
			}
		} while (msg.msgType != MIDI_NA);

		HAL_Delay(LOOP_DELAY);

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
	RCC_OscInitStruct.HSEState = RCC_HSE_ON;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
	RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL4;
	RCC_OscInitStruct.PLL.PREDIV = RCC_PREDIV_DIV1;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK) {
		Error_Handler();
	}
	HAL_RCC_MCOConfig(RCC_MCO, RCC_MCO1SOURCE_HSE, RCC_MCODIV_1);
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void) {

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 31250;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };
	/* USER CODE BEGIN MX_GPIO_Init_1 */
	/* USER CODE END MX_GPIO_Init_1 */

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GPIOB, LED_Pin | Relay_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : InConn_Pin */
	GPIO_InitStruct.Pin = InConn_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(InConn_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ExtSW_Pin */
	GPIO_InitStruct.Pin = ExtSW_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(ExtSW_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pins : LED_Pin Relay_Pin */
	GPIO_InitStruct.Pin = LED_Pin | Relay_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/*Configure GPIO pin : PA8 */
	GPIO_InitStruct.Pin = GPIO_PIN_8;
	GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	GPIO_InitStruct.Alternate = GPIO_AF0_MCO;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/*Configure GPIO pin : SW_Pin */
	GPIO_InitStruct.Pin = SW_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(SW_GPIO_Port, &GPIO_InitStruct);

	/* USER CODE BEGIN MX_GPIO_Init_2 */
	/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t size) {
	uint8_t* buffer = midiMessageReceived();
	startUartInterrupts(buffer);
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void) {
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1) {
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
