/**
 * @file
 *        The library that implements a Delayed Tolerant Network
 * @author
 *        Johann Mifsud <johann.mifsud.13@ucl.ac.uk>
 */

#include "contiki.h"
#include "dtn.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#define PRINT2ADDR(addr) printf("%02x%02x:%02x%02x",(addr)->u8[3], (addr)->u8[2], (addr)->u8[1], (addr)->u8[0])

/**
 * @brief Initializes the packet queue
 * @details Initializes the packet queue that will contain the queue buffer items
 * 
 * @param q reference to where the packet queue should be initialized
 * @param S The MAX the queue can hold
 */
PACKETQUEUE(pkt_q, MAX_QUEUE_PACKETS);

static struct dtn_channels dtn_chan;
static struct dtn_vars dtn_global;

/**
 * @brief  Get the Header structure from the packet queue item passed.
 * @details Get the header structure from the packet queue item. The header is 
 *          situated in data part. Since it is shifted when it is saved to queuebuf.
 * 
 * @param packetqueue_item Queue Packet that requires the header to be fetched.
 * @return Pointer to the queuebuf header area. 
 */
static struct dtn_msg_header
*get_hdr_buff(struct packetqueue_item *pq_item){
	struct dtn_msg_header *r_msg_hdr;
	struct queuebuf *q_buf = packetqueue_queuebuf(pq_item);
	r_msg_hdr = (struct dtn_msg_header*) queuebuf_dataptr(q_buf);
	return r_msg_hdr;
}

/**
 * @brief Check if the header is same. Version and same protocol name.
 * @details Check if the header is same. Version and same protocol name.
 * 
 * @param dtn_msg_header The header to test if correct.
 */
static int
is_spray_wait(struct dtn_msg_header *hdr){
	if(hdr->protocol.version != DTN_VERSION){
		return 0;
	}
	if(hdr->protocol.magic[0] != 'S' || hdr->protocol.magic[1] != 'W'){
		return 0;
	}
	return 1;
}

/*-------------------------------- Debug Functions---------------------------------*/

/**
 * @brief Print protocol header from buffer. When the header is in the header section.
 * @details Print protocol header from buffer. When the header is actually in header section.
 */
static void
print_buf_with_hdr(){
	if(5 >= DEBUG_LEVEL){
		return;
	}

	struct dtn_msg_header *hdr = (struct  dtn_msg_header*) packetbuf_hdrptr(); 
	struct dtn_msg_data *data = (struct dtn_msg_data*) packetbuf_dataptr();

	printf("--- !Dump! ----");
	printf("Msg from: eSender: "); 
	printf(" - { source: "); 
	PRINT2ADDR(&hdr->ereceiver); 
	printf(" id:%d, num_copies:%d } - ", hdr->epacketid, hdr->num_copies);
	printf("DATA: {NAME: %s} \n", data->data);
}

/**
 * @brief Print from buffer when header is in data section.
 * @details Print from buffer when header is in data section.
 */
static void
print_buf_no_hdr(){
	if(5 >= DEBUG_LEVEL){
		return;
	}

	struct dtn_msg_header *hdr = (struct  dtn_msg_header*) packetbuf_dataptr(); 

	printf("!----- NO HDR - DUMP -----! ");
	printf("Msg from: eSender: "); 
	PRINT2ADDR(&hdr->esender); 
	printf(" - { source: "); 
	PRINT2ADDR(&hdr->ereceiver); 
	printf(" id:%d, num_copies:%d } - \n", hdr->epacketid, hdr->num_copies);
}


/**
 * @brief Print the current Queue.
 * @details Print the current Queue.
 */
static void
print_q(){
	if(5 >= DEBUG_LEVEL){
		return;
	}

	struct packetqueue_item *q_item;
	int qLength = packetqueue_len(dtn_global.pkt_q);
	struct dtn_msg_header *tmp_hdr;
	int i;

	q_item = packetqueue_first(dtn_global.pkt_q);
	//Loop through queue
	for(i = 0; i < qLength; i++){
		tmp_hdr = get_hdr_buff(q_item);
		printf("{ ID: %d - Esender: ",tmp_hdr->epacketid);
		PRINT2ADDR(&tmp_hdr->esender);
		printf(" - Ereceiver: ");
		PRINT2ADDR(&tmp_hdr->ereceiver);
		printf(" NUM COPIES: %d } \n", tmp_hdr->num_copies);	

		//Last One should be NULL
		if(q_item->next != NULL){
			q_item = q_item->next;
		}		
	}
}

/**
 * @brief Debug function. Used to output message and address.
 * @details Debug funtion. Used to output message and address.
 * 
 * @param priority Priorty to the message. If smaller then DEBUG_LEVEL, it will not output the message.
 * @param msg Message to output.
 * @param address Address to output.
 */
void
DEBUG_MSG(int priority, char msg[], const rimeaddr_t *address){
	if(priority > DEBUG_LEVEL){
		return;
	}

	printf("DEBUG: %s", msg);
	if(address != NULL){
		PRINT2ADDR(address);
	}
	printf("\n");
}

/**
 * @brief Print Packet when priority is less then DEBUG_LEVEL;
 * @details Print Packet when priority is less then DEBUG_LEVEL;
 * 
 * @param dtn_msg_header Message Header to output.
 * @param priority Priority value that can be printed.
 */
void
DEBUG_PKT(struct dtn_msg_header *hdr, int priority){
	if(priority > DEBUG_LEVEL){
		return;
	}
	printf("!! PACKAGE INFO !! - ID: %d ", hdr->epacketid);
	printf("- ESENDER - ");
	PRINT2ADDR(&hdr->esender);
	printf(" - ERECEIVER - ");
	PRINT2ADDR(&hdr->ereceiver);
	printf(" - NUMB COPIES - %d \n", hdr->num_copies);
}

/**
 * @brief Standard debuging function! Used to test code.
 * @details This function is the same for all implementations of the group.
 * 
 * @param dtn_msg_header The header to output.
 * @param func Function Name.
 */
static void
print_packetbuf(struct dtn_msg_header *a, char *func)
{
  printf("%s, ", func);
  PRINT2ADDR(packetbuf_addr(PACKETBUF_ADDR_SENDER));
  printf(", ");
  PRINT2ADDR(packetbuf_addr(PACKETBUF_ADDR_RECEIVER));
  if (is_spray_wait(a)) {
    printf(", ");
    PRINT2ADDR(&a->esender);
    printf(", ");
    PRINT2ADDR(&a->ereceiver);
    printf(", %d, %d\n",a->epacketid, a->num_copies);
  } else {
    printf(", X, X, X, X\n");
  }
}
/*------------------------------------- Workings ---------------------------------*/
/**
 * @brief Calculate the maximum lifetime of a packet in the queue.
 * @details Calculate the maximum lifetime of a packet in the queue.
 *          2*LOG2(L_COPIES)*REBROADCAST_INTERVAL
 */
/*static int
calculate_max_lifetime(uint16_t num_copies){
	int delay = 2*( log(num_copies) / log(2) )*( (dtn_q_size()*DTN_PACKET_DELAY) + DTN_QUEUE_DELAY);
	printf("Delay: %d \n", delay);
	return delay;
}*/

/** REPLACES THE FUNCTION ABOVE! SO NO NEED TO LINK MATH LIBRARY!! **/
static int
calculate_max_lifetime(uint16_t num_copies){
	return DTN_MAX_LIFETIME;
}

/**
 * @brief Divide value positive value by divider but always return ceiling.
 * @details Divide value positive value by divider but always return ceiling.
 * 
 * @param value The value to divide.
 * @param divider The divider.
 */
static int
ceiling_divider(uint16_t value, int divider){
	if(value % 2){
		//Save Ceiling
		return  1 + ((value - 1) / divider);
	}
	return value / divider;
}


/**
 * @brief Load header structure into header location.
 * @details Load header structure into header location.
 * 
 * @param dtn_msg_header Header to Load.
 */
static void
load_buf_hdr(struct dtn_msg_header *hdr){
	packetbuf_hdralloc(sizeof(struct dtn_msg_header));
	memcpy(packetbuf_hdrptr(), hdr, sizeof(struct dtn_msg_header));
}

/**
 * @brief Create a new header structure.
 * @details Create a new header structure.
 * 
 * @param destination The End destination.
 */
static void
create_buf_hdr(const rimeaddr_t *destination){
	struct dtn_msg_header hdr;

	hdr.protocol.version = DTN_VERSION;
	hdr.protocol.magic[0] = 'S';
	hdr.protocol.magic[1] = 'W';
	hdr.num_copies = DTN_L_COPIES;

	rimeaddr_copy(&hdr.esender, &rimeaddr_node_addr);
	rimeaddr_copy(&hdr.ereceiver, destination);
	hdr.epacketid = dtn_global.pkt_seq_no;

	//Load header into buffer
	load_buf_hdr(&hdr);

	//Increment Sequence Number
	dtn_global.pkt_seq_no += 1;
}

/**
 * @brief Create data section in packet.
 * @details Create data section in packet.
 * 
 * @param dtn_msg_data The data passed by the user.
 */
static void
create_buf_data(struct dtn_msg_data *data){
	packetbuf_clear();
	packetbuf_copyfrom(&data->data, sizeof(data->data));
}

/**
 * @brief Save buffer item in queue.
 * @details Save buffer item in queue. If the packet has just received set number of copies to 0. 
 *          Since no hand off has been given. 0 shows that no runicast has been received.
 * 
 * @param isReceived If the packet has just been received set to 1 (True) if from queue set to 0 (False)
 */
void
queue_buf(int isReceived){
	int *p;
	//Received
	struct dtn_msg_header *hdr = (struct dtn_msg_header*) packetbuf_dataptr();	
	int delay = DTN_TIMEOUT_UNCONFIRMED;

	if(isReceived == 1){
		//update number of copies to 0. No Runic yeath!
		hdr->num_copies = 0;
	} else {
		delay = calculate_max_lifetime(hdr->num_copies);
	}

	//Enqueue Received Buffer
	packetqueue_enqueue_packetbuf(dtn_global.pkt_q, delay, p);
	//Clear Buffer
	packetbuf_clear();
}

/**
 * @brief Load the packet item into the buffer.
 * @details Load the packet item into the buffer. If loading packet for Broadcast check that number of copies is not 0.
 * 
 * @param packetqueue_item The packet queue item needed to be loaded in the buffer.
 * @param is_bcast If loading packet for a broadcast set to 1 (True) else 0 (False).
 * @return integer if 0 (False) if 1 (True)
 */
static int
load_pkt_item(struct packetqueue_item *item, uint8_t is_bcast){
	struct queuebuf *q_buf;
	struct dtn_msg_header *hdr;

	//Get Queue buffer from packetqueue_item
	q_buf = packetqueue_queuebuf(item);

	//If bcast check that L != 0
	if(is_bcast == 1){
		hdr = (struct dtn_msg_header*) queuebuf_dataptr(q_buf);
		if(hdr->num_copies == 0){
			return 0;
		}
	}
	//Convert buff to header
	queuebuf_to_packetbuf(q_buf);
	return 1;
}

/**
 * @brief Remove data from buffer.
 * @details Remove data from buffer.
 */
static void
remove_data_from_buf(){
	struct dtn_msg_header *hdr;
	struct dtn_msg_header *hdr_cpy;
	//Read data ptr
	hdr = (struct dtn_msg_header*) packetbuf_dataptr();
	hdr_cpy = hdr;
	//Clear Buffer
	packetbuf_clear();
	//Copy in buffer
	packetbuf_copyfrom(hdr_cpy, sizeof(struct dtn_msg_header));
}

/**
 * @brief Load header only to buffer.
 * @details Load header only to buffer.
 * 
 * @param packetqueue_item The packet queue item to load.
 * @param isHalved Should the numb_copies be halved? if 1(True) half if 0 (False) leave as is.
 */
static void
load_hdr_item(struct packetqueue_item *item, int isHalved){
	struct queuebuf *q_buf;
	struct dtn_msg_header *hdr;
	struct dtn_msg_header cpy_hdr;

	//Get buffer
	q_buf = packetqueue_queuebuf(item);
	//Convert buff to header
	hdr = (struct dtn_msg_header*) queuebuf_dataptr(q_buf);
	//Copy so if alteration made it is not comitted in good memory
	memcpy(&cpy_hdr, hdr, sizeof(struct dtn_msg_header));
	//Half value. Sending with runicast
	if(isHalved == 1 && hdr->num_copies >= 2){
		cpy_hdr.num_copies = ceiling_divider(cpy_hdr.num_copies, 2);
	}
	//Add Header in data buffer
	packetbuf_copyfrom(&cpy_hdr, sizeof(struct dtn_msg_header));
}

/**
* Parse through queue and broadcast
*/
/**
 * @brief Search in queue and find packet with same epacketid, esender and ereceiver.
 * @details Search in queue and find packet with same epacketid, esender and ereceiver. 
 * 
 * @param pkt_id The packet id
 * @param esender The End Sender Address
 * @param ereceiver The End Receiver Address
 * @return Packet Queue Pointer or NULL (0) if not found.
 */
static struct packetqueue_item
*find_packet(uint16_t pkt_id, const rimeaddr_t *esender, const rimeaddr_t *ereceiver){
	int i = 0;
	int qLength = packetqueue_len(dtn_global.pkt_q);
	struct packetqueue_item *q_item;
	struct dtn_msg_header *tmp_hdr;
	
	//Queue is empty!!
	if(qLength == 0){
		return NULL;
	}
	
	q_item = packetqueue_first(dtn_global.pkt_q);
	//Loop through queue
	for(i = 0; i < qLength; i++){
		tmp_hdr = get_hdr_buff(q_item);
		if(tmp_hdr->epacketid == pkt_id &&
				rimeaddr_cmp(&tmp_hdr->esender, esender) == 1 &&
				rimeaddr_cmp(&tmp_hdr->ereceiver, ereceiver) == 1){
			return q_item;
		}

		//Last One should be NULL
		if(q_item->next != NULL){
			q_item = q_item->next;
		}		
	}
	return NULL;
}

/**
 * @brief Remove queued packet.
 * @details Remove queue packet. Copied from packetqueue.c
 * 
 * @param item packetqueue_item to delete from queue.
 */
static void
dtn_remove_queued_packet(void *item) {
  struct packetqueue_item *i = item;
  struct packetqueue *q = i->queue;

  list_remove(*q->list, i);
  queuebuf_free(i->buf);
  ctimer_stop(&i->lifetimer);
  memb_free(q->memb, i);
}

/**
 * @brief Handle queue broadcast. Timer are set according to need.
 * @details Handle queue broadcast. A message is sent in interval set in DTN_PACKET_DELAY,
 *          this helps reduce traffic in channel making it more probable to receive a runicast.
 *          After all the queue is broadcasted a delay of DTN_QUEUE_DELAY is introduced to 
 *          reduce traffic on the channel. The function also checks that the number of copies is larger
 *          equak to 1. 
 * 
 * @param p_item Not used! The packet item sent is stored in the global variable.
 */
static void
broadcast_next(void *p_item){ 
	static struct packetqueue_item *next_item;
	struct dtn_msg_header *msg_hdr;
	//If queue is empty nothing to bcast
	if(dtn_q_size(dtn_global) <= 0){
		ctimer_set(&dtn_global.local_ctimer, DTN_QUEUE_DELAY, broadcast_next, &dtn_global.pkt_last_sent);
		return;
	}

	//If packet has not benn sent yeath or end of queue
	if(dtn_global.pkt_last_sent == NULL || dtn_global.pkt_last_sent->next == NULL){
		next_item = packetqueue_first(dtn_global.pkt_q);
	} else {
		next_item = dtn_global.pkt_last_sent->next;
	}
	
	msg_hdr = get_hdr_buff(next_item);
	//Check L value if 0 do not send!!!
	if(msg_hdr->num_copies < 1){
		//Immediately go to next calll
		ctimer_set(&dtn_global.local_ctimer, DTN_PACKET_DELAY, broadcast_next, &dtn_global.pkt_last_sent);
		return;
	}

	//Store Next Item
	dtn_global.pkt_last_sent = next_item;
	//Load packet in buffer
	if(load_pkt_item(next_item, 1) == 0){
		//Immediately go to next calll
		ctimer_set(&dtn_global.local_ctimer, DTN_PACKET_DELAY, broadcast_next, &dtn_global.pkt_last_sent);
		return;
	}
	//send broadcast
	print_packetbuf(packetbuf_dataptr(), "Spray");
	broadcast_send(&dtn_chan.bc);
	
	//@todo check which is most expensive calling function or creating new variable for delay?
	if(next_item->next == NULL){
		//Reset Timer
		ctimer_set(&dtn_global.local_ctimer, DTN_QUEUE_DELAY, broadcast_next, &dtn_global.pkt_last_sent);
	} else {
		//Reset Timer
		ctimer_set(&dtn_global.local_ctimer, DTN_PACKET_DELAY, broadcast_next, &dtn_global.pkt_last_sent);
	}
	
	return;
}
/*------------------------------------- Callbacks --------------------------------*/

/**
 * @brief Callback function called when Broadcast message is received.
 * @details Callback function called when Broadcast message is received. This checks that the broacast
 *          is an actuall message from a Spray and Wait protocol, that is has not been received. If full
 *          the message is not added to the queue. Once the message is going to be kept a Unicast is sent.
 * 
 * @param broadcast_conn the connection of the broadcast.
 * @param from Address from whom the message was received.
 */
static void
recv_bcast(struct broadcast_conn *c, const rimeaddr_t *from){
	struct dtn_msg_header *hdr = (struct dtn_msg_header*) packetbuf_dataptr();
	struct dtn_msg_header *saved_hdr;
	struct packetqueue_item *i_q;
	struct queuebuf *saved_buf;
	static rimeaddr_t esender, ereceiver;
	uint16_t pkt_id;
	
	DEBUG_MSG(2, "- RCV_BCAST - BCAST RECEVIED!! -- FROM: ", from);
	print_buf_no_hdr();

	if(is_spray_wait(hdr) == 0){
		return;
	}

	//Copy in temporary holders
	rimeaddr_copy(&esender, &hdr->esender);
	rimeaddr_copy(&ereceiver, &hdr->ereceiver);
	pkt_id = hdr->epacketid;

	//Packet is searching for source! If not source Drop
	//Or Sender was me!
	if((hdr->num_copies == 1 && rimeaddr_cmp(&ereceiver, &rimeaddr_node_addr) == 0) ||
		rimeaddr_cmp(&esender, &rimeaddr_node_addr) == 1){
		DEBUG_MSG(2, "- RCV_BCAST - COPY and NOT DESTINATION || I WAS SENDER!! -- FROM: ", from);
		return;
	}

	//Message for me!
	if(rimeaddr_cmp(&ereceiver, &rimeaddr_node_addr) == 1){
		printf("- RCV_BCAST - MY PRECIOUS!! :) !! -- FROM: %d SENDER: %d \n", from->u8[0], esender.u8[0]);
		//Put header only in buffer
		remove_data_from_buf();
		//Send Unicast
		print_packetbuf(packetbuf_dataptr(), "request");
		unicast_send(&dtn_chan.uc, from);
		
		return;
	} 

	//Find packet in queue
	i_q = find_packet(pkt_id, &esender, &ereceiver);
	//Packet saved
	if(i_q != NULL){
		saved_buf = packetqueue_queuebuf(i_q);
		saved_hdr = (struct dtn_msg_header*) queuebuf_dataptr(saved_buf);
		if(saved_hdr->num_copies > 0){
			DEBUG_MSG(2, "- RCV_BCAST - EXIST with num_copies > 0!! -- FROM: ", from);
			return;
		}
	}

	//Sorry Queue full! Drop msg
	if(i_q == NULL && dtn_q_size() == MAX_QUEUE_PACKETS){
		return;
	}

	//Not in Queue and queue not full!
	if(i_q == NULL){
		//Queue Received packet
		queue_buf(1);
		//Get new position
		i_q = find_packet(pkt_id, &esender, &ereceiver);
	}
	
	//Load Header
	load_hdr_item(i_q, 0);
	//Send Unicast 
	print_packetbuf(packetbuf_dataptr(), "request");
	unicast_send(&dtn_chan.uc, from);
}

/**
 * @brief Callback when unicast is received.
 * @details Callback when unicast is received. When Unicast is received check if the item is still in the queue
 *          and that the message sent was not delivered to the End receiver. If the end receiver do nothing, but
 *          if not send a reliable unicast with the current saved Number of Copies / 2. 
 * 
 * @param unicast_conn Unicast Connection
 * @param from Address from whom the unicast was received
 */
static void
recv_unic(struct unicast_conn *c, const rimeaddr_t *from){
	struct dtn_msg_header *hdr = (struct dtn_msg_header*) packetbuf_dataptr();
	struct packetqueue_item *i_q;
	DEBUG_MSG(1, "RECEVIED UNICAST!! -- FROM: ", from);
	DEBUG_PKT(hdr, -1);

	if(is_spray_wait(hdr) == 0){
		return;
	}

	i_q = find_packet(hdr->epacketid, &hdr->esender, &hdr->ereceiver);
	//Packet might have been removed!
	if(i_q == NULL){
		return;
	}

	//Packet received!! remove packet
	if(rimeaddr_cmp(&hdr->ereceiver, from) == 1){
		DEBUG_MSG(3, "PACKET RECEVIED TO DESTINATION: ", &hdr->esender);	
		dtn_remove_queued_packet(i_q);
		print_q();
		return;
	}
	
	//Load Item
	load_hdr_item(i_q, 1);
	//Tmp store packetqueue item
	dtn_global.sent_runicast = i_q;
	//Send runicast
	print_packetbuf(packetbuf_dataptr(), "handoff");
	runicast_send(&dtn_chan.rc, from, DTN_MAX_TRANSMISSIONs);
}

/**
 * @brief Callback when Runicast is received.
 * @details Callback when Runicast is received. If runicast is received change the Number of Copies of the
 *          packet to the one forwarded.
 * 
 * @param runicast_conn Runicast conenction
 * @param from Address from whom the message was sent.
 * @param seqno Seqno
 */
static void 
recv_runic(struct runicast_conn *c, const rimeaddr_t *from, uint8_t seqno){
	DEBUG_MSG(3, "Received Reliable Unicast!! FROM: ", from);

	struct dtn_msg_header *hdr = (struct dtn_msg_header*) packetbuf_dataptr();
	struct dtn_msg_header *saved_hdr;
	struct packetqueue_item *i_q;
	struct queuebuf *q_buf;

	if(is_spray_wait(hdr) == 0){
		return;
	}

	i_q = find_packet(hdr->epacketid, &hdr->esender, &hdr->ereceiver);
	if(i_q == NULL || hdr->num_copies <= 0){
		return;
	}

	q_buf = packetqueue_queuebuf(i_q);
	//Convert buff to header
	saved_hdr = (struct dtn_msg_header*) queuebuf_dataptr(q_buf);	
	saved_hdr->num_copies =  hdr->num_copies;
	//Overwritten timer to extend time!!
	ctimer_set(&i_q->lifetimer, calculate_max_lifetime(saved_hdr->num_copies), dtn_remove_queued_packet, i_q);
}

/**
 * @brief Callback when Reliable Unicast is Received Successfull.
 * @details Callback when Reliable Unicast is Received Successfull. When this message is received
 *          Half the local Numebr of Copies.
 * 
 * @param runicast_conn Runicast Connection
 * @param from Address with the receiving Address
 * @param retransmissions Number of retransmissions
 */
static void 
sent_runic(struct runicast_conn *c, const rimeaddr_t *from, uint8_t retransmissions){
	if(dtn_global.sent_runicast == NULL){
		return;
	}

	DEBUG_MSG(1, "SUCCESS: SENT RUNICAST TO: ", from);
	struct packetqueue_item *i_q = dtn_global.sent_runicast;
	struct queuebuf *q_buf;
	struct dtn_msg_header *saved_hdr;

	q_buf = packetqueue_queuebuf(i_q);
	//Convert buff to header
	saved_hdr = (struct dtn_msg_header*) queuebuf_dataptr(q_buf);
	saved_hdr->num_copies = ceiling_divider(saved_hdr->num_copies, 2);

	print_q();
}

/**
* Timedout Reliable Unicast
*/
/**
 * @brief Callback when Reliable Unicast fails.
 * @details Callback when Reliable Unicast fails. Do nothing
 * 
 * @param runicast_conn Reliable Unocast Connection
 * @param from Address that failed
 * @param retransmissions Number of retransmissions
 */
static void 
timedout_runic(struct runicast_conn *c, const rimeaddr_t *from, uint8_t retransmissions){
	DEBUG_MSG(1, "RUNICAST TIMEDOUT!! TO: ", from);
}

/*--------------------------------------------------------------------------------*/

//Initialize Callbacks
static const struct broadcast_callbacks dtn_bcast_call = {recv_bcast};
static const struct unicast_callbacks dtn_unic_call = {recv_unic};
static const struct runicast_callbacks dtn_runic_call = {
				recv_runic,
				sent_runic,
				timedout_runic
			};

/*--------------------------- Main Functions --------------------------------------*/

void 
dtn_init(){
	//Open Channels
	broadcast_open(&dtn_chan.bc, DTN_BCAST_CHANNEL, &dtn_bcast_call);
	unicast_open(&dtn_chan.uc, DTN_UNIC_CHANNEL, &dtn_unic_call);
	runicast_open(&dtn_chan.rc, DTN_RUNIC_CHANNEL, &dtn_runic_call);

	//Initialize Packet Queue
  	packetqueue_init(&pkt_q);
  	//Start struct
  	dtn_global.pkt_seq_no = 0;
  	dtn_global.pkt_q = &pkt_q;
  	dtn_global.pkt_last_sent = NULL;
  	dtn_global.sent_runicast = NULL;
  	//Start broadcasting in number of QUEUE DELAY
  	ctimer_set(&dtn_global.local_ctimer, DTN_QUEUE_DELAY, broadcast_next, &dtn_global.pkt_last_sent);
}

void
dtn_close(){
	broadcast_close(&dtn_chan.bc);
	unicast_close(&dtn_chan.uc);
	runicast_close(&dtn_chan.rc);
}

int
dtn_q_size(){
	return packetqueue_len(dtn_global.pkt_q);
}

void
dtn_new_buff(struct dtn_msg_data *data, const rimeaddr_t *destination){
	if(dtn_q_size() == MAX_QUEUE_PACKETS){
		return;
	}
	//Create new buffer
	//Data Buffer
	create_buf_data(data);
	//Header Buffer
	create_buf_hdr(destination);
	//Print Buffer
	print_buf_with_hdr();
	//Queue newly created record
	queue_buf(0);
}