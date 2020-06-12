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

#define N 16 //number of bits



int num_to_q(float n){
    if(n >= 1){
        n = (float)(pow(2, N-1)-1)/(float)(pow(2, N-1));
    }
    if(n < -1){
        n = -1;
    }
    signed int x = round(n*pow(2, N-1));
    printf("\n %d", x);
    if(x < 0){
        unsigned int y = - x + pow(2, N-1) - 1;
        return y;
    }else{
        return x;
    }
}

float q_to_num(int n){
    float x;
    if(n - (pow(2, N-1) - 1) > 0){
        x = -((float)n/(pow(2, N-1)) + 1/(pow(2, N-1)) - 1);
    } else{
        x = (float)n/(pow(2, N-1));
    }
    return x;
}



/** @addtogroup STM32F429I_DISCOVERY_Examples
  * @{
  */

/** @addtogroup ADC_DMA
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/ 
#define BUFFERSIZE	800
#define BUFFERSIZE_16 200
#define FILTERORDER	256

/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* You can monitor the converted value by adding the variable "uhADC3ConvertedValue" 
   to the debugger watch window */
__IO uint8_t entryBuffer[BUFFERSIZE]; //Buffer de entrada
__IO uint8_t outputBuffer[BUFFERSIZE/2]; //Buffer de salida
__IO uint8_t filterBuffer[FILTERORDER]; //Buffer de filtro
float32_t testBuffer[BUFFERSIZE/2]; //Buffer de salida

/*
 *  Filter implementation
 */

arm_fir_instance_f32 fir_struct_right_channel = {0};
arm_fir_instance_f32 fir_struct_left_channel = {0};

float32_t fir_sts_right_channel [BUFFERSIZE_16 + FILTERORDER - 1];
float32_t fir_sts_left_channel [BUFFERSIZE_16 + FILTERORDER - 1];


#define HALF 0x1
#define FULL 0x2
#define EMPTY 0x0

float f;
unsigned char b[] = {'a','b','c','d'};
int byte_count = 0;

void get_float(){
	memcpy(&f, &b, sizeof(f));
}

__IO uint8_t uart_receive = 0, buffer_i = 0, buffer_status = 0;
DAC_InitTypeDef  DAC_InitStructure;

float estados_fir[30];

float coef[FILTERORDER] = {
};

struct Node * head_left = NULL;
struct Node * tail_left = NULL;

struct Node * head_right = NULL;
struct Node * tail_right = NULL;

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
		if (head_left == NULL){
			head_left = new_node;
			new_node->prev = NULL;
		}else{
        	struct Node *iter;
        	iter = head_left;
        	while (iter->next != NULL){
        		iter = iter->next;
        	}
        	iter->next = new_node;
        	new_node->prev = iter;
        	tail_left = new_node;
		}
	}
	for(int index = 0; index <= FILTERORDER; index++){
			struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
			new_node-> data = 0;
			if (head_right == NULL){
				head_right = new_node;
				new_node->prev = NULL;
			}else{
	        	struct Node *iter;
	        	iter = head_right;
	        	while (iter->next != NULL){
	        		iter = iter->next;
	        	}
	        	iter->next = new_node;
	        	new_node->prev = iter;
	        	tail_right = new_node;
			}
	}
}


/* Push a new value and pop the older one */
void push_right(int data){
	struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
	/* push */
	new_node->next = head_right;
	new_node->data = data;
	head_right->prev = new_node;
	head_right = new_node;
	/* pop */
    struct Node *temp;
	temp = tail_right->prev;
	tail_right = temp;
	tail_right-> next = NULL;
}

void push_left(int data){
	struct Node* new_node = (struct Node*) malloc(sizeof(struct Node));
	/* push */
	new_node->next = head_left;
	new_node->data = data;
	head_left->prev = new_node;
	head_left = new_node;
	/* pop */
    struct Node *temp;
	temp = tail_left->prev;
	tail_left = temp;
	tail_left-> next = NULL;
}

int sum_filter_right = 0;
int sum_filter_left = 0;

void getFilterRight(void){
	sum_filter_right = 0; // sum_filter is 32bits
	  int coef_i = 0;
	  struct Node *iter;
	  iter = head_right;
	  while(iter->next != NULL){
		  sum_filter_right += iter->data * coef[coef_i];
		  iter=iter->next;
		  coef_i++;
	  }
}

void getFilterLeft(void){
	sum_filter_left = 0; // sum_filter is 32bits
	  int coef_i = 0;
	  struct Node *iter;
	  iter = head_left;
	  while(iter->next != NULL){
		  sum_filter_left += iter->data * coef[coef_i];
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
	//fill(); // Create the filter fifo                                          */
	//arm_fir_init_f32(&fir_struct_right_channel, FILTERORDER, (float32_t *)&testBuffer[0], &fir_sts_right_channel[0], BUFFERSIZE_16);
	//arm_fir_init_f32(&fir_struct_left_channel, FILTERORDER, (float32_t *)&testBuffer[BUFFERSIZE_16], &fir_sts_left_channel[0], BUFFERSIZE_16);
	system_init();
	buffer_status = EMPTY;

	while (1)
	  {
	  while(buffer_status==0); //Esperar interrupcion del DMA
	  if (buffer_status==HALF){
		  for(int sample=0; sample<BUFFERSIZE/2;sample++){
			  outputBuffer[sample] = entryBuffer[sample];
		  }
	  }else if (buffer_status == FULL){
		  for(int sample=0; sample<BUFFERSIZE/2;sample++){
			  outputBuffer[sample] = entryBuffer[(BUFFERSIZE/2) + sample];
		  }
	  }
	  dumpData();
	  buffer_status = 0;

	  }
}

void system_init(void){
    /* InitStructures */
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStruct;
	//NVIC_InitTypeDef   NVIC_InitStructure;
	NVIC_InitTypeDef   NVIC_InitStructure_DMA;
	NVIC_InitTypeDef   NVIC_InitStructure_DMA_RX;
	DMA_InitTypeDef       DMA_InitStructureRX;
	DMA_InitTypeDef       DMA_InitStructure;

    /* Enable clocks */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_DMA2, ENABLE);

    /*
        Refer to page 308 in https://www.st.com/resource/en/reference_manual/dm00031020-stm32f405-415-stm32f407-417-stm32f427-437-and-stm32f429-439-advanced-arm-based-32-bit-mcus-stmicroelectronics.pdf
    */
	/* DMA2 Stream7 channel4 configuration Tx from memory to uart
      send the output buffer to the uartTX
      Memory address increments, uart address is keep the same
      It will send bytes, but i can be change by changing DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte
      Same for entry
    */
	DMA_InitStructure.DMA_Channel = DMA_Channel_4;
	DMA_InitStructure.DMA_PeripheralBaseAddr = (uint32_t)(&USART1->DR);
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)outputBuffer;
	DMA_InitStructure.DMA_DIR = DMA_DIR_MemoryToPeripheral;
	DMA_InitStructure.DMA_BufferSize = BUFFERSIZE/2;
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream7, &DMA_InitStructure);

	/* DMA2 Stream2 channel4 configuration RX from uart to memory */
	DMA_InitStructureRX.DMA_Channel = DMA_Channel_4;
	DMA_InitStructureRX.DMA_PeripheralBaseAddr = (uint32_t)(&USART1->DR);
	DMA_InitStructureRX.DMA_Memory0BaseAddr = (uint32_t)entryBuffer;
	DMA_InitStructureRX.DMA_DIR = DMA_DIR_PeripheralToMemory;
	DMA_InitStructureRX.DMA_BufferSize = BUFFERSIZE;
	DMA_InitStructureRX.DMA_PeripheralInc = DMA_PeripheralInc_Disable;
	DMA_InitStructureRX.DMA_MemoryInc = DMA_MemoryInc_Enable;
	DMA_InitStructureRX.DMA_PeripheralDataSize = DMA_PeripheralDataSize_Byte;
	DMA_InitStructureRX.DMA_Mode = DMA_Mode_Circular;
	DMA_InitStructureRX.DMA_MemoryDataSize = DMA_MemoryDataSize_Byte;
	DMA_InitStructureRX.DMA_Priority = DMA_Priority_High;
	DMA_InitStructureRX.DMA_FIFOMode = DMA_FIFOMode_Disable;
	DMA_InitStructureRX.DMA_FIFOThreshold = DMA_FIFOThreshold_Full;
	DMA_InitStructureRX.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructureRX.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;
	DMA_Init(DMA2_Stream2, &DMA_InitStructureRX);


    /* Enable full transfer for TX and RX, and half transfer for RX */
	DMA_ITConfig(DMA2_Stream2, DMA_IT_TC | DMA_IT_HT, ENABLE);
	DMA_ITConfig(DMA2_Stream7, DMA_IT_TC , ENABLE);


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

	USART_InitStruct.USART_BaudRate = 921600;
	USART_InitStruct.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	USART_InitStruct.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
	USART_InitStruct.USART_Parity = USART_Parity_No;
	USART_InitStruct.USART_StopBits = USART_StopBits_1;
	USART_InitStruct.USART_WordLength = USART_WordLength_8b;
	USART_OverSampling8Cmd(USART1, ENABLE);
	USART_Init(USART1, &USART_InitStruct);

    /* We dont need the USART Interrup while using DMA for RX

	USART_ITConfig(USART1, USART_IT_RXNE, 	ENABLE); // rx interrupt

	NVIC_InitStructure.NVIC_IRQChannel = USART1_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_Init(&NVIC_InitStructure);

    */
	NVIC_InitStructure_DMA.NVIC_IRQChannel = DMA2_Stream7_IRQn;
	NVIC_InitStructure_DMA.NVIC_IRQChannelPreemptionPriority = 0x01;
	NVIC_InitStructure_DMA.NVIC_IRQChannelSubPriority = 0x01;
	NVIC_InitStructure_DMA.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure_DMA);

	NVIC_InitStructure_DMA_RX.NVIC_IRQChannel = DMA2_Stream2_IRQn;
	NVIC_InitStructure_DMA_RX.NVIC_IRQChannelPreemptionPriority = 0x02;
	NVIC_InitStructure_DMA_RX.NVIC_IRQChannelSubPriority = 0x02;
	NVIC_InitStructure_DMA_RX.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure_DMA_RX);

	USART_DMACmd(USART1, USART_DMAReq_Rx, ENABLE);
	USART_Cmd(USART1, ENABLE);
	DMA_Cmd(DMA2_Stream7, DISABLE);
	DMA_Cmd(DMA2_Stream2, ENABLE);

}

void dumpData(void){
	USART_DMACmd(USART1, USART_DMAReq_Tx, ENABLE);
	DMA_Cmd(DMA2_Stream7, ENABLE);
	DMA_ITConfig(DMA2_Stream7, DMA_IT_TC , ENABLE);
}

/*
void USART1_IRQHandler(void)
{
	if (USART1->SR && 1<<5){
		uart_receive = USART_ReceiveData(USART1);
		if (buffer_i < BUFFERSIZE && !buffer_status){
			entryBuffer[buffer_i] = uart_receive;
			buffer_i++;
		} else{
			buffer_status = 1;
			buffer_i = 0;
		}
	}
}
*/

void DMA2_Stream7_IRQHandler(void)
{
	USART_DMACmd(USART1, USART_DMAReq_Tx, DISABLE);
	DMA_Cmd(DMA2_Stream7, DISABLE);
	if (DMA2->HISR && 1<<27){
		DMA2->HIFCR = 1<<27;
		DMA2->HIFCR = 0;
		DMA_ITConfig(DMA2_Stream7, DMA_IT_TC , DISABLE);
	}
}

void DMA2_Stream2_IRQHandler(void)
{
	/*
	 * never disable DMA for RX
	 */
	if (DMA2->LISR == 1<<21){ // Full transfer complete, idk why it didnt worked with &&
			DMA2->LIFCR = 1<<21;
			DMA2->LIFCR = 0;
			buffer_status = FULL;
	}
	else if (DMA2->LISR && 1<<20){ // Half transfer complete
		DMA2->LIFCR = 1<<20;
		DMA2->LIFCR = 0;
		buffer_status = HALF;
	}
}

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
