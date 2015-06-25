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
 * A link can be stopped reversibly so that no further data 
 * transmission or reception occurs and then the underlying channel 
 * can be accessed liberally. 
 * Transmission and reception side can be stopped separately.
 * This can be done instantly by mtc_link_stop_in() or
 * mtc_link_stop_out() or with information to other side.
 * An informed stop on transmission side when a message queued with stop 
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

/**Stops the incoming connection of the link. 
 * \param self The link
 */
void mtc_link_stop_in(MtcLink *self);

/**Stops the outgoing connection of the link. 
 * \param self The link
 */
void mtc_link_stop_out(MtcLink *self);

/**Considers the link to be broken. 
 * No further data transfer can happen.
 * \param self The link
 */
void mtc_link_break(MtcLink *self);

/**Determines if the link is broken 
 * (data transfer is irreversibly stopped)
 * \param self The link
 */
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

/**Determines whether link has any data waiting to be sent.
 * \param self The link
 * \return 1 if link has data to send, 0 otherwise
 */
int mtc_link_has_unsent_data(MtcLink *self);

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

///Event source structure for MtcLink
typedef struct
{
	///Event source structure
	MtcEventSource parent;
	
	///The link this event source belongs
	MtcLink *link;
	///Function to be called when all data has been sent, or NULL
	void (*sent) (MtcLink *link, void *data);
	///Function to be called when a message has been received, or NULL
	void (*received) (MtcLink *link, MtcLinkInData in_data, void *data);
	///Function to be called when link is broken, or NULL
	void (*broken) (MtcLink *link, void *data);
	///Function to be called when transmission side of the link is
	///stopped
	void (*stopped) (MtcLink *link, void *data);
	///Data to pass to above callback functions
	void *data;
} MtcLinkEventSource;

/**Gets event source structure for given link. It's lifetime is 
 * tied to the link.
 * \param self The link
 * \return Event source for the given link
 */
#define mtc_link_get_event_source(self) \
	((MtcLinkEventSource *) (&(((MtcLink *) (self))->event_source)))

/**Sets whether given link performs event-driven IO.
 * This is not effective unless event source is backed by an 
 * implementation.
 * You need to ensure that nonblocking IO is enabled on underlying
 * mechanism.
 * \param self the link
 * \param val Nonzero to enable event-driven IO, 0 to disable
 */
void mtc_link_set_events_enabled(MtcLink *self, int val);

/**Gets whether event-driven IO is enabled on the link.
 * \param self The link
 * \return Nonzero if event-driven IO is enabled
 */
#define mtc_link_get_events_enabled(self) \
	((int) (((MtcLink *) (self))->events_enabled))

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


///Value table for implementing various types of links
typedef struct
{
	///Implementation for mtc_link_queue()
	void (*queue) 
		(MtcLink *self, MtcMsg *msg, int stop);
	///Implementation for mtc_link_has_unsent_data()
	int (*has_unsent_data) 
		(MtcLink *self);
	///Implementation for mtc_link_send(). 
	///Link's status is automatically adjusted from the return value.
	MtcLinkIOStatus (*send)
		(MtcLink *self);
	///Implementation for mtc_link_receive().
	///Link's status is automatically adjusted from the return value.
	MtcLinkIOStatus (*receive)
		(MtcLink *self, MtcLinkInData *data);
	///Implementation for mtc_link_set_events_enabled().
	void (*set_events_enabled) (MtcLink *self, int val);
	///Value table for the event source
	MtcEventSourceVTable event_source_vtable;
	///Function called when link's status changes, and when
	///mtc_link_queue(), mtc_link_send(), and mtc_link_receive()
	///is called. Can be NULL.
	void (*action_hook) (MtcLink *self);
	///Called to free up all resources for the link.
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

/**Allocates a new link structure. Size of the structure should 
 * be atleast sizeof(MtcLink). 
 * \param size Size of the link structure. 
 * 		Should be atleast sizeof(MtcLink).
 * \param vtable Value table containing implementation functions
 */
MtcLink *mtc_link_create(size_t size, const MtcLinkVTable *vtable);

/**An event-driven flush operator that sends all unsent data in links 
 * and destroys them.
 */
typedef struct _MtcLinkAsyncFlush MtcLinkAsyncFlush;

/**Creates a new MtcLinkAsyncFlush object.
 * \return A new MtcLinkAsyncFlush
 */
MtcLinkAsyncFlush *mtc_link_async_flush_new();

/**Adds a link to MtcLinkAsyncFlush object. 
 * link's event source should already be backed by an implementation.
 * The flush operator holds its own reference to the link, so calling
 * code can drop its reference immediately.
 * \param flush The flush operator to add link to.
 * \param link The link to be flushed
 */
void mtc_link_async_flush_add(MtcLinkAsyncFlush *flush, MtcLink *link);

/**Removes all links in the flush operator.
 * \param flush The flush operator
 */
void mtc_link_async_flush_clear(MtcLinkAsyncFlush *flush);

/**Increments the reference count by 1.
 * \param flush The flush operator
 */
void mtc_link_async_flush_ref(MtcLinkAsyncFlush *flush);

/**Decrements the reference count by 1.
 * If reference count drops to 0, the flush operator is destroyed.
 * \param flush The flush operator
 */
void mtc_link_async_flush_unref(MtcLinkAsyncFlush *flush);

///\}
