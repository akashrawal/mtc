/* da.h
 * Destination allocator. 
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

//DA

///The destination allocator
typedef struct _MtcDA MtcDA;

/**Initializes the destination allocator
 * \param self Pointer to structure to be initialized
 * \param n_static_objects Number of static objects which will be
 *        added to the destination allocator
 */
void mtc_da_init(MtcDA *self, int n_static_objects);

/**Frees all memory associated with destination allocator
 * \param self The destination allocator
 */
void mtc_da_destroy(MtcDA *self);

//Peer

///Structure representing a peer. 
typedef struct _MtcPeer MtcPeer;

/**Initializes a peer. 
 * \param self Pointer to structure to be initialized
 * \param link A link
 * \param da The destination allocator
 */
void mtc_peer_init(MtcPeer *self, MtcLink *link, MtcDA *da);

///Structure to store callback function and its data
///to be called when link fails
typedef struct _MtcPeerFailNotify MtcPeerFailNotify;
struct _MtcPeerFailNotify
{
	///The callback function to be called when link fails
	void (*cb) (void *data);
	///The data to be passed to callback function
	void *data;
	
	MtcPeerFailNotify *prev;
	MtcPeerFailNotify *next;
};

/**Adds a function to be called when link connected to the peer fails
 * \param self The peer
 * \param notify Pointer to the structure that will contain 
 *        callback function. This pointer should be valid until
 *        removed by mtc_peer_remove_fail_notify(). 
 */
void mtc_peer_add_fail_notify(MtcPeer *self, MtcPeerFailNotify *notify);

/**Removes the callback function added by mtc_peer_add_fail_notify().
 * \param self The peer
 * \param notify Pointer to the structure that contained
 *        callback function.
 */
void mtc_peer_remove_fail_notify
	(MtcPeer *self, MtcPeerFailNotify *notify);

/**Calls and removes all callback functions registered using
 * mtc_peer_add_fail_notify(). Should be called when underlying link
 * fails. 
 * \param self The peer
 */
void mtc_peer_raise_fail(MtcPeer *self);

/**Frees all resources associated with the peer.
 * \param self The peer
 */
void mtc_peer_destroy(MtcPeer *self);

//Generic destination (private)
typedef enum
{
	MTC_DEST_FC_HANDLE,
	MTC_DEST_EVT_HANDLE,
	MTC_DEST_OBJECT_HANDLE,
	MTC_DEST_EVT_PIN
} MtcDestType;

#define MTC_DEST_DYNAMIC_BIT (((uint64_t) 1) << 63)
 
typedef struct
{
	MtcAflItem parent;
	MtcDestType type;
	uint64_t dest_id;
	MtcPeer *peer;
} MtcDest;

//Event data structures on server side (private)
typedef struct _MtcEventPin MtcEventPin;
struct _MtcEventPin
{
	MtcDest parent;
	
	uint64_t handle_id;
	MtcEventPin *event_next, *event_prev;
	MtcEventPin *peer_next, *peer_prev;
};

#define MTC_EVENT_STORE_MAX_PINS 256

typedef struct _MtcEventStore MtcEventStore;
struct _MtcEventStore
{
	MtcEventStore *next;
	MtcEventPin pins[MTC_EVENT_STORE_MAX_PINS];
};

//Function call in client side

///Status code for a remote function call
typedef enum
{
	///Nothing happened.
	MTC_FC_NONE = 0,
	///Temporary failure. Alias for MTC_FC_NONE.
	MTC_FC_TEMP_FAIL = 0,
	///Link has been stopped by MTC_SIGNAL_STOP signal.
	MTC_FC_LINK_STOP = 1,
	
	MTC_FC_STATUS_MAX_RESET = 2,
	
	///Function call failed because link has failed.
	MTC_FC_FAIL_LINK = 3,
	///Function call failed because of peer side error.
	MTC_FC_FAIL_CODE = 4,
	///Function call finished successfully.
	MTC_FC_SUCCESS = 5
} MtcFCStatus;

/**Prototype of the function to be called when something happens 
 * related to the remote function call.
 * \param status Status code of the remote function call
 * \param reply Message containing out paramaters if they are there
 *              and function call completed successfully, 
 *              else NULL
 * \param data User specified data
 */
typedef void (*MtcFCReturnCB) 
	(MtcFCStatus status, MtcMsg *reply, void *data);

///Structure representing a remote function call handle
typedef struct
{
	MtcDest parent;
	int msg_in_reply;
	MtcPeerFailNotify notify;
	
	///Current status 
	MtcFCStatus status;
	///Message containing out paramaters if function call succeeds,
	///or NULL if a callback function is set
	MtcMsg *reply;
	
	///Callback function, else NULL
	MtcFCReturnCB cb;
	///Data for the callback function
	void *data;
} MtcFCHandle;

/**Gets destination ID for function call handle. 
 * \param handle Pointer to the function call handle (MtcFCHandle *)
 * \return Destination ID (uint64_t)
 */
#define mtc_fc_handle_dest_id(handle) ((handle)->parent.dest_id)

/**Starts a remote function call
 * \param self The peer where the object is hosted
 * \param dest_id The destination ID of the object
 * \param msg Message containing in paramaters of function call
 * \param handle Structure to keep track of the function call.
 *               The structure must exist till function call
 *               fails or succeeds or is cancelled.
 */
void mtc_peer_start_fc
	(MtcPeer *self, uint64_t dest_id, MtcMsg *msg, MtcFCHandle *handle);

/**Tries to synchronously finish the function call, assuming that
 * underlying link is not on nonblocking mode. This function blocks
 * till function call is completed.
 * \param handle The function call handle
 * \return The status of the function call
 */
MtcFCStatus mtc_fc_handle_finish(MtcFCHandle *handle);

/**Cancels the remote function call.
 * \param handle The function call handle
 */
void mtc_fc_handle_cancel(MtcFCHandle *handle);

//Event handling on client side

///Status code for event connetion.
typedef enum 
{
	///Event connection broke because link has failed.
	MTC_EVT_FAIL_LINK = 1,
	///Event connection broke because of peer side error.
	MTC_EVT_FAIL_CODE = 2,
	///Link has been stopped by MTC_SIGNAL_STOP signal.
	MTC_EVT_LINK_STOP = 3,
	///Event has been raised by object.
	MTC_EVT_RAISE = 4
} MtcEventStatus;

/**Type of function to be called if anything related to the event
 * connection happens
 * \param status The status code
 * \param reply The paramaters of the event if any, else NULL
 * \param data User specified data
 */
typedef void (*MtcEventCB)
	(MtcEventStatus status, MtcMsg *reply, void *data);

///Structure representing event connection handle
typedef struct 
{
	MtcDest parent;
	int msg_in_reply;
	MtcPeerFailNotify notify;
	
	uint64_t disconnect_id;
	
	///Callback function
	MtcEventCB cb;
	///Data to be passed to the callback function
	void *data;
} MtcEventHandle;

/**Gets destination ID for event connection handle. 
 * \param handle Pointer to the event handle (MtcEventHandle *)
 * \return Destination ID (uint64_t)
 */
#define mtc_event_handle_dest_id(handle) ((handle)->parent.dest_id)

/**Sets up a connection to an event.
 * \param self The peer where the object is located
 * \param dest_id Destination ID of the object
 * \param tag Event tag of the event to connect
 * \param handle Structure to keep track of the event.
 *               The structure must exist till event is disconnected.
 */
void mtc_peer_connect_event
	(MtcPeer *self, uint64_t dest_id, uint32_t tag,
	MtcEventHandle *handle);

/**Disconnects the event. 
 * \param handle The event handle
 */
void mtc_event_handle_disconnect(MtcEventHandle *handle);

//Object interface

///Structure representing an object handle
typedef struct _MtcObjectHandle MtcObjectHandle;

/**Gets destination ID for object handle. 
 * \param handle Object handle handle (MtcObjectHandle *)
 * \return Destination ID (uint64_t)
 */
#define mtc_object_handle_dest_id(handle) ((handle)->parent.dest_id)

/**Gets user-defined data associated with the object handle.
 * \param handle Object handle handle (MtcObjectHandle *)
 * \return Data associated with the object (void *)
 */
#define mtc_object_handle_data(handle) ((handle)->data)

/**Type of function to be supplied as implementation of a function
 * call.
 * \param handle The object handle
 * \param peer The peer which made the function call
 * \param reply_to Destination number to send reply
 * \param msg Message containing function arguments.
 */
typedef void (*MtcFnImpl) 
	(MtcObjectHandle *handle, MtcPeer *peer, uint64_t reply_to, 
	MtcMsg *msg);

/**Creates a new object handle
 * \param da The destination allocator
 * \param static_id The static destination ID if this object should have
 *        a static destination ID, else -1
 * \param object_data Data to associate with the object
 * \param n_fns Number of functions the object has. 
 * \param n_evts Number of events the object has.
 * \param fn_impls An array of function implementations.
 * \return A new object handle
 */
MtcObjectHandle *mtc_object_handle_new
	(MtcDA *da, int static_id, void *object_data,
	MtcClassInfo info, MtcFnImpl *fn_impls);

/**Destroys an object handle
 * \param self An object handle
 */
void mtc_object_handle_destroy(MtcObjectHandle *self);

/**Raises an event of the object
 * \param self An object handle
 * \param tag Event tag of the event to raise
 * \param msg Message containing event paramaters if any or NULL
 */
void mtc_object_handle_raise_event
	(MtcObjectHandle *self, uint32_t tag, MtcMsg *msg);

//Routing

/**Do appropriate thing to received message/signal.
 * \param self The destination allocator
 * \param peer The peer from where the message or signal was received
 * \param data The message or signal received.
 */
void mtc_da_route(MtcDA *self, MtcPeer *peer, MtcLinkInData *data);


//Structure definitions
struct _MtcObjectHandle
{
	MtcDest parent;
	MtcDA *da;
	void *object_data;
	MtcClassInfo info;
	MtcFnImpl *fns;
	MtcEventPin event_rings[];
};

struct _MtcDA
{
	MtcPeer *peers;
	MtcObjectHandle **objects;
	int n_objects;
	uint32_t version;
	MtcAfl dests;
	MtcEventStore *pin_store;
	int n_pins;
};

struct _MtcPeer
{
	///The destination allocator
	MtcDA *da;
	///The link the peer is connected over
	MtcLink *link;
	
	MtcPeerFailNotify *fail_cbs;
	MtcEventPin peer_ring;
};
