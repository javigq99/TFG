#ifdef __cplusplus
 extern "C" {
#endif

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include "queue.h"
#include "main.h"
#include "ai_platform.h"
#include "network.h"
#include "network_data.h"
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include "ai_datatypes_defines.h"
#include "nnTasks.h"


#define WRITE_ADDR 0xD4
#define READ_ADDR 0xD5
#define ACEL_CONTROL_REG 0x10
#define GIR_CONTROL_REG 0x11
#define REG_ACEL 0x28
#define REG_GIR 0x22
#define REG_STATUS 0x1E
#define ACEL_SENSIVITY 0.061f
#define GIR_SENSIVITY  0.07f
#define SENSORS_DATA_RATE 26
#define MEASUREMENT_RANGE 2000.0f
#define SENSORS_DATA_RATE 26

extern UART_HandleTypeDef huart1;
extern I2C_HandleTypeDef hi2c2;

float acelBuff[3];
float girBuff[3];
const char * movements [AI_NETWORK_OUT_1_SIZE] = {"parado","andar","correr","bici"};

void acquireDataTask(void * param);
void processDataTask(void * param);
void postProcessTask(void * param);
void initSensorsAndNN();
void initTask(void * param);
void ReadI2C_IT(uint8_t *buffer,uint16_t addr, uint8_t len);
void readSensorsIT();
uint8_t initSensors();

xSemaphoreHandle inputSemHandle;
xSemaphoreHandle outputSemHandle;
xSemaphoreHandle uartSemaphoreHandle;
xSemaphoreHandle I2CBusySem;
xSemaphoreHandle I2CBufferReadySem;

xTaskHandle initHandle;
xTaskHandle acquireDataHandle;
xTaskHandle processDataHandle;
xTaskHandle postProcessHandle;

TickType_t start,end,elapsed;

uint8_t * in_data = NULL;
uint8_t * out_data = NULL;

static ai_handle network = AI_HANDLE_NULL;
static ai_network_report network_info;

/* Global c-array to handle the activations buffer */
AI_ALIGNED(4)
static ai_u8 activations[AI_NETWORK_DATA_ACTIVATIONS_SIZE];

/*  In the case where "--allocate-inputs" option is used, memory buffer can be
 *  used from the activations buffer. This is not mandatory.
 */
#if !defined(AI_NETWORK_INPUTS_IN_ACTIVATIONS)
/* Allocate data payload for input tensor */
AI_ALIGNED(4)
static ai_u8 in_data_s[AI_NETWORK_IN_1_SIZE_BYTES];
#endif

/*  In the case where "--allocate-outputs" option is used, memory buffer can be
 *  used from the activations buffer. This is no mandatory.
 */
#if !defined(AI_NETWORK_OUTPUTS_IN_ACTIVATIONS)
/* Allocate data payload for the output tensor */
AI_ALIGNED(4)
static ai_u8 out_data_s[AI_NETWORK_OUT_1_SIZE_BYTES];
#endif

void createNnObjects(){

	uartSemaphoreHandle = xSemaphoreCreateBinary();
	inputSemHandle = xSemaphoreCreateBinary();
	outputSemHandle = xSemaphoreCreateBinary();
	I2CBusySem = xSemaphoreCreateBinary();
	I2CBufferReadySem = xSemaphoreCreateBinary();

	xSemaphoreGive(uartSemaphoreHandle); // UART printf
	xSemaphoreGive(I2CBusySem);
	xSemaphoreTake(inputSemHandle,0); //Buffer de entrada
	xSemaphoreTake(outputSemHandle,0); //Buffer de salida
	xSemaphoreTake(I2CBufferReadySem,0);

}

void initSensorsAndNN(){

	xTaskCreate(initTask,"initTask",512,NULL,1,&initHandle);
}

void initTask(void * param){

	uint8_t ret;

	ret = initSensors();

	if (ret == 0){

		printf("Acelerómetro y Giroscopio configurados a una frecuencia de %d Hz en un rango de medida: +- %.2f mg,dps \r\n\n",SENSORS_DATA_RATE,MEASUREMENT_RANGE);
		printf("Sensibilidad de los sensores: %.3f ,%.6f \r\n\n",ACEL_SENSIVITY,GIR_SENSIVITY);

		initNN();

	}else{

		printf("Ha ocurrido un error iniciando los sensores\r\n");
		NVIC_SystemReset();
	}

	createNnTasks();

	vTaskDelete(initHandle);

}
void createNnTasks()
{

	xTaskCreate(processDataTask, "processDataTask",256, NULL,1, &processDataHandle);

	xTaskCreate(postProcessTask, "postProcessTask",256, NULL,1, &postProcessHandle);

	xTaskCreate(acquireDataTask, "acquireDataTask",256, NULL,1, &acquireDataHandle);


}


static void ai_log_err(const ai_error err, const char *fct)
{
  /* USER CODE BEGIN log */
  if (fct)
    printf("TEMPLATE - Error (%s) - type=0x%02x code=0x%02x\r\n", fct,
        err.type, err.code);
  else
    printf("TEMPLATE - Error - type=0x%02x code=0x%02x\r\n", err.type, err.code);

  do {} while (1);
  /* USER CODE END log */
}

static int ai_boostrap(ai_handle w_addr, ai_handle act_addr)
{
  ai_error err;

  /* 1 - Create an instance of the model */
  err = ai_network_create(&network, AI_NETWORK_DATA_CONFIG);
  if (err.type != AI_ERROR_NONE) {
    ai_log_err(err, "ai_network_create");
    return -1;
  }

  /* 2 - Initialize the instance */
  const ai_network_params params = AI_NETWORK_PARAMS_INIT(
      AI_NETWORK_DATA_WEIGHTS(w_addr),
      AI_NETWORK_DATA_ACTIVATIONS(act_addr) );

  if (!ai_network_init(network, &params)) {
      err = ai_network_get_error(network);
      ai_log_err(err, "ai_network_init");
      return -1;
    }

  /* 3 - Retrieve the network info of the created instance */
  if (!ai_network_get_info(network, &network_info)) {
    err = ai_network_get_error(network);
    ai_log_err(err, "ai_network_get_error");
    ai_network_destroy(network);
    network = AI_HANDLE_NULL;
    return -3;
  }

  return 0;
}

static int ai_run(void *data_in, void *data_out)
{
  ai_i32 batch;

  ai_buffer *ai_input = network_info.inputs;
  ai_buffer *ai_output = network_info.outputs;

  ai_input[0].data = AI_HANDLE_PTR(data_in);
  ai_output[0].data = AI_HANDLE_PTR(data_out);

  batch = ai_network_run(network, ai_input, ai_output);
  if (batch != 1) {
    ai_log_err(ai_network_get_error(network),
        "ai_network_run");
    return -1;
  }

  return 0;
}

void initNN()
{

	ai_boostrap(ai_network_data_weights_get(), activations);

	if (network) {

		if ((network_info.n_inputs != 1) || (network_info.n_outputs != 1)) {
  			ai_error err = {AI_ERROR_INVALID_PARAM, AI_ERROR_CODE_OUT_OF_RANGE};
  			ai_log_err(err, "template code should be updated\r\n to support a model with multiple IO");
  			NVIC_SystemReset();

		}
	}

	/* 1 - Set the I/O data buffer */

	#if AI_NETWORK_INPUTS_IN_ACTIVATIONS
		in_data = network_info.inputs[0].data;
	#else
		in_data = in_data_s;
	#endif

	#if AI_NETWORK_OUTPUTS_IN_ACTIVATIONS
		out_data = network_info.outputs[0].data;
	#else
		out_data = out_data_s;
	#endif

	if ((!in_data) || (!out_data)) {
		printf("TEMPLATE - I/O buffers are invalid\r\n");
		NVIC_SystemReset(); //Reset al dispositivo
	}


	printf("\r\nRed neuronal inicializada\r\n\n");


}


void acquireDataTask(void * param)
{

	uint8_t i;

	for(;;){
		for(i = 0; i < AI_NETWORK_IN_1_SIZE; i += 6){
			readSensorsIT();
			((ai_float *)in_data)[i] = (ai_float)(acelBuff[0]/MEASUREMENT_RANGE); //2000
			((ai_float *)in_data)[i+1] = (ai_float)(acelBuff[1]/MEASUREMENT_RANGE);
			((ai_float *)in_data)[i+2] = (ai_float)(acelBuff[2]/MEASUREMENT_RANGE);
			((ai_float *)in_data)[i+3] = (ai_float)(girBuff[0]/MEASUREMENT_RANGE);
			((ai_float *)in_data)[i+4] = (ai_float)(girBuff[1]/MEASUREMENT_RANGE);
			((ai_float *)in_data)[i+5] = (ai_float)(girBuff[2]/MEASUREMENT_RANGE);
		}

		xSemaphoreGive(inputSemHandle);

	}

}

void processDataTask(void * param)
{

	int res;

	for(;;){

		xSemaphoreTake(inputSemHandle,portMAX_DELAY);

		res = ai_run(in_data,out_data); //Inferencia


		if (res) {

			ai_error err = {AI_ERROR_INVALID_STATE, AI_ERROR_CODE_NETWORK};
		    ai_log_err(err, "Process has FAILED");
		    NVIC_SystemReset();

		}else{

			xSemaphoreGive(outputSemHandle);

		}

	}
}
void postProcessTask(void * param)
{
	float salida;
	uint8_t maxIndex;
	uint8_t i;

	for(;;){

		xSemaphoreTake(outputSemHandle,portMAX_DELAY);
		maxIndex = 0;
		for(i = 1; i < AI_NETWORK_OUT_1_SIZE; i++){
				if(((float*)out_data)[i] > ((float*)out_data)[maxIndex]){
					maxIndex = i;
				}
		}

		salida = ((float*)out_data)[maxIndex];
		elapsed = end - start;
		printf("Precisión: %f \r\n",salida);
		printf("Movimiento: %s \r\n",movements[maxIndex]);
		printf("Tiempo de inferencia: %lu ticks (ms) \r\n",elapsed);
		printf("======================== \r\n");

	}
}

void HAL_I2C_MemRxCpltCallback( I2C_HandleTypeDef *hi2c)
{

    BaseType_t hasWaken;

    xSemaphoreGiveFromISR(I2CBusySem,&hasWaken);

    xSemaphoreGiveFromISR(I2CBufferReadySem,&hasWaken);

    portYIELD_FROM_ISR(hasWaken);


}

void ReadI2C_IT(uint8_t *buffer,uint16_t addr, uint8_t len)
{

	xSemaphoreTake(I2CBusySem, portMAX_DELAY);

	HAL_I2C_Mem_Read_IT(&hi2c2, 0xD5, addr,I2C_MEMADD_SIZE_8BIT, buffer, len);

	xSemaphoreTake(I2CBufferReadySem, portMAX_DELAY);

	xSemaphoreTake(I2CBufferReadySem, 0);

}

uint8_t initSensors()
{

	uint8_t acelConfig [1] = {0x20}; //contenido del registro con la configuracion adecuada
	uint8_t girConfig [1] = {0x2C}; // 26Hz 2000mg, 2000dps
	uint8_t ret;

	ret = HAL_I2C_Mem_Write(&hi2c2, WRITE_ADDR, GIR_CONTROL_REG, I2C_MEMADD_SIZE_8BIT, girConfig, 1, 100);

	ret |= HAL_I2C_Mem_Write(&hi2c2, WRITE_ADDR, ACEL_CONTROL_REG, I2C_MEMADD_SIZE_8BIT, acelConfig, 1, 100);

	printf("Acelerómetro y Giroscopio configurados en un rango de medida: +- %.2f mg,dps \r\n",MEASUREMENT_RANGE);
	printf("Sensibilidad de los sensores: %.3f ,%.6f \r\n\n",ACEL_SENSIVITY,GIR_SENSIVITY);

	return ret;

}

void readSensorsIT()
{

	uint8_t i2cBuffer[12];
	uint8_t i;

	ReadI2C_IT(i2cBuffer, REG_STATUS, 1);

	while((i2cBuffer[0] & 0x03) != 0x03){ // Polling a los bits que indican cuando hay datos nuevos

		ReadI2C_IT(i2cBuffer, REG_STATUS, 1);

	}

	ReadI2C_IT(i2cBuffer, REG_GIR, 12); // 0x22 reg acel y 0x28, al estar consecutivos podemos
									    // leer los 2 sensores de una sola transferencia
	for(i = 0; i < 3; i++){

		girBuff[i] = (float) ((int16_t) (i2cBuffer[2*i+1] << 8) | i2cBuffer[2*i]) * GIR_SENSIVITY;

		acelBuff[i] = (float) ((int16_t) (i2cBuffer[(2*i+1)+6] << 8) | i2cBuffer[(2*i)+6]) * ACEL_SENSIVITY;

		//0.14  0.29  0.43  0.58
	}

}


#ifdef __cplusplus
}
#endif
