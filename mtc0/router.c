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

//MDLC
uint32_t mtc_msg_read_member_ptr(MtcMsg *msg)
{
	MtcMBlock byte_stream;
	uint32_t res;
	
	byte_stream = mtc_msg_get_blocks(msg)[0];
	
	if (byte_stream.size < 4)
		return MTC_MEMBER_PTR_NULL;
	
	mtc_uint32_copy_from_le(byte_stream.mem, &res);
	
	return res;
}

MtcMsg *mtc_msg_with_member_ptr_only(uint32_t member_ptr)
{
	MtcMsg *msg;
	MtcMBlock byte_stream;
	
	msg = mtc_msg_new(4, 0);
	
	byte_stream = mtc_msg_get_blocks(msg)[0];
	
	mtc_uint32_copy_to_le(byte_stream.mem, &member_ptr);
	
	return msg;
}

//Router

//Dynamic destinations submodule

typedef struct
{
	unsigned int key;
	unsigned char id[];
} MtcDynamicAddr;

#define counter ((unsigned char *) (router->dynamic_counter))

static void mtc_router_init_dynamic(MtcRouter *router)
{
	router->dynamic_counter = mtc_alloc(1);
	router->dynamic_len = 1;
	counter[0] = 0;
	
	mtc_afl_init(&(router->dests));
}

static MtcDest *mtc_router_alloc_dynamic_dest
	(MtcRouter *router, size_t size)
{
	MtcMBlock addr;
	MtcDynamicAddr *dyn_addr;
	size_t dest_offset;
	size_t alloc_len;
	MtcDest *dest;
	int i;
	unsigned int key;
	
	//Calculations
	addr.size = sizeof(MtcDynamicAddr) + router->dynamic_len;
	dest_offset = mtc_offset_align(addr.size);
	alloc_len = dest_offset + size;
	
	//Allocate
	addr.mem = mtc_rcmem_alloc(alloc_len);
	dest = (MtcDest *) MTC_PTR_ADD(addr.mem, dest_offset);
	
	//Insert
	key = mtc_afl_insert(&(router->dests), (MtcAflItem *) dest);
	
	//Set address
	dest->addr = addr;
	dyn_addr = (MtcDynamicAddr *) addr.mem;
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
	
	return dest;
}

static void mtc_router_remove_dynamic(MtcRouter *router, MtcDest *dest)
{
	mtc_afl_remove(&(router->dests), dest->parent.key);
}

static void mtc_router_free_dynamic(MtcRouter *router)
{
	int n_remain = mtc_afl_get_n_items(&(router->dests));
	if (n_remain)
		mtc_error("MtcRouter destroyed with "
			"%d dynamic destinations still remaining", n_remain);
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

static MtcDest *mtc_router_alloc_static_dest
	(MtcRouter *router, size_t size, int static_id)
{
	MtcMBlock addr;
	MtcStaticAddr *static_addr;
	size_t dest_offset;
	size_t alloc_len;
	MtcDest *dest;
	
	//Calculations
	addr.size = 1;
	dest_offset = mtc_offset_align(addr.size);
	alloc_len = dest_offset + size;
	
	//Allocate
	addr.mem = mtc_rcmem_alloc(alloc_len);
	dest = (MtcDest *) MTC_PTR_ADD(addr.mem, dest_offset);
	
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
	
	//Insert
	if (router->static_dests[static_id])
		mtc_error("static_id %d is already occupied", static_id);
	router->static_dests[static_id] = dest;
	
	//Set address
	dest->addr = addr;
	static_addr = (MtcStaticAddr *) addr.mem;
	static_addr->static_id = static_id;
	
	return dest;
}

static void mtc_router_remove_static(MtcRouter *router, MtcDest *dest)
{
	int static_id = mtc_dest_get_static_id(dest);
	
	router->static_dests[static_id] = NULL;
}

static void mtc_router_free_static(MtcRouter *router)
{
	int i, n = 0;
	for (i = 0; i < router->static_dests_len; i++)
		if (router->static_dests[i])
			n++;
	if (n)
		mtc_error("MtcRouter destroyed with "
			"%d static destinations still remaining", n);
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
		
		mtc_free(router);
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

void mtc_router_deliver(MtcRouter *router, MtcMBlock addr,
	MtcPeer *src, MtcMBlock ret_addr, MtcMsg *payload)
{
	MtcDest *dest = NULL;
	
	//Find destination
	if (addr.size == 1)
	{
		//Static destination
		MtcStaticAddr *static_addr = (MtcStaticAddr *) addr.mem;
		int static_id = static_addr->static_id;
		
		if (static_id < router->static_dests_len)
		{
			dest = router->static_dests[static_id];
		}
	}
	else if (addr.size > sizeof(MtcDynamicAddr))
	{
		//Dynamic destinations
		MtcDynamicAddr *dyn_addr = (MtcDynamicAddr *) addr.mem;
		MtcDest *x = (MtcDest *) mtc_afl_lookup
			(&(router->dests), dyn_addr->key);
		
		if (x)
		{
			if (addr.size == x->addr.size)
			{
				if (memcmp(addr.mem, x->addr.mem, addr.size) == 0)
				{
					dest = x;
				}
			}
		}
	}
	
	//If destination not found, send error
	if (! dest)
	{
		if (ret_addr.size)
		{
			MtcMsg *error_payload = mtc_msg_with_member_ptr_only
				(MTC_MEMBER_PTR_ERROR | MTC_ERROR_INVALID_ARGUMENT);
			
			mtc_peer_sendto(src, ret_addr, NULL, error_payload);
			
			mtc_msg_unref(error_payload);
		}
		
		mtc_warn("peer %p attempted delivery to nonexistent address",
			src);
		
		return;
	}
	
	//Deliver
	(* dest->vtable->msg)(dest, src, ret_addr, payload);
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
		MtcRing *r = &(peer->reset_notifys);
		
		if (r->next != r || r->prev != r)
			mtc_error("all reset_notify's not removed prior to "
				"mtc_peer_unref");
		
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
	el->next = el->prev = el;
}

void mtc_peer_reset(MtcPeer *peer)
{
	MtcRing *el, *r = &(peer->reset_notifys), r2_mem;
	MtcRing *r2 = &r2_mem;
	MtcPeerResetNotify *notify;
	
	*r2 = *r;
	r2->prev->next = r2;
	r2->next->prev = r2;
	r->next = r->prev = r;
	
	while (r2->next != r2)
	{
		el = r2->next;
		el->prev->next = el->next;
		el->next->prev = el->prev;
		el->next = el->prev = el;
		notify = (MtcPeerResetNotify *) el;
		
		(* notify->cb)(notify);
	}
}

void mtc_peer_sendto(MtcPeer *peer, MtcMBlock addr,
		MtcDest *reply_dest, MtcMsg *payload)
{
	if (! addr.size)
		mtc_error("Attempted to send to a null address");
	
	(* peer->router->vtable->peer_sendto)
		(peer, addr, reply_dest, payload);
}

int mtc_peer_sync_io_step(MtcPeer *peer)
{
	return (* peer->router->vtable->peer_sync_io_step)(peer);
}


//Addresses

MtcMBlock mtc_addr_new_static(int static_id)
{
	MtcMBlock res;
	MtcStaticAddr *static_addr;
	
	static_addr = (MtcStaticAddr *) mtc_rcmem_alloc(1);
	static_addr->static_id = static_id;
	
	res.mem = static_addr;
	res.size = 1;
	return res;
}


//Destinations

static MtcDest *mtc_dest_create
	(MtcRouter *router, int static_id, size_t size, 
	MtcDestVTable *vtable)
{
	MtcDest *dest;
	
	if (static_id < 0)
	{
		//Dynamic
		
		dest = mtc_router_alloc_dynamic_dest(router, size);
	}
	else if (static_id < 256)
	{
		//Static
		
		dest = mtc_router_alloc_static_dest(router, size, static_id);
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
		mtc_rcmem_unref(dest->addr.mem);
	}
}

MtcMBlock mtc_dest_get_addr(MtcDest *dest)
{
	mtc_rcmem_ref(dest->addr.mem);
	
	return dest->addr;
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

static void mtc_fc_handle_msg(MtcDest *dest, MtcPeer *src, 
	MtcMBlock ret_addr, MtcMsg *payload)
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
		if (handle->cb)
			(* handle->cb)(handle, handle->cb_data);
		
		return;
	}
	else if (MTC_MEMBER_PTR_IS_ERROR(member_ptr))
	{
		handle->status = MTC_MEMBER_PTR_GET_IDX(member_ptr);
		if (handle->cb)
			(* handle->cb)(handle, handle->cb_data);
		return;
	}
	else
	{
		goto unintended;
	}
	
	mtc_error("Assertion failure: unreachable code");
	
unintended:
	mtc_warn("Unintended message received on (MtcFCHandle *) %p", 
		handle);
	if (ret_addr.size)
	{
		MtcMsg *error_msg = mtc_msg_with_member_ptr_only
			(MTC_MEMBER_PTR_ERROR | MTC_ERROR_INVALID_ARGUMENT);
		
		mtc_peer_sendto
			(src, ret_addr, NULL, error_msg);
		
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
	(MtcPeer *peer, MtcMBlock addr, MtcFCBinary *binary, void *args)
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
		(peer->router, -1, size, &mtc_fc_handle_vtable);
	handle->peer = peer;
	mtc_peer_ref(peer);
	handle->notify.cb = mtc_fc_handle_peer_reset;
	if (out_args_offset)
		handle->out_args = MTC_PTR_ADD(handle, out_args_offset);
	else
		handle->out_args = NULL;
	handle->status = MTC_ERROR_TEMP;
	handle->binary = binary;
	handle->cb = handle->cb_data = NULL;
	
	mtc_peer_add_reset_notify(peer, &(handle->notify));
		
	//Send mail
	if (binary->in_args_c_size)
		payload = (* binary->in_args_ser)(args);
	else
		payload = mtc_msg_with_member_ptr_only
			(MTC_MEMBER_PTR_FN | binary->id);
	mtc_peer_sendto(peer, addr, (MtcDest *) handle, payload);
	mtc_msg_unref(payload);
	
	return handle;
}

void mtc_fc_start_unhandled
	(MtcPeer *peer, MtcMBlock addr, MtcFCBinary *binary, void *args)
{
	MtcMsg *payload;
	
	//Send mail
	if (binary->in_args_c_size)
		payload = (* binary->in_args_ser)(args);
	else
		payload = mtc_msg_with_member_ptr_only
			(MTC_MEMBER_PTR_FN | binary->id);
	mtc_peer_sendto(peer, addr, NULL, payload);
	mtc_msg_unref(payload);
}

MtcStatus mtc_fc_finish_sync(MtcFCHandle *handle)
{
	MtcStatus status;
	
	//Block
	while ((status = handle->status) == MTC_ERROR_TEMP)
	{
		if (mtc_peer_sync_io_step(handle->peer)	< 0)
			break;
	}
	
	return status;
}

//Object handle

static void mtc_object_handle_msg(MtcDest *dest, 
		MtcPeer *src, MtcMBlock ret_addr, 
		MtcMsg *payload)
{
	MtcObjectHandle *handle = (MtcObjectHandle *) dest;
	uint32_t member_ptr;
	
	member_ptr = mtc_msg_read_member_ptr(payload);
	
	if (MTC_MEMBER_PTR_IS_FN(member_ptr))
	{
		int fn_id = MTC_MEMBER_PTR_GET_IDX(member_ptr);
		MtcFCBinary	*fc_binary;
		
		if (fn_id >= handle->binary->n_fns)
			goto unintended;
		
		fc_binary = handle->binary->fns + fn_id;
		
		//Deserialize in arguments
		if (fc_binary->in_args_c_size)
		{
			void *args = mtc_alloc(fc_binary->in_args_c_size);
			
			if ((* fc_binary->in_args_deser)
				(payload, args) < 0)
			{
				mtc_free(args);
				goto unintended;
			}
			
			(* handle->impl[fn_id])(handle, src, ret_addr, args);
			
			(* fc_binary->in_args_free)(args);
			mtc_free(args);
		}
		else
		{
			(* handle->impl[fn_id])(handle, src, ret_addr, NULL);
		}
		
		return;
	}
	else
	{
		goto unintended;
	}
	
unintended:
	mtc_warn("Unintended message received on (MtcObjectHandle *) %p", 
		handle);
	if (ret_addr.size)
	{
		MtcMsg *error_msg = mtc_msg_with_member_ptr_only
			(MTC_MEMBER_PTR_ERROR | MTC_ERROR_INVALID_ARGUMENT);
		
		mtc_peer_sendto
			(src, ret_addr, NULL, error_msg);
		
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

void mtc_fc_return(MtcPeer *src, MtcMBlock ret_addr, 
	MtcFCBinary *binary, void *out_args)
{
	MtcMsg *payload;
	
	//Send mail
	if (binary->out_args_c_size)
		payload = (* binary->out_args_ser)(out_args);
	else
		payload = mtc_msg_with_member_ptr_only
			(MTC_MEMBER_PTR_FN_RETURN);
	mtc_peer_sendto(src, ret_addr, NULL, payload);
	mtc_msg_unref(payload);
}

