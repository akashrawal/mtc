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
 * Links are abstractions for channels for transfer of messages. 
 * 
 * Links are represented by MtcLink structure.
 * 
 * You create a link using mtc_link_new() from two file descriptors,
 * one for sending, other for receiving (which can be same too, e.g. in
 * case of sockets.)
 * 
 * To send messages and signals use mtc_link_queue_msg()
 * and mtc_msg_queue_signal(). These functions only enqueue data for
 * transmission. To actually transmit data use mtc_link_send().
 * 
 * To receive a message or a signal use mtc_link_receive().
 * 
 * In sending and receiving, there is an additional concept of destinations.
 * These are where a message or a signal
 * may be directed after receiving on the other end. 
 * Destinations are uniquely identified within a process by 
 * a destination identifier of type uint64_t
 * See MtcSwitch for more information. 
 * However in MTC Lowlevel this concept of destinations
 * is completely optional, it makes sense in MTC Highlevel where
 * destinations are heavily used.
 * 
 * Links can do both blocking and non-blocking IO. To configure this
 * you can use mtc_link_set_blocking(), or get its file descriptor
 * and run fcntl() calls manually.
 */

///Structure that represents a link
typedef struct _MtcLink MtcLink;

///Status of a link
typedef enum
{
	///Data transfer may take place
	MTC_LINK_STATUS_OPEN = 0,
	///Data transfer is temporarily stopped due to MTC_SIGNAL_STOP
	MTC_LINK_STATUS_STOPPED = 1,
	///Link is broken.
	MTC_LINK_STATUS_BROKEN = 2
} MtcLinkStatus;

///Return status of sending or receiving operation on link.
typedef enum
{
	///All data has been sent successfully / a message or a signal
	///has been received successfully
	MTC_LINK_IO_OK = 0,
	///All data could not be sent because link has been stopped. / 
	///No data can be received because link has been stopped. 
	MTC_LINK_IO_STOP = 1,
	///A temproary error has occurred. Operation might succeed if tried
	///again.
	MTC_LINK_IO_TEMP = 2,
	///An irrecoverable error has occurred. Link is broken now.
	MTC_LINK_IO_FAIL = 3
} MtcLinkIOStatus;

///Structure to hold the received data
typedef struct
{
	///The destination to send data to
	uint64_t dest;
	///The destination where reply to the data may be sent. 
	uint64_t reply_to;
	///Message, if this is a message, or NULL if this is a signal.
	MtcMsg *msg;
	///Signal number if this is a signal
	uint32_t signum;
} MtcLinkInData;

///Structure to hold callback function to call when link stops
typedef struct _MtcLinkStopCB MtcLinkStopCB;
struct _MtcLinkStopCB
{
	MtcLinkStopCB *prev, *next;
	uintptr_t stop_id;
	///The callback function
	void (*cb) (MtcLink *link, void *data);
	///Data to pass to the callback function
	void *data;
};

///Structure to hold tests for an event loop integration to perform
typedef struct _MtcLinkTest MtcLinkTest;
struct _MtcLinkTest
{
	///pointer to next test, or NULL
	MtcLinkTest *next;
	///Name of the test. Should be well known for event loop 
	///integrations to support. Unused characters should be set to NULL.
	char name[8];
	///Test data follows it
};

///Test name for poll() test
#define MTC_LINK_TEST_POLLFD "pollfd"

///Test data for poll() test
typedef struct 
{
	MtcLinkTest parent;
	///File descriptor to poll
	int fd;
	///Events to check on the file descriptor
	int events;
	///After polling, results should be set here
	int revents;
} MtcLinkTestPollFD;

///Events supported by the poll() test
typedef enum 
{
	MTC_POLLIN = 1 << 0,
	MTC_POLLOUT = 1 << 1,
	MTC_POLLPRI = 1 << 2,
	MTC_POLLERR = 1 << 3,
	MTC_POLLHUP = 1 << 4,
	MTC_POLLNVAL = 1 << 5
} MtcLinkPollEvents;

///Flags specifying what to do based on test results
typedef enum 
{
	///Data may be sent without blocking
	MTC_LINK_TEST_SEND = 1 << 0,
	///There is data ready to be received without blocking
	MTC_LINK_TEST_RECV = 1 << 1,
	///Link should be broken
	MTC_LINK_TEST_BREAK = 1 << 2
} MtcLinkTestAction;

//Value table for implementing various types of links
typedef struct
{
	void (*queue_msg) 
		(MtcLink *self, uint64_t dest, uint64_t reply_to, MtcMsg *msg);
	void (*queue_signal) 
		(MtcLink *self, uint64_t dest, uint64_t reply_to, uint32_t signal);
	int (*can_send) 
		(MtcLink *self);
	MtcLinkIOStatus (*send)
		(MtcLink *self);
	MtcLinkIOStatus (*receive)
		(MtcLink *link, MtcLinkInData *data);
	MtcLinkTest *(*get_tests) (MtcLink *self);
	int (*eval_test_result) (MtcLink *self);
	void (*finalize) (MtcLink *self);
} MtcLinkVTable;

struct _MtcLink
{
	//Reference count
	int refcount;
	
	MtcLinkStatus in_status, out_status;
	
	uintptr_t stop_execd, stop_queued;
	MtcLinkStopCB stop_cbs;
	
	const MtcLinkVTable *vtable;
};

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

/**Resumes data transmission stopped by sending MTC_SIGNAL_STOP signal.
 * \param self The link
 */
void mtc_link_resume_out(MtcLink *self);

/**Resumes data reception stopped on receiving MTC_SIGNAL_STOP signal.
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
 * composer after mtc_link_queue_msg() so that the message  is freed
 * as soon as the message is sent.
 * \param self The link
 * \param dest Identifier of the destination where to route the message after
 *             reception on the other end
 * \param reply_to Identifier of the destination where response to the message 
 *                 might be received. 
 * \param msg The message to send.
 */
void mtc_link_queue_msg
	(MtcLink *self, uint64_t dest, uint64_t reply_to, MtcMsg *msg);

/**Schedules a signal to be sent through the link.
 * \param self The link
 * \param dest Identifier of the destination where to route the message after
 *             reception on the other end
 * \param reply_to Identifier of the destination where response to the message 
 *                 might be received. 
 * \param signum The signal
 */
void mtc_link_queue_signal
	(MtcLink *self, uint64_t dest, uint64_t reply_to, uint32_t signum);

/**Determines whether mtc_link_send will try to send any data.
 * This can be useful before using select() or poll() functions.
 * \param self The link
 * \return 1 if link has data to send, 0 otherwise
 */
int mtc_link_can_send(MtcLink *self);

/**Tries to send all queued data
 * \param self The link
 * \return Status of this sending operation
 */
MtcLinkIOStatus mtc_link_send(MtcLink *self);

/**Adds a callback structure containing a callback function to be 
 * called when link stops due to last queued MTC_SIGNAL_STOP signal.
 * 
 * The callback function is called only once, only for the 
 * MTC_SIGNAL_STOP signal that was queued just before the callback 
 * is added. After calling, the callback structure is removed 
 * automatically.
 * 
 * \param self The link
 * \param cb The callback structure to add
 */
void mtc_link_add_stop_cb(MtcLink *self, MtcLinkStopCB *cb);

/**Removes the callback structure from associated link so that callback 
 * function will not be called.
 * \param cb The callback structure
 */
void mtc_link_stop_cb_remove(MtcLinkStopCB *cb);

/**Tries to receive a message or a signal.
 * \param self The link
 * \param data Return location for received message or signal
 * \return Status of this receiving operation
 */
MtcLinkIOStatus mtc_link_receive(MtcLink *self, MtcLinkInData *data);

/**Gets the tests for event loop integration
 * \param self The link
 * \return A linked list of tests to be performed. The test results
 *         should also be written to same memory.
 */
MtcLinkTest *mtc_link_get_tests(MtcLink *self);

/**Evaluates the results of the tests.
 * \param self The link
 * \return Flags specifying the consensus of the tests. 
 *          (See MtcLinkTestAction)
 */
int mtc_link_eval_test_result(MtcLink *self);

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

//Creates a new link.
MtcLink *mtc_link_create(size_t size, const MtcLinkVTable *vtable);

/**Creates a new link that works with file descriptors.
 * \param in_fd The file descriptor to receive from.
 * \param out_fd The file descriptor to send to. Can be as
 *               same as in_fd.
 * \return A new link.
 */
MtcLink *mtc_fd_link_new(int out_fd, int in_fd);

/**Gets the file descriptor used for sending.
 * \param link The link
 * \return a file descriptor
 */
int mtc_fd_link_get_out_fd(MtcLink *link);

/**Gets the file descriptor used for receiving.
 * \param link The link
 * \return a file descriptor
 */
int mtc_fd_link_get_in_fd(MtcLink *link);

/**Gets whether the file descriptors will be closed when the link 
 * is destroyed.
 * \param link The link
 * \return 1 if the file descriptor should be closed on destruction, 
 *         0 otherwise.
 */
int mtc_fd_link_get_close_fd(MtcLink *link);

/**Sets whether the file descriptors will be closed when the link 
 * is destroyed.
 * \param link The link
 * \param val 1 if the file descriptor should be closed on destruction, 
 *            0 otherwise.
 */
void mtc_fd_link_set_close_fd(MtcLink *link, int val);


///\}
