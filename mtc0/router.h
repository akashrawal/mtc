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
 
typedef struct _MtcRing MtcRing;

struct _MtcRing
{
	MtcRing *prev, *next;
};

#define mtc_ring_init(ring_ptr) \
do {\
	MtcRing *cast_ring_ptr = (MtcRing *) (ring_ptr); \
	cast_ring_ptr->prev = cast_ring_ptr->next = cast_ring_ptr; \
} while(0)

//User API

//TODO: Implement functions
//TODO: Docs
//Router
typedef struct _MtcRouter MtcRouter;

void mtc_router_ref(MtcRouter *router);

void mtc_router_unref(MtcRouter *router);

void mtc_router_set_event_mgr(MtcRouter *router, MtcEventMgr *mgr);

//MtcEventMgr *mtc_router_get_event_mgr(MtcRouter *router);
#define mtc_router_get_event_mgr(router) \
	((MtcEventMgr *) (((MtcRouter *) (router))->mgr))

//Peer
typedef struct _MtcPeer MtcPeer;

void mtc_peer_ref(MtcPeer *peer);

void mtc_peer_unref(MtcPeer *peer);

#define mtc_peer_get_router(peer) \
	((MtcRouter *) (((MtcPeer *) (peer))->router))

typedef struct _MtcPeerResetNotify MtcPeerResetNotify;
struct _MtcPeerResetNotify
{
	MtcRing parent;
	
	void (* cb)(MtcPeerResetNotify *notify);
};

void mtc_peer_add_reset_notify
	(MtcPeer *peer, MtcPeerResetNotify *notify);

void mtc_peer_reset_notify_remove(MtcPeerResetNotify *notify);

//Addresses
typedef struct 
{
	int refcount;
//public readonly:
	MtcPeer *peer;
	size_t len;
	unsigned char data[];
} MtcAddr;

void mtc_addr_ref(MtcAddr *addr);

void mtc_addr_unref(MtcAddr *addr);

MtcAddr *mtc_addr_new_static(MtcPeer *peer, int static_id);

MtcAddr *mtc_addr_new_from_raw
	(MtcPeer *peer, void *addr_data, size_t addr_len);

MtcAddr *mtc_addr_copy(MtcAddr *src_addr);

int mtc_addr_equal(MtcAddr *addr1, MtcAddr *addr2);

int mtc_addr_equal_raw
	(MtcAddr *addr1, void *addr2_data, size_t addr2_len);

#define mtc_static_id_max (255)

#define mtc_addr_is_static(addr) \
	((int) (((MtcAddr *) (addr))->len == 1))

#define mtc_addr_get_static_id(addr) \
	((int) (((MtcAddr *) (addr))->data[0]))

//Destinations
typedef struct _MtcDest MtcDest;

typedef struct 
{
	void (*msg) (MtcDest *dest, 
		MtcPeer *src, void *ret_addr_data, size_t ret_addr_len, 
		MtcMsg *payload);
	void (*destroy) (MtcDest *dest);
} MtcDestVTable;

struct _MtcDest
{
	MtcAflItem parent;
	
	int refcount;
	int removed;
	MtcRouter *router;
	MtcDestVTable *vtable;
	size_t addr_len;
	void *addr_data;
};

void mtc_dest_ref(MtcDest *dest);

void mtc_dest_unref(MtcDest *dest);

#define mtc_dest_get_router(dest) \
	((MtcRouter *) (((MtcDest *) (dest))->router))

void mtc_dest_remove(MtcDest *dest);

#define mtc_dest_get_removed(dest) \
	((int) (((MtcDest *) (dest))->removed))

MtcAddr *mtc_dest_copy_addr(MtcDest *dest);

#define mtc_dest_get_addr(dest) 
	((void *) (((MtcDest *) (dest))->addr_data))

#define mtc_dest_get_addr_len(dest) 
	((size_t) (((MtcDest *) (dest))->addr_len))

#define mtc_dest_is_static(dest) \
	((int) (((MtcDest *) (dest))->addr_len == 1))

#define mtc_dest_get_static_id(dest) \
	((int) (((unsigned char *) (((MtcDest *) (dest))->addr_data))[0]))

//Implementer API

typedef struct
{
	void (*set_event_mgr) (MtcRouter *router, MtcEventMgr *mgr);
	void (*peer_sendto) (MtcPeer *peer, 
		void *addr_data, size_t addr_len,
		MtcDest *reply_dest, MtcMsg *payload);
	void (*sync_io_step)(MtcRouter *router, MtcPeer *peer);
	void (*peer_destroy)(MtcPeer *peer);
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
	MtcDest *static_dests;
	int static_dests_len;
};

MtcRouter *mtc_router_create
	(size_t struct_size, MtcRouterVTable *vtable);

void mtc_router_sync_io_step(MtcRouter *router, MtcPeer *peer);

void mtc_router_deliver(MtcRouter *router, 
	void *dest_addr_data, size_t dest_addr_len,
	MtcPeer *src, void *ret_addr_data, size_t ret_addr_len, 
	MtcMsg *payload);
	
struct _MtcPeer
{
	int refcount;
	MtcRouter *router;
	MtcRing reset_notifys;
};

void mtc_peer_init(MtcPeer *peer, MtcRouter *router);

void mtc_peer_destroy(MtcPeer *peer);
	
void mtc_peer_reset(MtcPeer *peer);

void mtc_peer_sendto(MtcPeer *peer, 
		void *addr_data, size_t addr_len,
		MtcDest *reply_dest, MtcMsg *payload);

//Function call

typedef struct _MtcFCHandle MtcFCHandle;

typedef void (*MtcFCFn) 
	(MtcFCHandle *handle, MtcStatus status, void *data);

struct _MtcFCHandle
{
	MtcDest parent;
	
	MtcPeer *peer;
	MtcPeerResetNotify notify;
	void *out_args;
	MtcStatus status;
	MtcFCBinary *binary;
	
	MtcFCFn cb;
	void *cb_data;
};

MtcFCHandle *mtc_fc_start
	(MtcAddr *addr, MtcFCBinary *binary, void *args);

void mtc_fc_start_unhandled
	(MtcAddr *addr, MtcFCBinary *binary, void *args);

MtcStatus mtc_fc_finish_sync(MtcFCHandle *handle);

#define mtc_fc_get_status(handle) \
	((MtcStatus) (((MtcFCHandle *) (handle))->status))

#define mtc_fc_get_out_args(handle, type) \
	((type *) (((MtcFCHandle *) (handle))->out_args))

//Event management

typedef struct _MtcEventHandle MtcEventHandle;

typedef void (*MtcEventFn) 
	(MtcEventHandle *handle, MtcStatus status, void *data);

struct _MtcEventHandle
{
	MtcDest parent;
	
	MtcAddr *addr;
	MtcPeerResetNotify notify;
	void *args;
	MtcStatus status;
	MtcEventBinary *binary;
	
	MtcEventFn cb;
	void *cb_data;
};

MtcEventHandle *mtc_event_connect
	(MtcAddr *addr, MtcFCBinary *binary, void *args);

MtcStatus mtc_event_fetch_sync(MtcEventHandle *handle);

#define mtc_event_get_args(handle, type) \
	((type *) (((MtcEventHandle *) (handle))->args))
	
//Object handle

typedef struct _MtcObjectHandle MtcObjectHandle;

typedef void (*MtcFnImpl) 
	(MtcObjectHandle *handle, MtcAddr *return_addr, void *args, 
		void *impl_data);

struct _MtcObjectHandle
{
	MtcDest parent;
	
	MtcClassBinary *binary;
	MtcFnImpl *impl;
	void *impl_data;
	MtcRing event_rings[];
};

typedef struct _MtcEventConnection MtcEventConnection;

struct _MtcEventConnection
{
	MtcDest parent;
	
	MtcRing event_ring;
	MtcPeer *peer;
	MtcPeerResetNotify notify;
	size_t addr_len;
	char addr[];
}

MtcObjectHandle *mtc_object_handle_new
	(MtcRouter *router, MtcClassBinary *binary, int static_id,
	MtcFnImpl *impl, void *impl_data);

void mtc_object_handle_raise_event
	(MtcObjectHandle *handle, int id, void *args);


