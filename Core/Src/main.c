/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <string.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define PATH_SIZE 				32
#define CMD_SIZE				100
#define BUF_RW_SIZE				1000
#define END_OF_FILE				26
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
SD_HandleTypeDef hsd;

UART_HandleTypeDef huart1;

/* USER CODE BEGIN PV */
FATFS fatfs;
FIL myfile;
FRESULT fresult;
FILINFO fno;
DIR dir;

uint8_t cmd[CMD_SIZE];
uint8_t buff_read[BUF_RW_SIZE];
uint8_t buff_write[BUF_RW_SIZE];
UINT byte_read = 0;
uint8_t cmd_buf_index = 0;
uint8_t write_buf_index = 0;
uint8_t cmd_length = 0;
TCHAR path[PATH_SIZE];
char mess_buff[100];
uint32_t i;
uint8_t level = 0;

typedef enum {
	LEVEL_INCREASE,
	LEVEL_DECREASE,
	LEVEL_NOT_CHANGE
}Level_State_t;

typedef enum {
	CMD_READY,
	CMD_BUSY,
	CMD_CAN_USE
}Cmd_State_t;

typedef enum {
	MODE_CMD,
	MODE_WRITE
}Mode_t;

typedef enum {
	CMD_LINE_SHOW,
	CMD_LINE_HIDE
}CMD_Line_t;

Cmd_State_t cmd_state = CMD_READY;
Level_State_t level_state = LEVEL_NOT_CHANGE;
Mode_t mode = MODE_CMD;
CMD_Line_t cmd_line = CMD_LINE_SHOW;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SDIO_SD_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	if(mode == MODE_CMD) {
		if(cmd_state == CMD_READY) {
			cmd_buf_index++;
			if(cmd[cmd_buf_index - 1] == '\n' && cmd_buf_index > 1) {
				sprintf(mess_buff, "\n");
				HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, 1, 1000);
				cmd_state = CMD_CAN_USE;
				cmd_length = cmd_buf_index - 2;
				cmd[cmd_length] = '\0';
				cmd_buf_index = 0;
			}
			HAL_UART_Receive_IT(&huart1, &cmd[cmd_buf_index], 1);
		}
	}
	else if(mode == MODE_WRITE) {
		write_buf_index++;
		if(buff_write[write_buf_index - 1] == END_OF_FILE) {
			sprintf(mess_buff, "\r\n");
			HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, 1, 1000);
			write_buf_index = 0;
			mode = MODE_CMD;
		}
		HAL_UART_Receive_IT(&huart1, &buff_write[write_buf_index], 1);
	}
}

void clearCMDBuffer() {
	memset(cmd, 0, CMD_SIZE);
}

void clearWriteBuffer() {
	memset(buff_write, 0, BUF_RW_SIZE);
}

void clearReadBuffer() {
	memset(buff_read, 0, BUF_RW_SIZE);
}

void mount_sd() {
	fresult = f_mount(&fatfs, "/", 1);
	if (fresult != FR_OK) {
		sprintf(mess_buff, "error in mounting SD CARD...\r\n");
		HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
	}
	else {
		sprintf(mess_buff, "SD CARD mounted successfully...\r\n");
		HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
	}
}

FRESULT getPath() {
	/* Get the directory */
	fresult = f_getcwd(path, PATH_SIZE);

	return fresult;
}

void returnParentsDir(char source[]) {
	uint8_t path_length = strlen(source);

	for(uint8_t i = path_length - 1; i > 0; i--) {
		if(level == 1) {
			source[1] = '\0';
			level_state = LEVEL_DECREASE;
			return;
		}
		if(source[i] == '/') {
			source[i] = '\0';
			level_state = LEVEL_DECREASE;
			return;
		}
	}
	source[1] = '\0';
	level_state = LEVEL_NOT_CHANGE;
}

FRESULT listFileDir(char* path) {
	fresult = f_opendir(&dir, path);                       		/* Open the directory */
	if (fresult == FR_OK) {
		for (;;) {
			fresult = f_readdir(&dir, &fno);                   	/* Read a directory item */
			if (fresult != FR_OK || fno.fname[0] == 0) break;  	/* Error or end of dir */
			if (fno.fattrib & AM_DIR) {            				/* Directory */
				sprintf(mess_buff, "   %-20s%-20s\r\n", "<DIR>", fno.fname);
				HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
			} else {                               				/* File */
				sprintf(mess_buff, "   %-8s%8lu    %-20s\r\n", "<FIL>", fno.fsize, fno.fname);
				HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
			}
		}
		sprintf(mess_buff, "\r\n");
		HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
		f_closedir(&dir);
	} else {
		sprintf(mess_buff, "Failed to open \"%s\". (%u)\r\n", path, fresult);
		HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
	}
	return fresult;
}
/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{
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

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_SDIO_SD_Init();
  MX_FATFS_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
//  Ringbuf_init();  // init the ring buffer
  mount_sd();
  HAL_UART_Receive_IT(&huart1, &cmd[cmd_buf_index], 1);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if(cmd_line == CMD_LINE_SHOW) {
		  fresult = getPath();
		  if(fresult != FR_OK) {
			  sprintf(mess_buff, "There is a error when get the path!\r\n");
			  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
			  cmd_line = CMD_LINE_HIDE;
		  }
		  else {
			  sprintf(mess_buff, "SD: %s> ", path);
			  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
			  cmd_line = CMD_LINE_HIDE;
		  }
	  }

	  switch(cmd_state) {
	  case CMD_CAN_USE:
		  //_______________________list all files and directories_______________________
		  if(cmd_length > 0 && !strcmp((const char *)cmd, "ls")) {
			  fresult = getPath();
			  if(fresult == FR_OK) {
				  sprintf(mess_buff, "\tDirectory: %s\r\n\n", path);
			  	  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
				  listFileDir(path);
			  }
		  }
		  //_______________________change to another directory_______________________
		  else if(cmd_length > 0 && !strncmp((const char *)cmd, "cd", 2) && cmd[2] == ' ') {
			  getPath();

			  char tmp[CMD_SIZE];
			  if(level >= 1) {
				  memcpy(&tmp[1], &cmd[3], cmd_length - 3);
				  tmp[0] = '/';
				  tmp[cmd_length - 2] = '\0';

				  if(strcmp("/..", tmp) == 0) {
					  returnParentsDir(path);
				  }
				  else {
					  level_state = LEVEL_INCREASE;
					  strcat(path, tmp);
				  }
			  }
			  else {
				  memcpy(tmp, &cmd[3], cmd_length - 3);
				  tmp[cmd_length - 3] = '\0';
				  if(strcmp("..", tmp) == 0) {
					  returnParentsDir(path);
				  }
				  else {
					  level_state = LEVEL_INCREASE;
					  strcat(path, tmp);
				  }
			  }

			  fresult = f_chdir(path);
			  if(fresult == FR_OK) {
				  if(level_state == LEVEL_DECREASE) {
					  level--;
				  }
				  else if(level_state == LEVEL_INCREASE) {
					  level++;
				  }
			  }
		  }
		  //_______________________print all content of a file_______________________
		  else if(cmd_length > 0 && !strncmp((const char *)cmd, "cat", 3) && cmd[3] == ' ') {
			  char tmp[CMD_SIZE];

			  memcpy(tmp, &cmd[4], cmd_length - 4);
			  tmp[cmd_length - 4] = '\0';

			  fresult = f_open(&myfile, tmp, FA_OPEN_EXISTING | FA_READ);

			  if(fresult != FR_OK) {
				  sprintf(mess_buff, "Cannot open the file\r\n");
				  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
				  clearCMDBuffer();
				  break;
			  }

			  fresult = f_read(&myfile, buff_read, f_size(&myfile), &byte_read);

			  if(fresult != FR_OK) {
				  sprintf(mess_buff, "Cannot read the file\r\n");
				  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
				  clearCMDBuffer();
				  break;
			  }

			  f_close(&myfile);
			  HAL_UART_Transmit(&huart1, (const uint8_t *)buff_read, strlen((const char *)buff_read), 1000);
			  clearReadBuffer();

			  sprintf(mess_buff, "\r\n");
			  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
		  }
		  //_______________________create a file_______________________
		  else if(cmd_length > 0 && !strncmp((const char *)cmd, "touch", 5) && cmd[5] == ' ') {
			  char tmp[CMD_SIZE];

			  memcpy(tmp, &cmd[6], cmd_length - 6);
			  tmp[cmd_length - 6] = '\0';

			  fresult = f_open(&myfile, tmp, FA_CREATE_NEW);

			  if(fresult != FR_OK) {
				  sprintf(mess_buff, "Cannot create the file\r\n");
				  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
				  clearCMDBuffer();
				  break;
			  }

			  f_close(&myfile);
		  }
		  //_______________________write into a file_______________________
		  else if(cmd_length > 0 && !strncmp((const char *)cmd, "vi", 2) && cmd[2] == ' ') {
			  char tmp[CMD_SIZE];

			  memcpy(tmp, &cmd[3], cmd_length - 3);
			  tmp[cmd_length - 3] = '\0';

			  fresult = f_open(&myfile, tmp, FA_OPEN_EXISTING | FA_WRITE);

			  if(fresult != FR_OK) {
				  sprintf(mess_buff, "Cannot open the file\r\n");
				  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
				  clearCMDBuffer();
				  break;
			  }

			  f_truncate(&myfile);

			  mode = MODE_WRITE;
			  huart1.RxState = HAL_UART_STATE_READY;
			  HAL_UART_Receive_IT(&huart1, &buff_write[write_buf_index], 1);

			  while(buff_write[strlen((const char *)buff_write) - 1] != END_OF_FILE);	//until enter the EOF

			  sprintf(mess_buff, "\r\n");
			  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);

			  huart1.RxState = HAL_UART_STATE_READY;
			  HAL_UART_Receive_IT(&huart1, &cmd[cmd_buf_index], 1);

			  fresult = f_write(&myfile, buff_write, strlen((const char *)buff_write), &byte_read);
			  clearWriteBuffer();

			  if(fresult != FR_OK) {
				  sprintf(mess_buff, "Cannot write the file\r\n");
				  HAL_UART_Transmit(&huart1, (const uint8_t *)mess_buff, strlen(mess_buff), 1000);
				  clearCMDBuffer();
				  break;
			  }

			  f_close(&myfile);
		  }
		  cmd_state = CMD_READY;
		  cmd_line = CMD_LINE_SHOW;
		  clearCMDBuffer();
	  }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 72;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief SDIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_SDIO_SD_Init(void)
{

  /* USER CODE BEGIN SDIO_Init 0 */

  /* USER CODE END SDIO_Init 0 */

  /* USER CODE BEGIN SDIO_Init 1 */

  /* USER CODE END SDIO_Init 1 */
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 6;
  /* USER CODE BEGIN SDIO_Init 2 */

  /* USER CODE END SDIO_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */
  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
/* USER CODE BEGIN MX_GPIO_Init_1 */
/* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(OTG_FS_PowerSwitchOn_GPIO_Port, OTG_FS_PowerSwitchOn_Pin, GPIO_PIN_SET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOD, LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : OTG_FS_PowerSwitchOn_Pin */
  GPIO_InitStruct.Pin = OTG_FS_PowerSwitchOn_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(OTG_FS_PowerSwitchOn_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PC1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLDOWN;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PDM_OUT_Pin */
  GPIO_InitStruct.Pin = PDM_OUT_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(PDM_OUT_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : I2S3_WS_Pin */
  GPIO_InitStruct.Pin = I2S3_WS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF6_SPI3;
  HAL_GPIO_Init(I2S3_WS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SPI1_SCK_Pin SPI1_MISO_Pin SPI1_MOSI_Pin */
  GPIO_InitStruct.Pin = SPI1_SCK_Pin|SPI1_MISO_Pin|SPI1_MOSI_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI1;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : BOOT1_Pin */
  GPIO_InitStruct.Pin = BOOT1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BOOT1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : CLK_IN_Pin */
  GPIO_InitStruct.Pin = CLK_IN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF5_SPI2;
  HAL_GPIO_Init(CLK_IN_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD4_Pin LD3_Pin LD5_Pin LD6_Pin
                           Audio_RST_Pin */
  GPIO_InitStruct.Pin = LD4_Pin|LD3_Pin|LD5_Pin|LD6_Pin
                          |Audio_RST_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

  /*Configure GPIO pin : VBUS_FS_Pin */
  GPIO_InitStruct.Pin = VBUS_FS_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(VBUS_FS_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : OTG_FS_ID_Pin OTG_FS_DM_Pin OTG_FS_DP_Pin */
  GPIO_InitStruct.Pin = OTG_FS_ID_Pin|OTG_FS_DM_Pin|OTG_FS_DP_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : OTG_FS_OverCurrent_Pin */
  GPIO_InitStruct.Pin = OTG_FS_OverCurrent_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(OTG_FS_OverCurrent_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : MEMS_INT2_Pin */
  GPIO_InitStruct.Pin = MEMS_INT2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_EVT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(MEMS_INT2_GPIO_Port, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
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
