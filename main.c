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
__IO uint16_t entryBuffer[BUFFERSIZE]; //Buffer de muestras
__IO uint16_t outputBuffer[BUFFERSIZE]; //Buffer de muestras
__IO uint16_t filterBuffer[FILTERORDER]; //Buffer de muestras

uint32_t sum_filter = 0;

Point 	pointBuffer[BUFFERSIZE];	 //Buffer de puntos para gráficar
pPoint  pPoints=(pPoint)pointBuffer;

__IO uint8_t intFlag=0, cpuDone=1, overrunFlag = 0, bufferInstance = 0, uart_receive = 0, buffer_i = 0, buffer_full = 0;
DAC_InitTypeDef  DAC_InitStructure;

float estados_fir[30];

float coef[FILTERORDER] = {
		-1.73612192012222373e-17,
		-0.00955767541404129242,
		-0.0092974093256849525,
		5.54109570193356632e-18,
		0.0111337082817587057,
		0.0137385090585045375,
		-6.0208811328752934e-18,
		-0.0300378724281842591,
		-0.0665891363015007898,
		-0.0931977857830110501,
		-0.0938325802020069238,
		-0.060861958046541216,
		1.22790813396389179e-17,
		0.0705052346295413218,
		0.126528797747938904,
		0.147825096653919491,
		0.126528797747938904,
		0.0705052346295413218,
		1.22790813396389179e-17,
		-0.060861958046541216,
		-0.0938325802020069238,
		-0.0931977857830110501,
		-0.0665891363015007898,
		-0.0300378724281842591,
		-6.0208811328752934e-18,
		0.0137385090585045375,
		0.0111337082817587057,
		5.54109570193356632e-18,
		-0.0092974093256849525,
		-0.00955767541404129242,
		-1.73612192012222373e-17
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

static void USART_init(void){
	USART_InitTypeDef USART_InitStruct;
	NVIC_InitTypeDef   NVIC_InitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

	USART_InitStruct.USART_BaudRate = 9600;
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStruct.USART_Parity = USART_Parity_No;
	USART_InitStruct.USART_StopBits = USART_StopBits_1;
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;
	USART_Init(USART1, &USART_InitStruct);
	USART_Cmd(USART1, ENABLE);

	USART_ITConfig(USART1, USART_IT_RXNE, ENABLE); // rx interrupt

	/**
	 * Set Channel to USART1
	 * Set Channel Cmd to enable. That will enable USART1 channel in NVIC
	 * Set Both priorities to 0. This means high priority
	 *
	 * Initialize NVIC
	 */
	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);

}


void USART1_IRQHandler(void)
{
	if ((USART1->SR & USART_FLAG_RXNE) != (u16)RESET){
		uart_receive = USART_ReceiveData(USART1);
		if (buffer_i < BUFFERSIZE && !buffer_full){
			entryBuffer[buffer_i] = uart_receive;
			buffer_i++;
		} else{
			buffer_full = 1;
			buffer_i = 0;
		}
	}
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
