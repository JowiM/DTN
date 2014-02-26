/**
 * @file
 *        Header file for Delay Tolerant Network Protocol
 * @author
 *        Johann Mifsud <johann.mifsud.13@ucl.ac.uk>
 */

#include "net/rime.h"
#include <math.h>
/**
 * @brief	Broadcast Channel - Spray Message
 */
#define DTN_BCAST_CHANNEL 128
/**
 * @brief	Unicast Channel - Handle Spray Message Notification
 */
#define DTN_UNIC_CHANNEL DTN_BCAST_CHANNEL+1
/**
 * @brief	Reliable Unicast Channel - L Message Propogation
 */
#define DTN_RUNIC_CHANNEL DTN_UNIC_CHANNEL+1
/**
 * @brief	The Queue Size Limit
 */
#define MAX_QUEUE_PACKETS 5
/**
 * @brief	MAX Retransmission for Reliable Unicast
 */
#define DTN_MAX_TRANSMISSIONs 3
/**
 * @brief	Max Number of L Copies that can be distributed
 */
#define DTN_L_COPIES 8
/**
 * @brief	Protocol Version Number
 */
#define DTN_VERSION 1
/**
 * @brief	Control Verbosaty of the Debug functions
 */
#define DEBUG_LEVEL 0
/**
 * @brief	Delay incured on queue when all queue has been broadcasted
 */
#define DTN_QUEUE_DELAY 3*CLOCK_SECOND
/**
 * @brief	Delay incurred for next packet broadcast
 */
#define DTN_PACKET_DELAY 1*CLOCK_SECOND
/**
 * @brief	Packet that has not been given any Copies to propogate is dropped after this value
 */
#define DTN_TIMEOUT_UNCONFIRMED 1*CLOCK_SECOND
/**
 * @brief	The FIXED Lifetime a packet has in a queue. Used when log equation is not used.
 */
#define DTN_MAX_LIFETIME 60*CLOCK_SECOND
/**
 * @brief 	Holds the Rime channels
 */
struct dtn_channels{
	struct broadcast_conn bc;
	struct unicast_conn uc;
	struct runicast_conn rc;
};

/**
 * @brief 	Structure used to hold general variables
 * @details 	Holds variables that are required throughout the execution of the protocol
 *           	- *pkt_last_sent: Points to the last pkt sent. Used by the method that handles
 *          			 the next packet to be broadcasted.
 *              - pkt_seq_no: Holds an integer which will be used as an id when generating an new message
 *              - *pkt_q: Holds the packet queue. Which is initialised in the beginning.
 *              - local_ctimer: Used to initialise packet queue timer.
 *              - *sent_runicast: Holds the packet item that a runicast has been sent for. Important 
 *          		because when success is received, there isn't a way how to know which packet
 *          		has been ACKed. 
 */
struct dtn_vars{
	struct packetqueue_item *pkt_last_sent;
	uint8_t pkt_seq_no;
	struct packetqueue *pkt_q;
	struct ctimer local_ctimer;
	struct packetqueue_item *sent_runicast;
};

/**
 * @brief	Sub Struct of the MSG header. 
 * @details	Holds Protocol General Information:
 *          - version: The Version of the protocol
 *          - magic: is a 2 byte array used to state the name of the protocol
 */
struct dtn_proto_header {
	uint8_t version;
	uint8_t magic[2];
};


/**
 * @brief The Protocol Header that is used for every package sent from DTN
 * @details The Protocol Header that is used for every package sent from DTN.
 *          This is vital because it contains all the information needed so the protocol works.
 *          	- protocol: Holds protocol related information. Such as version
 *          	- num_copies: The number of copies that a receiving node can distribute. Used in two important ways:
 *          		-# If the value is 1 on broadcast this means that this is not an actual spray.
 *          			But searching for destination. Used so we do not implemment another channel for neighbour discovery
 *          		-# During runicast (HandOff) the actual L value that should be saved is sent.
 *          	- esender: Show the origin Sender
 *          	- ereceiver: Shows the intended message to.
 *          	- epacketid: The ID given to the message. Should be kept through out the packet life.
 */
struct dtn_msg_header {
	struct dtn_proto_header protocol;
	uint16_t num_copies;
	rimeaddr_t esender;
	rimeaddr_t ereceiver;
	uint16_t epacketid;
};

/**
 * @brief Holds the data message. For now holds a char array.
 * 
 */
struct dtn_msg_data {
	char data[10];
};

/**
 * @brief Initialises all the variables and rime channels.
 * @details Initialises all the variables and rime channels. 
 *          This includes the struct dtn_vars and channels for broadcast, unicast and runicast 
 */
void dtn_init();

/**
 * @brief Close channels
 * @details Close opened channels
 */
void dtn_close();

/**
 * @brief Get the current size of the packet queue
 * @details return the value of the packet queue
 * @return Integer
 */
int dtn_q_size();

/**
 * @brief Create a new message.
 * @details Creates a new message. When the message is created it is added to the packet queue.
 *          NB: If the queue is full the message is dropped. This is part of the protocol specification.
 * 
 * @param dtn_msg_data Data that will populate the message
 * @param destination Destination to whom the packete should be sent.
 */
void dtn_new_buff(struct dtn_msg_data *data, const rimeaddr_t *destination);