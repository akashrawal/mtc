/* router.c
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

#include "common.h"

//TODO: memcmp/memcpy vs loop debate?

//TODO: static and dynamic addressing interface?

//Router

//Dynamic destinations submodule

typedef struct
{
	unsigned int key;
	unsigned char id[];
} MtcDynamicAddr;

#define MTC_DYNAMIC_ADDR(addr) ((MtcDynamicAddr *) (addr))

#define mtc_dynamic_len(router) \
	(sizeof(MtcDynamicAddr) + (router)->dynamic_len);

#define counter ((unsigned char *) (router->dynamic_counter))

static void mtc_router_init_dynamic(MtcRouter *router)
{
	router->dynamic_counter = mtc_alloc(1);
	router->dynamic_len = 1;
	counter[0] = 0;
	
	mtc_afl_init(&(router->dests));
}

static void mtc_router_add_dynamic_dest
	(MtcRouter *router, MtcDest *dest)
{
	MtcDynamicAddr *dyn_addr;
	int i;
	unsigned int key;
	
	key = mtc_afl_insert(&(router->dests), (MtcAflItem *) dest);
	
	//Set address
	dyn_addr = (MtcDynamicAddr *) dest->addr_data;
	dyn_addr->key = key;
	memcpy(dyn_addr->id, router->dynamic_counter);
	
	//Increment counter
	for (i = 0; i < router->dynamic_len; i++)
	{
		if (counter[i] < 255)
			break;
		counter[i] = 0;
	}
	
	if (i == router->dynamic_len)
	{
		router->dynamic_len++;
		router->dynamic_counter = mtc_realloc
			((void *) counter, router->dynamic_len);
		counter[i] = 0;
	}
	
	counter[i]++;
}

static void mtc_router_remove_dynamic(MtcRouter *router, MtcDest *dest)
{
	mtc_afl_remove(dest->parent.key);
}

static void mtc_router_free_dynamic(MtcRouter *router)
{
	//TODO: Consider assertion for remaining dests?
	mtc_afl_destroy(&(router->dests));
	mtc_free(router->dynamic_counter);
}

#undef counter

//Static destinations submodule
static void mtc_router_init_static(MtcRouter *router)
{
	int i;
	
	router->static_dests_len = 4;
	router->static_dests = (MtcDest *) mtc_alloc
		(sizeof(void *) * router->static_dests_len);
		
	for (i = 0; i < router->static_dests_len; i++)
	{
		router->static_dests[i] = NULL;
	}
}

static void mtc_router_add_static_dest
	(MtcRouter *router, MtcDest *dest, int static_id)
{
	//Enlarge router->static_dests if neccessary
	if (static_id >= router->static_dests_len)
	{
		int i, new_len;
		
		new_len = static_id * 2;
		if (new_len > mtc_static_id_max)
			new_len = mtc_static_id_max;
		
		router->static_dests = (MtcDest *) mtc_realloc
			(router->static_dests, sizeof(void *) * new_len);
		
		for (i = router->static_dests_len; i < new_len; i++)
		{
			router->static_dests[i] = NULL;
		}
		
		router->static_dests_len = new_len;
	}
	
	//Add
	if (router->static_dests[static_id])
		mtc_error("static_id %d is already occupied", static_id);
	router->static_dests[static_id] = dest;
	
	//Set address
	((unsigned char *) dest->addr_data)[0] = static_id;
}

static void mtc_router_remove_static(MtcRouter *router, MtcDest *dest)
{
	int static_id = mtc_dest_get_static_id(dest);
	
	router->static_dests[static_id] = NULL;
}

static void mtc_router_free_static(MtcRouter *router)
{
	//TODO: Consider adding an assertion for destinations still
	//remaining?
	mtc_free(router->static_dests);
}

MtcRouter *mtc_router_create
	(size_t struct_size, MtcRouterVTable *vtable)
{
	MtcRouter *router;
	
	if (struct_size < sizeof(MtcRouter))
		mtc_error("struct_size is smaller than sizeof(MtcRouter)");
	
	router = (MtcRouter *) mtc_alloc(struct_size);
	
	router->refcount = 1;
	router->vtable = vtable;
	router->mgr = NULL;
	mtc_router_init_dynamic_counter(router);
	mtc_router_init_static(router);
	
	return router;
}

void mtc_router_ref(MtcRouter *router)
{
	router->refcount++;
}

void mtc_router_unref(MtcRouter *router)
{
	router->refcount--;
	
	if (router->refcount <= 0)
	{
		(* router->vtable->destroy) (router);
		
		if (router->mgr)
			mtc_event_mgr_unref(router->mgr);
			
		mtc_router_free_dynamic(router);
		mtc_router_free_static(router);
		
		free(router);
	}
}

void mtc_router_set_event_mgr(MtcRouter *router, MtcEventMgr *mgr)
{
	(* router->vtable->set_event_mgr)(router, mgr);
	
	if (router->mgr)
		mtc_event_mgr_unref(router->mgr);
	mtc_event_mgr_ref(mgr);
	router->mgr = mgr;
}

void mtc_router_sync_io_step(MtcRouter *router, MtcPeer *peer)
{
	(* router->sync_io_step)(router, peer);
}

void mtc_router_deliver(MtcRouter *router, 
	void *dest_addr_data, size_t dest_addr_len,
	MtcPeer *src, void *ret_addr_data, size_t ret_addr_len, 
	MtcMsg *payload)
{
	MtcDest *dest = NULL;
	
	//Find destination
	if (dest_addr_len == 1)
	{
		//Static destination
		int static_id = ((unsigned char *) dest_addr_data)[0];
		
		if (static_id < router->static_dests_len)
		{
			dest = router->static_dests[static_id];
		}
	}
	else if (dest_addr_len > sizeof(MtcDynamicAddr))
	{
		//Dynamic destinations
		MtcDynamicAddr *dyn_addr = (MtcDynamicAddr *) dest_addr_data;
		MtcDest *x = (MtcDest *) mtc_afl_lookup
			(&(router->dests), dyn_addr->key);
		
		if (x)
		{
			if (dest_addr_len == x->addr_len)
			{
				if (memcmp(dest_addr_data, x->addr_data, 
					dest_addr_len) == 0)
				{
					dest = x;
				}
			}
		}
	}
	
	//If destination not found, send error
	if (! dest)
	{
		if (ret_addr_len)
		{
			MtcMsg *error_payload = mtc_msg_with_member_ptr_only
				(MTC_MEMBER_PTR_ERROR | MTC_ERROR_INVALID_DEST);
			
			mtc_router_sendto(router, 
				ret_addr_data, ret_addr_len, NULL, error_payload);
			
			mtc_msg_unref(error_payload);
		}
		
		return;
	}
	
	//Deliver
	(* dest->vtable->msg)
		(dest, src, ret_addr_data, ret_addr_len, payload);
}

//Peer


void mtc_peer_init(MtcPeer *peer, MtcRouter *router)
{
	peer->refcount = 1;
	peer->router = router;
	mtc_router_ref(router);
	mtc_ring_init(&(peer->reset_notifys));
}

void mtc_peer_destroy(MtcPeer *peer)
{
	mtc_router_unref(peer->router);
}


void mtc_peer_ref(MtcPeer *peer)
{
	peer->refcount++;
}

void mtc_peer_unref(MtcPeer *peer)
{
	peer->refcount--;
	
	if (peer->refcount <= 0)
	{
		peer->reset_notifys.next->prev = NULL;
		peer->reset_notifys.prev->next = NULL;
		
		(* peer->router->vtable->peer_destroy)(peer);
	}
}

void mtc_peer_add_reset_notify
	(MtcPeer *peer, MtcPeerResetNotify *notify)
{
	MtcRing *el = (MtcRing *) notify, r = &(peer->reset_notifys);
	
	el->next = r;
	el->prev = r->prev;
	r->prev = el;
	el->prev->next = el;
}

void mtc_peer_reset_notify_remove(MtcPeerResetNotify *notify)
{
	MtcRing *el = (MtcRing *) notify;
	el->prev->next = el->next;
	el->next->prev = el->prev;
}

void mtc_peer_reset(MtcPeer *peer)
{
	MtcRing *el, r = &(peer->reset_notifys), r2_mem;
	MtcRing *r2 = &r2_mem;
	MtcPeerResetNotify *notify;
	
	*r2 = *r;
	r2->prev->next = r2;
	r2->next->prev = r2;
	
	while (r2->next != r2)
	{
		el = r2->next;
		el->prev->next = el->next;
		el->next->prev = el->prev;
		el->next = el;
		el->prev = el;
		notify = (MtcPeerResetNotify *) el;
		
		(* notify->cb)(notify);
	}
}

void mtc_peer_sendto(MtcPeer *peer, 
		void *addr_data, size_t addr_len,
		MtcDest *reply_dest, MtcMsg *payload)
{
	(* peer->router->peer_sendto)
		(peer, addr_data, addr_len, reply_dest, payload);
}


//Addresses

void mtc_addr_ref(MtcAddr *addr)
{
	addr->refcount++;
}

void mtc_addr_unref(MtcAddr *addr)
{
	addr->refcount--;
	
	if (addr->refcount <= 0)
	{
		if (addr->peer)
			mtc_peer_unref(addr->peer);
		free(addr);
	}
}

MtcAddr *mtc_addr_new_static(MtcPeer *peer, int static_id)
{
	MtcAddr *addr;
	
	addr = (MtcAddr *) malloc(sizeof(MtcAddr) + 1);
	addr->refcount = 1;
	addr->peer = peer;
	addr->len = 1;
	addr->data[0] = static_id;
	if (peer)
		mtc_peer_ref(peer);
		
	return addr;
}

MtcAddr *mtc_addr_new_from_raw
	(MtcPeer *peer, void *addr_data, size_t addr_len)
{
	MtcAddr *addr;
	
	addr = (MtcAddr *) malloc(sizeof(MtcAddr) + addr_len);
	addr->refcount = 1;
	addr->peer = peer;
	addr->len = addr_len;
	memcpy(addr->data, addr_data, addr_len);
	if (peer)
		mtc_peer_ref(peer);
		
	return addr;
}

MtcAddr *mtc_addr_copy(MtcAddr *src_addr)
{
	MtcAddr *addr;
	
	addr = (MtcAddr *) malloc(sizeof(MtcAddr) + src_addr->len);
	addr->refcount = 1;
	addr->peer = src_addr->peer;
	addr->len = src_addr->len;
	memcpy(addr->data, src_addr->data, src_addr->len);
	if (addr->peer)
		mtc_peer_ref(addr->peer);
		
	return addr;
}

int mtc_addr_equal(MtcAddr *addr1, MtcAddr *addr2)
{
	int i;
	
	if (addr1->len != addr2->len)
		return 0;
	
	for (i = 0; i < addr1->len; i++)
	{
		if (addr1->data[i] != addr2->data[i])
			return 0;
	}
	
	return 1;
}

int mtc_addr_equal_raw
	(MtcAddr *addr1, void *addr2_data, size_t addr2_len)
{
	int i;
	
	if (addr1->len != addr2_len)
		return 0;
	
	for (i = 0; i < addr2_len; i++)
	{
		if (addr1->data[i] != ((char *) addr2_data)[i])
			return 0;
	}
	
	return 1;
}

//Destinations

static MtcDest *mtc_dest_create
	(MtcRouter *router, int static_id, size_t size, 
	MtcDestVTable *vtable)
{
	MtcDest *dest;
	void *addr_data;
	size_t addr_len
	
	if (static_id < 0)
	{
		//Dynamic
		
		addr_len = mtc_dynamic_len(router);
		dest = (MtcDest *) mtc_alloc2(size, addr_len, &addr_data);
		
		dest->addr_len = addr_len;
		dest->addr_data = addr_data;
		
		mtc_router_add_dynamic_dest(router, dest);
	}
	else if (static_id < 256)
	{
		//Static
		
		addr_len = 1;
		dest = (MtcDest *) mtc_alloc2(size, addr_len, &addr_data);
		
		dest->addr_len = addr_len;
		dest->addr_data = addr_data;
		
		mtc_router_add_static_dest(router, dest, static_id);
	}
	else
	{
		mtc_error("Invalid static ID %d", static_id);
	}
	
	dest->refcount = 1;
	dest->removed = 0;
	dest->router = router;
	mtc_router_ref(router);
	dest->vtable = vtable;
	
	return dest;
}

void mtc_dest_remove(MtcDest *dest)
{
	if (dest->removed)
		return;
	
	dest->removed = 1;
	
	if (mtc_dest_is_static(dest))
	{
		//Static
		mtc_router_remove_static(dest->router, dest);
	}
	else
	{
		//Dynamic
		mtc_router_remove_dynamic(dest->router, dest);
	}
}

void mtc_dest_ref(MtcDest *dest)
{
	dest->refcount++;
}

void mtc_dest_unref(MtcDest *dest)
{
	dest->refcount--;

	if (dest->refcount <= 0)
	{
		mtc_dest_remove(dest);
		
		(* dest->vtable->destroy)(dest);
		
		mtc_router_unref(dest->router);
		mtc_free(dest);
	}
}

MtcAddr *mtc_dest_copy_addr(MtcDest *dest)
{
	return mtc_addr_new_from_raw
		(NULL, dest->addr_data, dest->addr_len);
}

//Implementer API and binding


//Function call
//TODO: Implement and delete irrelevant

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


//TODO: implement
MtcDestVTable mtc_fc_handle_vtable = 
{
	mtc_fc_handle_msg,
	mtc_fc_handle_destroy
};

MtcFCHandle *mtc_fc_start
	(MtcAddr *addr, MtcFCBinary *binary, void *args)
{
	MtcFCHandle	*handle;
	MtcMsg *payload;
	size_t out_args_offset, size;
	
	//Calculate size
	size = sizeof(MtcFCHandle);
	if (binary->out_args_c_size)
	{
		mtc_align_offset(size);
		out_args_offset = size;
		size += binary->out_args_c_size;
	}
	
	//Create and init handle
	handle = (MtcFCHandle *) mtc_dest_create
		(router, -1, size, mtc_fc_handle_vtable);
	
	handle->peer = addr->peer;
	mtc_peer_ref(addr->peer);
	//TODO: implement
	handle->notify.cb = mtc_fc_handle_peer_reset;
	if (binary->out_args_c_size)
		handle->out_args = MTC_PTR_ADD(handle, out_args_offset);
	else
		handle->out_args = NULL;
	handle->status = MTC_ERROR_TEMP;
	handle->binary = binary;
	handle->cb = handle->cb_data = NULL;
	
	mtc_peer_add_reset_notify(addr->peer, &(handle->notify));
		
	//Send mail
	payload = (* binary->in_args_ser)(args);
	mtc_peer_sendto(addr->peer, addr->data, addr->len, 
		(MtcDest *) handle, payload);
	mtc_msg_unref(payload);
	
	return handle;
}

void mtc_fc_start_unhandled
	(MtcAddr *addr, MtcFCBinary *binary, void *args)
{
	//Send mail
	payload = (* binary->in_args_ser)(args);
	mtc_peer_sendto(addr->peer, addr->data, addr->len, 
		NULL, payload);
	mtc_msg_unref(payload);
}

MtcStatus mtc_fc_finish_sync(MtcFCHandle *handle)
{
	//
}

#define mtc_fc_get_out_args(handle, type) \
	((type *) (((MtcFCHandle *) (handle))->out_args))

