#include "contiki.h"
#include "dtn.h"
#include <stdio.h>
#include <string.h>
#include "lib/memb.h"
#if CONTIKI_TARGET_ORISENPRIME
	#include "button-sensors.h"
	#include "dev/leds.h"
#else
	#include "dev/button-sensor.h"
#endif

MEMB(data_stream, struct dtn_msg_data, 1);

#if CONTIKI_TARGET_ORISENPRIME
	#define FLASH_LED(l) {		\
		leds_on(l); 			\
		clock_delay_msec(50); 	\
		leds_off(l); 			\
		clock_delay_msec(50);	\
	}
#endif

//Initialize variables
static struct dtn_msg_data *myData;

/**
 * @brief Set Device Address
 * @details Set Device Address
 */
static void
set_local_address(){
	static rimeaddr_t myAddr;
	rimeaddr_copy(&myAddr, &rimeaddr_null);
	myAddr.u8[0] = 0x08;
	rimeaddr_copy(&rimeaddr_node_addr, &myAddr);
}

/**
 * @brief Destruct Stuff
 * @details Destruct Stuff
 */
static void
destructor(){
	dtn_close();
	memb_free(&data_stream, myData);
}


//Initialize Process information
/*----------------------------------------------*/
PROCESS(bus_transport, "Delayed Bus");
AUTOSTART_PROCESSES(&bus_transport);
/*----------------------------------------------*/
//Process Method
PROCESS_THREAD(bus_transport, ev, data){
	PROCESS_BEGIN();
	PROCESS_EXITHANDLER(destructor());

	static rimeaddr_t addr_ereceviver;
	
	memb_init(&data_stream);

	myData = memb_alloc(&data_stream);
	strcpy(myData->data, "Johann");

//Set Local Address if Orisen Prime
#if CONTIKI_TARGET_ORISENPRIME
		set_local_address();
#endif

	//Intialize protocol
	SENSORS_ACTIVATE(button_sensor);
#if CONTIKI_TARGET_ORISENPRIME
	set_power(0x00);
#endif
	dtn_init();

	while(1){

		PROCESS_WAIT_EVENT_UNTIL(ev   == sensors_event &&
			     data == &button_sensor);

#if CONTIKI_TARGET_ORISENPRIME
		FLASH_LED(LEDS_BLUE);
#endif

		rimeaddr_copy(&addr_ereceviver, &rimeaddr_null);
		addr_ereceviver.u8[0] = 0x03;
		//Create new buffer
		dtn_new_buff(myData, &addr_ereceviver);
	}

	PROCESS_END();
}