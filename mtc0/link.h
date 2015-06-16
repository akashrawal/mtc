/* link.h
 * Support for transmission and reception of message
 * 
 * Copyright 2013 Akash Rawal
 * This file is part of MTC.
 * 
 * MTC is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * MTC is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with MTC.  If not, see <http://www.gnu.org/licenses/>.
 */

 
/**
 * \addtogroup mtc_link
 * \{
 * 
 * Links are abstractions for sending and receiving serialized data
 * over network.
 * 
 * Links are represented by MtcLink structure.
 * 
 * You create a file descriptor link using mtc_fd_link_new() from 
 * two file descriptors, one for sending, other for receiving 
 * (which can be same too, e.g. in case of sockets.)
 * 
 * To send messages use mtc_link_queue(). This only enqueue data for
 * transmission. To actually transmit data use mtc_link_send().
 * To receive a message use mtc_link_receive().
 * Whether these functions perform blocking or non-blocking IO is 
 * upto underlying channel. 
 * 
 * A link can be stopped so that no further data transmission or 
 * reception occurs and then the underlying channel can be accessed 
 * liberally. Transmission and reception side can be stopped separately.
 * The transmission side is stopped when a message queued with stop 
 * paramater set to 1 is sent successfully. Reception side is stopped 
 * on successful reception of a message with stop paramater set to 1.
 */

///Structure that represents a link
typedef struct _MtcLink MtcLink;

///Status of a link
typedef enum
{
	///Data transfer may take place
	MTC_LINK_STATUS_OPEN = 0,
	///Data transfer is temporarily stopped. Underlying channel can 
	///be accessed liberally. 
	MTC_LINK_STATUS_STOPPED = 1,
	///Link is broken.
	MTC_LINK_STATUS_BROKEN = 2
} MtcLinkStatus;


/**Gets the currernt outgoing connection status of the link.
 * 
 * This function does not perform any polling and just returns the value
 * stored in the link.
 * 
 * \param self The link
 * \return an MtcLinkStatus value that indicates current status of the link
 */
MtcLinkStatus mtc_link_get_out_status(MtcLink *self);

/**Gets the currernt incoming connection status of the link.
 * 
 * This function does not perform any polling and just returns the value
 * stored in the link.
 * 
 * \param self The link
 * \return an MtcLinkStatus value that indicates current status of the link
 */
MtcLinkStatus mtc_link_get_in_status(MtcLink *self);

/**Considers the link to be broken. 
 * No further data transmission can happen.
 * \param self The link
 */
void mtc_link_break(MtcLink *self);

#define mtc_link_is_broken(self) \
	(((MtcLink *) (self))->in_status == MTC_LINK_STATUS_BROKEN)

/**Resumes data transmission.
 * \param self The link
 */
void mtc_link_resume_out(MtcLink *self);

/**Resumes data reception.
 * \param self The link
 */
void mtc_link_resume_in(MtcLink *self);

/**Schedules a message to be sent through the link.
 * 
 * This function does no IO, use mtc_link_send() to actually send data.
 * 
 * The link increments the reference count of the message by 1,
 * and after successful transmission of message or destruction of link
 * it is decremented. So you can call mtc_msg_unref() on the message
 * composer after mtc_link_queue() so that the message  is freed
 * as soon as the message is sent.
 * \param self The link
 * \param msg The message to send.
 * \param stop Whether link should be stopped on successful 
 *             transmission of the message
 */
void mtc_link_queue
	(MtcLink *self, MtcMsg *msg, int stop);

/**Determines whether mtc_link_send will try to send any data.
 * This can be useful before using select() or poll() functions.
 * \param self The link
 * \return 1 if link has data to send, 0 otherwise
 */
int mtc_link_can_send(MtcLink *self);

///Return status of sending or receiving operation on link.
typedef enum
{
	///All data has been sent successfully / a message
	///has been received successfully
	MTC_LINK_IO_OK = 0,
	///A message with stop flag was/has been sent. Link is stopped now./ 
	///No data can be received because link has been stopped. 
	MTC_LINK_IO_STOP = 1,
	///A temproary error has occurred. Operation might succeed if tried
	///again.
	MTC_LINK_IO_TEMP = 2,
	///An irrecoverable error has occurred. Link is broken now.
	MTC_LINK_IO_FAIL = 3
} MtcLinkIOStatus;

/**Tries to send all queued data
 * \param self The link
 * \return Status of this sending operation
 */
MtcLinkIOStatus mtc_link_send(MtcLink *self);

///Structure to hold the received data
typedef struct
{
	///Message received
	MtcMsg *msg;
	///Set to 1 if link has been stopped
	int stop;
} MtcLinkInData;

/**Tries to receive one message
 * \param self The link
 * \param data Return location for received message
 * \return Status of this receiving operation
 */
MtcLinkIOStatus mtc_link_receive(MtcLink *self, MtcLinkInData *data);

///Event source base type for MtcLink
typedef struct
{
	MtcEventSource parent;
	
	MtcLink *link;
	void (*received) (MtcLink *link, MtcLinkInData in_data, void *data);
	void (*broken) (MtcLink *link, void *data);
	void (*stopped) (MtcLink *link, void *data);
	void *data;
} MtcLinkEventSource;

//TODO: write doc
#define mtc_link_get_event_source(self) \
	(&(((MtcLink *) (self))->event_source))

void mtc_link_set_events_enabled(MtcLink *self, int val);

#define mtc_link_get_events_enabled(self) \
	(((MtcLink *) (self))->events_enabled)

/**Increments the reference count of the link by one.
 * \param self The link
 */
void mtc_link_ref(MtcLink *self);

/**Decrements the reference count of the link by one.
 * If the reference count becomes equal to zero the link
 * is destroyed.
 * \param self The link.
 */
void mtc_link_unref(MtcLink *self);

/**Gets reference count of the link.
 * \param self The link
 * \return The reference count of the link.
 */
int mtc_link_get_refcount(MtcLink *self);

//TODO: document how to extend MtcLink

//Value table for implementing various types of links
typedef struct
{
	void (*queue) 
		(MtcLink *self, MtcMsg *msg, int stop);
	int (*can_send) 
		(MtcLink *self);
	MtcLinkIOStatus (*send)
		(MtcLink *self);
	MtcLinkIOStatus (*receive)
		(MtcLink *self, MtcLinkInData *data);
	void (*set_events_enabled) (MtcLink *self, int val);
	MtcEventSourceVTable event_source_vtable;
	void (*action_hook) (MtcLink *self); //Can be NULL
	void (*finalize) (MtcLink *self);
} MtcLinkVTable;

struct _MtcLink
{
	//Reference count
	int refcount;
	
	MtcLinkStatus in_status, out_status;
	int events_enabled;
	MtcLinkEventSource event_source;
	
	const MtcLinkVTable *vtable;
};

//Creates a new link.
MtcLink *mtc_link_create(size_t size, const MtcLinkVTable *vtable);

///\}
