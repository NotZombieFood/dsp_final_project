/**
  ******************************************************************************
  * @file    ADC_DMA/main.c
  * @author  MCD Application Team
  * @version V1.0.1
  * @date    11-November-2013
  * @brief   This example describes how to use the ADC and DMA to transfer 
  *          continuously converted data from ADC to memory.
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT 2013 STMicroelectronics</center></h2>
  *
  * Licensed under MCD-ST Liberty SW License Agreement V2, (the "License");
  * You may not use this file except in compliance with the License.
  * You may obtain a copy of the License at:
  *
  *        http://www.st.com/software_license_agreement_liberty_v2
  *
  * Unless required by applicable law or agreed to in writing, software 
  * distributed under the License is distributed on an "AS IS" BASIS, 
  * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  * See the License for the specific language governing permissions and
  * limitations under the License.
  *
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdlib.h>
#include "arm_math.h"

/** @addtogroup STM32F429I_DISCOVERY_Examples
  * @{
  */

/** @addtogroup ADC_DMA
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/ 
#define BUFFERSIZE	100
#define FILTERORDER	30

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* You can monitor the converted value by adding the variable "uhADC3ConvertedValue" 
   to the debugger watch window */
__IO float entryBuffer[BUFFERSIZE]; //Buffer de muestras
__IO float outputBuffer[BUFFERSIZE]; //Buffer de muestras
__IO float filterBuffer[FILTERORDER]; //Buffer de muestras

uint32_t sum_filter = 0;

float f;
unsigned char b[] = {'a','b','c','d'};
int byte_count = 0;

void get_float(){
	memcpy(&f, &b, sizeof(f));
}

Point 	pointBuffer[BUFFERSIZE];	 //Buffer de puntos para gráficar
pPoint  pPoints=(pPoint)pointBuffer;

__IO uint8_t intFlag=0, cpuDone=1, overrunFlag = 0, bufferInstance = 0, uart_receive = 0, buffer_i = 0, buffer_full = 0;
DAC_InitTypeDef  DAC_InitStructure;

float estados_fir[30];

float coef[FILTERORDER] = {
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
		9,
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
		9,
		1,
		2,
		3,
		4,
		5,
		6,
		7,
		8,
		9
};


struct Node * head = NULL;
struct Node * tail = NULL;


/* Structure for FIFO */
struct Node{
	int  data;
    struct Node *next;
    struct Node *prev;
};

/* Get the FIFO filled with 0s */
void fill(){
	for(int index = 0; index <= FILTERORDER; index++){
		struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
		new_node-> data = 0;
		if (head == NULL){
			head = new_node;
			new_node->prev = NULL;
		}else{
        	struct Node *iter;
        	iter = head;
        	while (iter->next != NULL){
        		iter = iter->next;
        	}
        	iter->next = new_node;
        	new_node->prev = iter;
        	tail = new_node;
		}
	}
}


/* Push a new value and pop the older one */
void push(int data){
	struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
	/* push */
	new_node->next = head;
	new_node->data = data;
	head->prev = new_node;
	head = new_node;
	/* pop */
    struct Node *temp;
	temp = tail->prev;
	tail = temp;
	tail-> next = NULL;
}

/* this is just for debug */
void printArray(){
    struct Node *iter;
    iter = head;
    while(iter->next != NULL){
		printf("%d",iter->data);
		iter=iter->next;
	}
	printf("\r\n");
}


static void delay(__IO uint32_t nCount)
{
  __IO uint32_t index = 0;
  for(index = nCount; index != 0; index--)
  {
  }
}


/**
  * @brief  get the sum at the end of filter
  * @param  None
  * @retval None
  */

void getFilter(void){
	  sum_filter = 0; // sum_filter is 32bits
	  int coef_i = 0;
	  struct Node *iter;
	  iter = head;
	  while(iter->next != NULL){
		  sum_filter += iter->data * coef[coef_i];
		  iter=iter->next;
		  coef_i++;
	  }
}



/**
  * @brief  Main program
  * @param  None
  * @retval None
  */

int main(void)
{
  fill(); // Create the filter fifo
  /* ADC3 configuration *******************************************************/
  /*  - Enable peripheral clocks                                              */
  /*  - DMA2_Stream0 channel2 configuration                                   */
  /*  - Configure ADC Channel13 pin as analog input                           */
  /*  - Configure ADC3 Channel13                                              */
  USART_init();


	while (1)
	  {
	  while(buffer_full==0); //Esperar interrupcion del DMA

	  for(int sample=0; sample<BUFFERSIZE;sample++){
		  push(entryBuffer[sample]);
		  getFilter();
		  outputBuffer[sample] = sum_filter;
	  }
	  buffer_full = 0;

	  }
}

void USART_init(void){
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStruct;
	NVIC_InitTypeDef   NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA, ENABLE);


	/* Connect PXx to USARTx_Tx*/
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource9, GPIO_AF_USART1);

	/* Connect PXx to USARTx_Rx*/
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource10, GPIO_AF_USART1);

	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_9;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	/* Configure USART Rx as alternate function  */
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
	GPIO_Init(GPIOA, &GPIO_InitStructure);

	USART_InitStruct.USART_BaudRate = 9600;
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStruct.USART_Parity = USART_Parity_No;
	USART_InitStruct.USART_StopBits = USART_StopBits_1;
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStruct);

	USART_ITConfig(USART1, USART_IT_RXNE, 	ENABLE); // rx interrupt

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);

	USART_Cmd(USART1, ENABLE);
}


void USART1_IRQHandler(void)
{
	if (USART1->SR && 1<<5){
		uart_receive = USART_ReceiveData(USART1);
		if (buffer_i < BUFFERSIZE && !buffer_full){
			if (byte_count <3){
				b[byte_count] = uart_receive;
				byte_count++;
			}else{
				b[byte_count] = uart_receive;
				byte_count = 0;
				get_float();
				entryBuffer[buffer_i] = f;
			}
			buffer_i++;
		} else{
			buffer_full = 1;
			buffer_i = 0;
		}
	}
}



/*
 * Docs
 * Send uart by using USART_SendData(USART1,DATA);
 *
 */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
