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

//TODO: Trap/logging for debugging invalid/unintended messages?

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
	memcpy(dyn_addr->id, router->dynamic_counter, router->dynamic_len);
	
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
	mtc_afl_remove(&(router->dests), dest->parent.key);
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
	router->static_dests = (MtcDest **) mtc_alloc
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
		
		router->static_dests = (MtcDest **) mtc_realloc
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
	mtc_router_init_dynamic(router);
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
	if (mgr)
		mtc_event_mgr_ref(mgr);
	router->mgr = mgr;
}

void mtc_router_sync_io_step(MtcRouter *router, MtcPeer *peer)
{
	(* router->vtable->sync_io_step)(router, peer);
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
				(MTC_MEMBER_PTR_ERROR | MTC_ERROR_INVALID_ARGUMENT);
			
			mtc_peer_sendto(src, 
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
	MtcRing *el = (MtcRing *) notify, *r = &(peer->reset_notifys);
	
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
	MtcRing *el, *r = &(peer->reset_notifys), r2_mem;
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
	if (! addr_len)
		mtc_error("Attempted to send to a null address");
	
	(* peer->router->vtable->peer_sendto)
		(peer, addr_data, addr_len, reply_dest, payload);
}


//Addresses

#define mtc_addr_data_unc(addr) ((unsigned char *) \
	(MTC_PTR_ADD((addr), mtc_offset_align(sizeof(MtcAddr)))) )

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

MtcAddr *mtc_addr_new_null(MtcPeer *peer)
{
	MtcAddr *addr;
	
	addr = (MtcAddr *) malloc(sizeof(MtcAddr));
	addr->refcount = 1;
	addr->peer = peer;
	addr->len = 0;
	if (peer)
		mtc_peer_ref(peer);
		
	return addr;
}

MtcAddr *mtc_addr_new_static(MtcPeer *peer, int static_id)
{
	MtcAddr *addr;
	
	addr = (MtcAddr *) malloc(mtc_offset_align(sizeof(MtcAddr)) + 1);
	addr->refcount = 1;
	addr->peer = peer;
	addr->len = 1;
	mtc_addr_data_unc(addr)[0] = static_id;
	if (peer)
		mtc_peer_ref(peer);
		
	return addr;
}

MtcAddr *mtc_addr_new_from_raw
	(MtcPeer *peer, void *addr_data, size_t addr_len)
{
	MtcAddr *addr;
	
	addr = (MtcAddr *) malloc(mtc_offset_align(sizeof(MtcAddr)) + addr_len);
	addr->refcount = 1;
	addr->peer = peer;
	addr->len = addr_len;
	if (addr_len)
		memcpy(mtc_addr_data_unc(addr), addr_data, addr_len);
	if (peer)
		mtc_peer_ref(peer);
		
	return addr;
}

MtcAddr *mtc_addr_copy(MtcAddr *src_addr)
{
	MtcAddr *addr;
	
	addr = (MtcAddr *) malloc(mtc_offset_align(sizeof(MtcAddr)) + src_addr->len);
	addr->refcount = 1;
	addr->peer = src_addr->peer;
	addr->len = src_addr->len;
	if (src_addr->len)
		memcpy(mtc_addr_data_unc(addr), mtc_addr_data_unc(src_addr), src_addr->len);
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
		if (mtc_addr_data_unc(addr1)[i] != mtc_addr_data_unc(addr2)[i])
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
		if (mtc_addr_data_unc(addr1)[i] != ((char *) addr2_data)[i])
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
	size_t addr_len;
	
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
	
	(* dest->vtable->removed) (dest);
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
		(NULL, 
		dest->addr_data, dest->addr_len);
}

//Function call

static void mtc_fc_handle_dispose(MtcFCHandle *handle)
{
	mtc_dest_remove((MtcDest *) handle);
	mtc_peer_reset_notify_remove(&(handle->notify));
}

static void mtc_fc_handle_peer_reset(MtcPeerResetNotify *notify)
{
	MtcFCHandle *handle = mtc_encl_struct(notify, MtcFCHandle, notify);
	
	//Remove it and set status
	mtc_fc_handle_dispose(handle);
	handle->status = MTC_ERROR_PEER_RESET;
	
	//Callback
	if (handle->cb)
	{
		(* handle->cb) (handle, handle->cb_data);
	}
}

static void mtc_fc_handle_msg(MtcDest *dest, 
		MtcPeer *src, void *ret_addr_data, size_t ret_addr_len, 
		MtcMsg *payload)
{
	MtcFCHandle *handle = (MtcFCHandle *) dest;
	uint32_t member_ptr;
	
	//Check the source first
	if (src != handle->peer)
		goto unintended;
	
	member_ptr = mtc_msg_read_member_ptr(payload);
	
	if (member_ptr == MTC_MEMBER_PTR_FN_RETURN)
	{
		//Deserialize out arguments
		if (handle->out_args)
		{	
			if ((* handle->binary->out_args_deser)
				(payload, handle->out_args) < 0)
			{
				goto unintended;
			}
		}
		
		handle->status = MTC_FC_SUCCESS;
		(* handle->cb)(handle, handle->cb_data);
		
		return;
	}
	else if (MTC_MEMBER_PTR_IS_ERROR(member_ptr))
	{
		handle->status = MTC_MEMBER_PTR_GET_IDX(member_ptr);
		(* handle->cb)(handle, handle->cb_data);
		
		return;
	}
	else
	{
		goto unintended;
	}
	
unintended:
	if (ret_addr_data)
	{
		MtcMsg *error_msg = mtc_msg_with_member_ptr_only
			(MTC_MEMBER_PTR_ERROR | MTC_ERROR_INVALID_ARGUMENT);
		
		mtc_peer_sendto
			(src, ret_addr_data, ret_addr_len, NULL, error_msg);
		
		mtc_msg_unref(error_msg);
	}
}

static void mtc_fc_handle_removed(MtcDest *dest)
{
	MtcFCHandle *handle = (MtcFCHandle *) dest;
	
	mtc_fc_handle_dispose(handle);
}

static void mtc_fc_handle_destroy(MtcDest *dest)
{
	MtcFCHandle *handle = (MtcFCHandle *) dest;
	
	mtc_fc_handle_dispose(handle);
	
	mtc_peer_unref(handle->peer);
	
	if (handle->status == MTC_FC_SUCCESS
		&& handle->out_args)
	{
		(* handle->binary->out_args_free)(handle->out_args);
	}
}

MtcDestVTable mtc_fc_handle_vtable = 
{
	mtc_fc_handle_msg,
	mtc_fc_handle_removed,
	mtc_fc_handle_destroy
};

MtcFCHandle *mtc_fc_start
	(MtcAddr *addr, MtcFCBinary *binary, void *args)
{
	MtcFCHandle	*handle;
	MtcMsg *payload;
	size_t out_args_offset, size;
	
	//Calculate size
	if (binary->out_args_c_size)
	{
		out_args_offset = mtc_offset_align(sizeof(MtcFCHandle));
		size = out_args_offset + binary->out_args_c_size;
	}
	else
	{
		out_args_offset = 0;
		size = sizeof(MtcFCHandle);
	}
	
	//Create and init handle
	handle = (MtcFCHandle *) mtc_dest_create
		(addr->peer->router, -1, size, &mtc_fc_handle_vtable);
	
	handle->peer = addr->peer;
	mtc_peer_ref(addr->peer);
	handle->notify.cb = mtc_fc_handle_peer_reset;
	if (out_args_offset)
		handle->out_args = MTC_PTR_ADD(handle, out_args_offset);
	else
		handle->out_args = NULL;
	handle->status = MTC_ERROR_TEMP;
	handle->binary = binary;
	handle->cb = handle->cb_data = NULL;
	
	mtc_peer_add_reset_notify(addr->peer, &(handle->notify));
		
	//Send mail
	payload = (* binary->in_args_ser)(args);
	mtc_peer_sendto(addr->peer, mtc_addr_data(addr), addr->len, 
		(MtcDest *) handle, payload);
	mtc_msg_unref(payload);
	
	return handle;
}

void mtc_fc_start_unhandled
	(MtcAddr *addr, MtcFCBinary *binary, void *args)
{
	MtcMsg *payload;
	
	//Send mail
	payload = (* binary->in_args_ser)(args);
	mtc_peer_sendto(addr->peer, mtc_addr_data(addr), addr->len, 
		NULL, payload);
	mtc_msg_unref(payload);
}

MtcStatus mtc_fc_finish_sync(MtcFCHandle *handle)
{
	MtcStatus status;
	
	//Block
	while ((status = handle->status) == MTC_ERROR_TEMP)
	{
		mtc_router_sync_io_step
			(mtc_dest_get_router(handle), handle->peer);
	}
	
	return status;
}

//Object handle

static void mtc_object_handle_msg(MtcDest *dest, 
		MtcPeer *src, void *ret_addr_data, size_t ret_addr_len, 
		MtcMsg *payload)
{
	MtcObjectHandle *handle = (MtcObjectHandle *) dest;
	uint32_t member_ptr;
	
	member_ptr = mtc_msg_read_member_ptr(payload);
	
	if (member_ptr == MTC_MEMBER_PTR_FN)
	{
		int fn_id = MTC_MEMBER_PTR_GET_IDX(member_ptr);
		MtcFCBinary	*fc_binary;
		MtcAddr *ret_addr;
		
		if (fn_id >= handle->binary->n_fns)
			goto unintended;
		
		ret_addr = mtc_addr_new_from_raw
			(src, ret_addr_data, ret_addr_len);
		fc_binary = handle->binary->fns + fn_id;
		
		//Deserialize in arguments
		if (fc_binary->in_args_c_size)
		{
			void *args = mtc_alloc(fc_binary->in_args_c_size);
			
			if ((* fc_binary->in_args_deser)
				(payload, args) < 0)
			{
				mtc_free(args);
				mtc_addr_unref(ret_addr);
				goto unintended;
			}
			
			(* handle->impl[fn_id])(handle, ret_addr, args);
			
			(* fc_binary->in_args_free)(args);
			mtc_free(args);
		}
		else
		{
			(* handle->impl[fn_id])(handle, ret_addr, NULL);
		}
		
		mtc_addr_unref(ret_addr);
		
		return;
	}
	else
	{
		goto unintended;
	}
	
unintended:
	if (ret_addr_data)
	{
		MtcMsg *error_msg = mtc_msg_with_member_ptr_only
			(MTC_MEMBER_PTR_ERROR | MTC_ERROR_INVALID_ARGUMENT);
		
		mtc_peer_sendto
			(src, ret_addr_data, ret_addr_len, NULL, error_msg);
		
		mtc_msg_unref(error_msg);
	}
}

static void mtc_object_handle_removed(MtcDest *dest)
{
	
}

static void mtc_object_handle_destroy(MtcDest *dest)
{
	
}

MtcDestVTable mtc_object_handle_vtable =
{
	mtc_object_handle_msg,
	mtc_object_handle_removed,
	mtc_object_handle_destroy
};

MtcObjectHandle *mtc_object_handle_new
	(MtcRouter *router, MtcClassBinary *binary, int static_id,
	MtcFnImpl *impl, void *impl_data)
{
	MtcObjectHandle *handle = (MtcObjectHandle *) mtc_dest_create
		(router, static_id, sizeof(MtcObjectHandle), 
		&mtc_object_handle_vtable);
	
	handle->binary = binary;
	handle->impl = impl;
	handle->impl_data = impl_data;
	
	return handle;
}

void mtc_fc_return(MtcAddr *addr, MtcFCBinary *binary, void *out_args)
{
	MtcMsg *payload;
	
	//Send mail
	payload = (* binary->out_args_ser)(out_args);
	mtc_peer_sendto(addr->peer, mtc_addr_data(addr), addr->len, 
		NULL, payload);
	mtc_msg_unref(payload);
}

