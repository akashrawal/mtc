/* router.h
 * Distributed object system abstraction
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
 * \addtogroup mtc_router
 * \{
 * 
 * A router is an abstract object responsible to direct messages to 
 * correct peers and correct destinations. 
 * 
 * A peer is an abstract object representing the entity that hosts 
 * an object. (e.g. a remote process)
 * 
 * A destination is an entity having an address that can receive 
 * messages. 
 * 
 * An address is block of memory whose contents uniquely identify
 * a destination.  There are two types of addresses 
 * (and consequently destinations):
 * - Static addresses: These are just one byte long. A user can get any
 *   static addess at will. They are generally used for objects 
 *   surviving for the lifetime of the process. 
 * - Dynamic addresses: These are more than one byte long. They
 *   are only assignable by the router. Their format is also opaque
 *   and should be considered implementation detail. They are used 
 *   for objects and function call handles created aritrarily.
 *   The only guarantee is that address once assigned is never 
 *   reused again.
 * 
 * Using these concepts a distributed object system is implemented 
 * here.
 * 
 */
 
//MDLC

//The first 4 bytes is an integer referring to a function or event.
//Called member pointer

#define MTC_MEMBER_PTR_NULL               ((uint32_t) 0)
#define MTC_MEMBER_PTR_FN                 ((uint32_t) 1L << 28)
#define MTC_MEMBER_PTR_FN_RETURN          ((uint32_t) 2L << 28)
#define MTC_MEMBER_PTR_ERROR              ((uint32_t) 3L << 28)
#define MTC_MEMBER_PTR_MASK               ((uint32_t) 15L << 28)

#define MTC_MEMBER_PTR_IS_FN(ptr) \
	(((ptr) & MTC_MEMBER_PTR_MASK) == MTC_MEMBER_PTR_FN)

#define MTC_MEMBER_PTR_IS_ERROR(ptr) \
	(((ptr) & MTC_MEMBER_PTR_MASK) == MTC_MEMBER_PTR_ERROR)

#define MTC_MEMBER_PTR_GET_IDX(ptr) \
	((ptr) & (~ MTC_MEMBER_PTR_MASK))
	
///Status codes used in distributed object system
typedef enum
{	
	MTC_ERROR_TEMP = -1,
	MTC_FC_SUCCESS = -2,
	
	MTC_ERROR_INVALID_PEER = 1,
	MTC_ERROR_PEER_RESET = 2,
	MTC_ERROR_INVALID_ARGUMENT = 3
} MtcStatus;

uint32_t mtc_msg_read_member_ptr(MtcMsg *msg);

MtcMsg *mtc_msg_with_member_ptr_only(uint32_t member_ptr);

typedef MtcMsg *(*MtcSerFn) (void *strx);
typedef int (*MtcDeserFn)   (MtcMsg *msg, void *strx);
typedef void (*MtcFreeFn)   (void *strx);

typedef struct 
{
	int        id;
	size_t     in_args_c_size;
	MtcSerFn   in_args_ser;
	MtcDeserFn in_args_deser;
	MtcFreeFn  in_args_free;
	size_t     out_args_c_size;
	MtcSerFn   out_args_ser;
	MtcDeserFn out_args_deser;
	MtcFreeFn  out_args_free;
} MtcFCBinary;

typedef struct 
{
	int             n_fns;
	MtcFCBinary     *fns;
} MtcClassBinary;

//User API

//TODO: Section documentation

//Objects

///Structure representing a router.
typedef struct _MtcRouter MtcRouter;

///Structure representing a peer
typedef struct _MtcPeer MtcPeer;

///Structure representing a destination
typedef struct _MtcDest MtcDest;

//Router

/**Increments the reference count by 1.
 * \param router A router
 */
void mtc_router_ref(MtcRouter *router);

/**Decrements the reference count by 1.
 * \param router A router
 */
void mtc_router_unref(MtcRouter *router);

/**Sets an event manager to use to perform event-driven IO
 * \param router A router
 * \param mgr An event manager, or NULL to unset current one
 */
void mtc_router_set_event_mgr(MtcRouter *router, MtcEventMgr *mgr);

/**Gets the event manager used to perform event-driven IO
 * \param router A router
 * \return The event manager used, or NULL if none used
 */
#define mtc_router_get_event_mgr(router) \
	((MtcEventMgr *) (((MtcRouter *) (router))->mgr))

//Peer
/**Increments the reference count by 1.
 * \param peer A peer
 */
void mtc_peer_ref(MtcPeer *peer);

/**Decrements the reference count by 1.
 * \param peer A peer
 */
void mtc_peer_unref(MtcPeer *peer);

/**Gets the router the peer holds reference to.
 * \param peer A peer
 * \return Associated router
 */
#define mtc_peer_get_router(peer) \
	((MtcRouter *) (((MtcPeer *) (peer))->router))

/**Structure to track reliability of a peer.
 */
typedef struct _MtcPeerResetNotify MtcPeerResetNotify;
struct _MtcPeerResetNotify
{
	MtcRing parent;
	
	///Function to be called when a message fails/may fail
	///to reach the peer.
	void (* cb)(MtcPeerResetNotify *notify);
};

/**Adds a callback to track reliability of a peer.
 * The callback is automatically removed once it is triggered. 
 * Or it can be removed by mtc_peer_reset_notify_remove().
 * \param peer A peer
 * \param notify A callback structure. Callback function must be
 * set and the structure must survive until removed by 
 * mtc_peer_reset_notify_remove() or the callback is triggered.
 */
void mtc_peer_add_reset_notify
	(MtcPeer *peer, MtcPeerResetNotify *notify);

/**Removes callback structure added previously via
 * mtc_peer_add_reset_notify(). If callback is already removed, 
 * it does nothing. 
 * \param notify A callback structure previously added via 
 * 			mtc_peer_add_reset_notify()
 */
void mtc_peer_reset_notify_remove(MtcPeerResetNotify *notify);

//Addresses

/**Largest integer that can be used as static address.
 */
#define mtc_static_id_max (255)

/**Create a new static address
 * \param static_id Uniquely identifies a static address
 * \return A reference counted memory containing a static address
 */
MtcMBlock mtc_addr_new_static(int static_id);

/**Structure explaining format of a static address
 */
typedef struct
{
	///Uniquely identifies a static address
	unsigned char static_id;
} MtcStaticAddr;

//Destinations

typedef struct 
{
	void (*msg) (MtcDest *dest, 
		MtcPeer *src, MtcMBlock ret_addr, MtcMsg *payload);
	void (*removed) (MtcDest *dest);
	void (*destroy) (MtcDest *dest);
} MtcDestVTable;

struct _MtcDest
{
	MtcAflItem parent;
	
	MtcMBlock addr;
	
	MtcRouter *router;
	MtcDestVTable *vtable;
	int refcount;
	int removed;
};

/**Increment reference count by 1.
 * \param dest A destination
 */
void mtc_dest_ref(MtcDest *dest);

/**Decrement reference count by 1
 * \param dest A destination
 */
void mtc_dest_unref(MtcDest *dest);

/**Gets the router the destination holds reference to.
 * \param dest A destination
 * \return Associated router
 */
#define mtc_dest_get_router(dest) \
	((MtcRouter *) (((MtcDest *) (dest))->router))

/**Removes a destination from the router, so that it does not receive
 * any messages.
 * \param dest A destination
 */
void mtc_dest_remove(MtcDest *dest);

/**Determines if a destination has been removed from its router.
 * \param dest A destination
 */
#define mtc_dest_get_removed(dest) \
	((int) (((MtcDest *) (dest))->removed))

/**Determines the address associated with the destination.
 * \param dest The destination
 */
MtcMBlock mtc_dest_get_addr(MtcDest *dest);

/**Determines if destination is given a static address.
 * \param dest The destination
 * \return Nonzero if destination is assigned a static address.
 */
#define mtc_dest_is_static(dest) \
	(((MtcDest *) (dest))->addr.size == 1)

/**Gets a unique integer that identifies a static address.
 * Do not use this on destinations having dynamic addresses.
 * \param dest The destination
 * \return unique integer that identifies a static address.
 */
#define mtc_dest_get_static_id(dest) \
	((int) (((MtcStaticAddr *) (((MtcDest *) (dest))->addr.mem))->static_id))

//Implementer API

///Value table for implementing a router
typedef struct
{
	///Implementation for mtc_link_set_event_mgr()
	void (*set_event_mgr) (MtcRouter *router, MtcEventMgr *mgr);
	///Function to send a message to peer
	void (*peer_sendto) (MtcPeer *peer, 
		MtcMBlock addr, MtcDest *reply_dest, MtcMsg *payload);
	///Function to synchronously perform IO (preferably) 
	///related to the peer 
	int (*peer_sync_io_step)(MtcPeer *peer);
	///Function to free all resources associated with the peer
	void (*peer_destroy)(MtcPeer *peer);
	///Function to free all resources associated with the router
	void (*destroy)(MtcRouter *router);
} MtcRouterVTable;

struct _MtcRouter
{
	int refcount;
	MtcRouterVTable *vtable;
	MtcEventMgr *mgr;
	MtcAfl dests;
	char *dynamic_counter;
	int dynamic_len;
	MtcDest **static_dests;
	int static_dests_len;
};

struct _MtcPeer
{
	int refcount;
	MtcRouter *router;
	MtcRing reset_notifys;
};

/**Allocates a structure for a router.
 * \param struct_size Size of the router structure. Must be atleast 
 * 			sizeof(MtcRouter).
 * \param vtable A structure containing implementation functions
 */
MtcRouter *mtc_router_create
	(size_t struct_size, MtcRouterVTable *vtable);

/**Delivers a message to its intended destination
 * \param router A router
 * \param addr Address to deliver the message to
 * \param src The peer the message came from
 * \param ret_addr Address where reply should be sent to src
 * \param payload Message to be delivered
 */
void mtc_router_deliver(MtcRouter *router, 	MtcMBlock addr,
	MtcPeer *src, MtcMBlock ret_addr, MtcMsg *payload);

/**Peer's constructor.
 * You must call this on a newly allocated peer structure before
 * doing your own initialization.
 * \param peer Memory to initialize
 * \param router The router to associate. The peer will hold a 
 * 		strong reference to the router.
 */
void mtc_peer_init(MtcPeer *peer, MtcRouter *router);

/**Peer's destructor. 
 * You must call this function in MtcRouterVTable::peer_destroy() 
 * before freeing the peer's memory.
 * \param peer The peer to destroy
 */
void mtc_peer_destroy(MtcPeer *peer);

/**To be called when connection to a peer is no longer reliable.
 * (e.g. a message fails to reach the peer)
 * It calls and removes all callbacks added by 
 * mtc_peer_add_reset_notify().
 * \param peer The peer
 */
void mtc_peer_reset(MtcPeer *peer);

//Functions used by destinations
/* Sends a message to a peer. 
 * \param peer The peer
 * \param addr The address of the destination 
 * \param reply_dest The destination that should receive reply
 * \param payload The message to send
 */
void mtc_peer_sendto(MtcPeer *peer, 
		MtcMBlock addr,
		MtcDest *reply_dest, MtcMsg *payload);

/* Synchronously performs peer-related IO.
 * \param peer The peer
 * \return -1 if there was some error
 */
int mtc_peer_sync_io_step(MtcPeer *peer);

//Function call

/**Function call handle.
 */
typedef struct _MtcFCHandle MtcFCHandle;

/**Type of the function that will be called whenever anything 
 * happens to the function call handle.
 * You should use mtc_fc_get_status() to check the status.
 * \param handle The function call handle
 * \param data Data passed to the callback
 */
typedef void (*MtcFCFn) 
	(MtcFCHandle *handle, void *data);

struct _MtcFCHandle
{
	MtcDest parent;
	
	MtcPeer *peer;
	MtcPeerResetNotify notify;
	void *out_args;
	MtcStatus status;
	MtcFCBinary *binary;
	
	///The callback function
	MtcFCFn cb;
	///Data passed to the callback function
	void *cb_data;
};

/**Starts a function call.
 * \param peer The peer where the object resides
 * \param addr Address assigned to its object handle
 * \param binary Pointer to mdlc generated structure for function call
 *               containing stubs and skeletons
 * \param args Pointer to structure containing function call 
 *            arguments. Structure declaration can be 
 *            generated by mdlc.
 * \return A function call handle.
 */
MtcFCHandle *mtc_fc_start
	(MtcPeer *peer, MtcMBlock addr, MtcFCBinary *binary, void *args);

/**Starts a function call, but without creating a handle.
 * \param peer The peer where the object resides
 * \param addr Address assigned to its object handle
 * \param binary Pointer to mdlc generated structure for function call
 *               containing stubs and skeletons
 * \param args Pointer to structure containing function call 
 *            arguments. Structure declaration can be 
 *            generated by mdlc.
 */
void mtc_fc_start_unhandled
	(MtcPeer *peer, MtcMBlock addr, MtcFCBinary *binary, void *args);

/**Tries to synchronously finish a function call.
 * \param handle A function call handle
 * \return What happened to the function call.
 */
MtcStatus mtc_fc_finish_sync(MtcFCHandle *handle);

/**Gets current status of the function call.
 * \param handle A function call handle
 * \return What happened to the function call.
 */
#define mtc_fc_get_status(handle) \
	((MtcStatus) (((MtcFCHandle *) (handle))->status))

/**Gets output arguments of a finished function call.
 * \param handle A function call handle
 * \param type Type of structure used
 * \return What happened to the function call.
 */
#define mtc_fc_get_out_args(handle, type) \
	((type *) (((MtcFCHandle *) (handle))->out_args))

//Object handle

///Object handle
typedef struct _MtcObjectHandle MtcObjectHandle;

/**Prototype of the function used to implement 
 * object's member functions
 * \param handle The object handle
 * \param src The peer who started function call
 * \param ret_addr Address to send return values to, 
 *          or {NULL, 0} if no reply is expected
 * \param args Pointer to input arguments structure.
 */
typedef void (*MtcFnImpl) 
	(MtcObjectHandle *handle, MtcPeer *src, MtcMBlock ret_addr, 
	void *args);

struct _MtcObjectHandle
{
	MtcDest parent;
	
	MtcClassBinary *binary;
	MtcFnImpl *impl;
	void *impl_data;
};

/**Creates a new object handle
 * \param router A router
 * \param binary Pointer to the class structure (mdlc generated)
 * \param static_id Negative to create dynamic destination,
 *            a number <= mtc_static_id_max for static destination
 * \param impl An array of functions implementing 
 *            all member functions
 * \param impl_data Data for the implementation
 */
MtcObjectHandle *mtc_object_handle_new
	(MtcRouter *router, MtcClassBinary *binary, int static_id,
	MtcFnImpl *impl, void *impl_data);

/**Get implementation data for the object handle
 * \param handle An object handle
 * \return Implementation data passed to mtc_object_handle_new()
 */
#define mtc_object_handle_get_impl_data(handle) \
	((void *) (((MtcObjectHandle *) (handle))->impl_data))

/**Sends return values of a function call back.
 * \param src The peer who started function call
 * \param ret_addr Address to send return values to
 * \param binary Pointer to mdlc generated structure for function call
 *               containing stubs and skeletons
 * \param out_args Pointer to structure containing output arguments
 */
void mtc_fc_return(MtcPeer *src, MtcMBlock ret_addr, MtcFCBinary *binary, 
	void *out_args);

/**
 * \}
 */

