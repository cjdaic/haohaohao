#include <FreeRTOS.h>
#include <task.h>
#include <xil_printf.h>
#include "common.h"
#include "TskData/TskData.h"
#include "TskNetwork/TskNetwork.h"

TaskHandle_t tskMainHandle;

void tskMain(void * arg)
{
	vTaskDelay(200);
    tskDataInit();
    tskNetworkInit();

	vTaskDelete(NULL);
}

int main()
{
    initPeriph();

	xTaskCreate(tskMain, "TskMain", configMINIMAL_STACK_SIZE, NULL, 16, &tskMainHandle);
	vTaskStartScheduler();
	while(1);

	return 0;
}
