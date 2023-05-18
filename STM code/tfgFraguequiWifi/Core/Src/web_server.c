/*
 * Wifi.c
 *
 *  Created on: Nov 29, 2020
 *      Author: rpaz
 */

#include <unistd.h>
#include "main.h"
#include "es_wifi.h"
#include "wifi.h"
#include "cmsis_os.h"
#include "semphr.h"
#include "network.h"
#include "structNN.h"

extern const char * movements [AI_NETWORK_OUT_1_SIZE];

/* Update SSID and PASSWORD with own Access point settings */
#define SSID     "javi"
#define PASSWORD "javi1999"
#define PORT           3001

#define TERMINAL_USE

#define WIFI_WRITE_TIMEOUT 10000
#define WIFI_READ_TIMEOUT  10000
#define SOCKET                 0


#ifdef  TERMINAL_USE
#define LOG(a) printf a
#else
#define LOG(a)
#endif
/* Private defines -----------------------------------------------------------*/


static  uint8_t resp[16];
static  uint8_t  IP_Addr[4];

extern nnOutput getNNoutput();


int wifi_server(void);

static WIFI_Status_t SendWebPage(nnOutput nnOut);
static int wifi_start(void);
static int wifi_connect(void);
static void WebServerProcess(void);



static int wifi_start(void)
{
  uint8_t  MAC_Addr[6];

 /*Initialize and use WIFI module */
  if(WIFI_Init() ==  WIFI_STATUS_OK)
  {
    LOG(("ES-WIFI Initialized.\n"));
    if(WIFI_GetMAC_Address(MAC_Addr) == WIFI_STATUS_OK)
    {
      LOG(("> es-wifi module MAC Address : %X:%X:%X:%X:%X:%X\n",
               MAC_Addr[0],
               MAC_Addr[1],
               MAC_Addr[2],
               MAC_Addr[3],
               MAC_Addr[4],
               MAC_Addr[5]));
    }
    else
    {
      LOG(("> ERROR : CANNOT get MAC address\n"));
      return -1;
    }
  }
  else
  {
    return -1;
  }
  return 0;
}



int wifi_connect(void)
{

  wifi_start();

  LOG(("\nConnecting to %s , %s\r\n",SSID,PASSWORD));
  if( WIFI_Connect(SSID, PASSWORD, WIFI_ECN_WPA2_PSK) == WIFI_STATUS_OK)
  {
    if(WIFI_GetIP_Address(IP_Addr) == WIFI_STATUS_OK)
    {
      LOG(("> es-wifi module connected: got IP Address : %d.%d.%d.%d\r\n",
               IP_Addr[0],
               IP_Addr[1],
               IP_Addr[2],
               IP_Addr[3]));
    }
    else
    {
		  LOG((" ERROR : es-wifi module CANNOT get IP address\r\n"));
      return -1;
    }
  }
  else
  {
		 LOG(("ERROR : es-wifi module NOT connected\r\n"));
     return -1;
  }
  return 0;
}

int wifi_server(void)
{

  LOG(("\nRunning Server \r\n"));
  if (wifi_connect()!=0) return -1;


  if (WIFI_STATUS_OK!=WIFI_StartServer(SOCKET, WIFI_TCP_PROTOCOL, 1, "", PORT))
  {
    LOG(("ERROR: Cannot start server.\n"));
  }

  LOG(("Server is running and waiting for an HTTP  Client connection to %d.%d.%d.%d\r\n",IP_Addr[0],IP_Addr[1],IP_Addr[2],IP_Addr[3]));

  do
  {
    uint8_t RemoteIP[4];
    uint16_t RemotePort;


    while (WIFI_STATUS_OK != WIFI_WaitServerConnection(SOCKET,1000,RemoteIP,&RemotePort))
    {
        LOG(("Waiting connection to  %d.%d.%d.%d\r\n",IP_Addr[0],IP_Addr[1],IP_Addr[2],IP_Addr[3]));

    }

    LOG(("Client connected %d.%d.%d.%d:%d\r\n",RemoteIP[0],RemoteIP[1],RemoteIP[2],RemoteIP[3],RemotePort));

    WebServerProcess();

    if(WIFI_CloseServerConnection(SOCKET) != WIFI_STATUS_OK)
    {
      LOG(("ERROR: failed to close current Server connection\n"));
      return -1;
    }
  }
  while(1);

}


static void WebServerProcess(void)
{

  bool stop = true;
  while(stop){

	  nnOutput nnOut = getNNoutput(); //Esperamos la cola con los datos de la tarea que procesa la salida de la red

	  if (nnOut.accuracy <= 0.50f){
		  nnOut.movement = 4;
	  }

	  if(SendWebPage(nnOut) != WIFI_STATUS_OK){
	         LOG(("> ERROR : No se pueden enviar los datos\r\n"));
	         stop = false;
	  }
	  else{
	         LOG(("Enviando datos: %d --> %s\r\n",nnOut.movement,movements[nnOut.movement]));
	  }
  }


 }

/**
  * @brief  Send HTML page
  * @param  None
  * @retval None
  */
static WIFI_Status_t SendWebPage(nnOutput data)
{

  uint16_t SentDataLength;
  WIFI_Status_t ret;

  sprintf((char *)resp, (char *)"%d,%.2f",data.movement,data.accuracy);

  ret = WIFI_SendData(0, (uint8_t *)resp, strlen((char *)resp), &SentDataLength, WIFI_WRITE_TIMEOUT);

  if((ret == WIFI_STATUS_OK) && (SentDataLength != strlen((char *)resp)))
  {
    ret = WIFI_STATUS_ERROR;
  }

  return ret;
}


//*************************************************************************//

extern SPI_HandleTypeDef hspi3;

/******************************************************************************/
/*                 STM32L4xx Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file.                                                                     */
/************************************************************
 *															*
 *		Rutinas de servicio de interrupciones				*
 *															*
 ************************************************************/

/**
  * @brief  This function handles external lines 1interrupt request.
  * @param  None
  * @retval None
  */
void EXTI1_IRQHandler(void)
{
 HAL_GPIO_EXTI_IRQHandler(GPIO_PIN_1);
 portYIELD_FROM_ISR(pdFALSE);
}

/**
  * @brief  EXTI line detection callback.
  * @param  GPIO_Pin: Specifies the port pin connected to corresponding EXTI line.
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  switch (GPIO_Pin)
  {
    case (GPIO_PIN_1):
    {
      SPI_WIFI_ISR();
      break;
    }
    default:
    {
      break;
    }
  }
  portYIELD_FROM_ISR(pdFALSE);
}

/**
  * @brief  SPI3 line detection callback.
  * @param  None
  * @retval None
  */
extern  SPI_HandleTypeDef hspi;
void SPI3_IRQHandler(void)
{
  HAL_SPI_IRQHandler(&hspi);
  portYIELD_FROM_ISR(pdFALSE);
}
