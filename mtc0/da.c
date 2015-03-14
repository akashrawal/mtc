/* da.c
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

#include "common.h"

static MtcDest *mtc_da_route_internal
	(MtcDA *self, MtcPeer *peer, MtcLinkInData *data);

//Initialization
void mtc_da_init(MtcDA *self, int n_static_objects)
{
	int i;
	self->objects = NULL;
	self->version = 0;
	mtc_afl_init(&(self->dests));
	self->n_objects = n_static_objects;
	self->objects = (MtcObjectHandle **) mtc_alloc
		(sizeof(void *) * n_static_objects);
	for (i = 0; i < n_static_objects; i++)
	{
		self->objects[i] = NULL;
	}
	self->n_pins = 0;
	self->pin_store = NULL;
}

//Generic destination management
static void mtc_da_insert_dest(MtcDA *self, MtcDest *dest)
{
	mtc_afl_insert(&(self->dests), (MtcAflItem *) dest);
	
	dest->dest_id = dest->parent.key
	                       | (((uint64_t) self->version) << 32)
	                       | MTC_DEST_DYNAMIC_BIT;
	
	self->version++;
}

static void mtc_da_remove_dest(MtcDA *self, MtcDest *dest)
{
	if (! mtc_afl_remove(&(self->dests), dest->parent.key))
		mtc_error("Destination %p not inserted or already removed",
			dest);
	
	dest->dest_id = 0;
}

//Event management data structures
static void mtc_event_pin_init_as_sentinel(MtcEventPin *pin)
{
	pin->parent.type = MTC_DEST_EVT_PIN;
	pin->parent.dest_id = 0;
	pin->parent.peer = NULL;
	pin->handle_id = 0;
	pin->event_next = pin->event_prev = pin;
	pin->peer_next = pin->peer_prev = pin;
}

static MtcEventPin *mtc_da_connect_event
	(MtcDA *self, MtcPeer *peer, MtcEventPin *event_ring, uint64_t handle_id)
{
	MtcEventPin *pin, *peer_ring;
	
	//Allocate memory for new pin
	if (self->n_pins % MTC_EVENT_STORE_MAX_PINS == 0)
	{
		MtcEventStore *new_store;
		
		new_store = (MtcEventStore *) malloc(sizeof(MtcEventStore));
		new_store->next = self->pin_store;
		self->pin_store = new_store;
	}
	pin = self->pin_store->pins 
		+ (self->n_pins % MTC_EVENT_STORE_MAX_PINS);
	self->n_pins++;
	
	//Initialize it
	pin->parent.type = MTC_DEST_EVT_PIN;
	mtc_da_insert_dest(self, (MtcDest *) pin);
	pin->parent.peer = peer;
	pin->handle_id = handle_id;
	
	pin->event_next = event_ring;
	pin->event_prev = event_ring->event_prev;
	event_ring->event_prev->event_next = pin;
	event_ring->event_prev = pin;
	
	peer_ring = &(peer->peer_ring);
	pin->peer_next = peer_ring;
	pin->peer_prev = peer_ring->peer_prev;
	peer_ring->peer_prev->peer_next = pin;
	peer_ring->peer_prev = pin;
	
	return pin;
}

static void mtc_da_move_event
	(MtcDA *self, MtcEventPin *from, MtcEventPin *to)
{
	//Copy all data
	*to = *from;
	
	if (from->parent.dest_id)
	{
		MtcEventPin *from2;
		
		//Replacement as MtcAflItem
		from2 = (MtcEventPin *) mtc_afl_replace
			(&(self->dests), ((MtcAflItem *) from)->key, 
			(MtcAflItem *) to);
		if (from2 != from)
			mtc_error("Bug found: from2(%p) != from(%p)",
				from2, from);
	}
	
	//Replacement in event ring and peer ring
	from->event_next->event_prev = to;
	from->event_prev->event_next = to;
	from->peer_next->peer_prev = to;
	from->peer_prev->peer_next = to;
}

static void mtc_da_remove_event
	(MtcDA *self, MtcEventPin *pin, MtcEventPin *spare)
{
	//Remove destination
	mtc_da_remove_dest(self, (MtcDest *) pin);
	
	//Do required movements
	self->n_pins--;
	mtc_da_move_event(self, pin, spare);
	mtc_da_move_event
		(self, 
		self->pin_store->pins 
			+ (self->n_pins % MTC_EVENT_STORE_MAX_PINS),
		pin);
	
	//Free memory if applicable
	if ((self->n_pins % MTC_EVENT_STORE_MAX_PINS) == 0)
	{
		MtcEventStore *to_free = self->pin_store;
		self->pin_store = self->pin_store->next;
		mtc_free(to_free);
	}
	
	//Remove from rings
	spare->event_next->event_prev = spare->event_prev;
	spare->event_prev->event_next = spare->event_next;
	spare->peer_next->peer_prev = spare->peer_prev;
	spare->peer_prev->peer_next = spare->peer_next;
}

//Peer management
void mtc_peer_init(MtcPeer *self, MtcLink *link, MtcDA *da)
{
	//Initialize the peer
	mtc_link_ref(link);
	self->link = link;
	self->da = da;
	self->fail_cbs = NULL;
	mtc_event_pin_init_as_sentinel(&(self->peer_ring));
}

void mtc_peer_add_fail_notify(MtcPeer *self, MtcPeerFailNotify *notify)
{
	notify->prev = NULL;
	notify->next = self->fail_cbs;
	if (self->fail_cbs)
		self->fail_cbs->prev = notify;
	self->fail_cbs = notify;
}

void mtc_peer_remove_fail_notify
	(MtcPeer *self, MtcPeerFailNotify *notify)
{
	if (notify->prev)
		notify->prev->next = notify->next;
	if (notify->next)
		notify->next->prev = notify->prev;
	if (self->fail_cbs == notify)
		self->fail_cbs = notify->next;
}

void mtc_peer_raise_fail(MtcPeer *self)
{
	//Run all callbacks and remove them
	{
		MtcPeerFailNotify dummy, *iter;
		
		iter = self->fail_cbs;
		self->fail_cbs = NULL;
		
		for (; iter; iter = dummy.next)
		{
			//Replace current element with a dummy one
			//which will never be destroyed
			dummy = *iter;
			if (iter->next)
				iter->next->prev = &dummy;
			iter->next = iter->prev = NULL;
			
			(* dummy.cb) (dummy.data);
		}
	}
	
	//Disconnect all events connected through this link
	{
		MtcEventPin *iter, spare;
		
		for (iter = self->peer_ring.peer_next; 
			iter != &(self->peer_ring);
			iter = spare.peer_next)
		{
			mtc_da_remove_event(self->da, iter, &spare);
		}
	}
}

void mtc_peer_destroy(MtcPeer *self)
{
	//The peer has failed
	mtc_peer_raise_fail(self);
	
	//Free resources
	mtc_link_unref(self->link);
}

void mtc_da_destroy(MtcDA *self)
{
	int i;
	MtcDest **dests;
	int n_dests;
	MtcEventStore *iter, *next;
	
	//Destroy static objects
	for (i = 0; i < self->n_objects; i++)
	{
		if (self->objects[i])
			mtc_free(self->objects[i]);
	}
	
	//Loop for each destination
	n_dests = mtc_afl_get_n_items(&(self->dests));
	if (n_dests > 0)
	{
		dests = (MtcDest **) mtc_alloc(n_dests * sizeof(void *));
		mtc_afl_get_items(&(self->dests), (MtcAflItem **) dests);
		mtc_afl_destroy(&(self->dests));
		for (i = 0; i < n_dests; i++)
		{
			if (dests[i]->type == MTC_DEST_OBJECT_HANDLE)
				mtc_free(dests[i]);
		}
		mtc_free(dests);
	}
	else
		mtc_afl_destroy(&(self->dests));
	
	mtc_free(self->objects);
	
	//Destroy event pins
	for (iter = self->pin_store; iter; iter = next)
	{
		next = iter->next;
		free(iter);
	}
}

//Function call on client side

static void mtc_fc_handle_set_status
	(MtcFCHandle *handle, MtcFCStatus status, MtcMsg *reply)
{
	MtcPeer *peer = handle->parent.peer;
	//Remove the destination if applicable
	if (status > MTC_FC_STATUS_MAX_RESET)
	{
		mtc_da_remove_dest(peer->da, (MtcDest *) handle);
		mtc_peer_remove_fail_notify
			(peer, &(handle->notify));
	}
	
	handle->status = status;
	if (handle->cb)
	{
		handle->reply = NULL;
		(* handle->cb)
			(status, reply, handle->data);
	}
	else
	{
		handle->reply = reply;
	}
}

static void mtc_fc_handle_link_fail_cb(void *data)
{
	MtcFCHandle *handle = (MtcFCHandle *) data;
	
	mtc_fc_handle_set_status(handle, MTC_FC_FAIL_LINK, NULL);
}

void mtc_peer_start_fc
	(MtcPeer *self, uint64_t dest_id, MtcMsg *msg, MtcFCHandle *handle)
{
	//Add the handle
	mtc_da_insert_dest(self->da, (MtcDest *) handle);
	
	//Initialize the handle
	handle->parent.type = MTC_DEST_FC_HANDLE;
	handle->parent.peer = self;
	handle->msg_in_reply 
		= (msg->da_flags & MTC_DA_FLAG_MSG_IN_REPLY) ? 1 : 0;
	handle->notify.cb = mtc_fc_handle_link_fail_cb;
	handle->notify.data = handle;
	mtc_peer_add_fail_notify(self, &(handle->notify));
	handle->status = MTC_FC_NONE;
	handle->reply = NULL;
	handle->cb = NULL;
	handle->data = NULL;
	
	//Send the function call message
	mtc_link_queue_msg
		(self->link, dest_id, handle->parent.dest_id, msg);
}

void mtc_peer_start_fc_unhandled
	(MtcPeer *self, uint64_t dest_id, MtcMsg *msg)
{
	//Send the function call message
	mtc_link_queue_msg
		(self->link, dest_id, 0, msg);
}

MtcFCStatus mtc_fc_handle_finish(MtcFCHandle *handle)
{
	MtcLinkInData link_data;
	MtcDest *affected;
	MtcLinkIOStatus status;
	
	//Send all data
	status = mtc_link_send(handle->parent.peer->link);
	if (status == MTC_LINK_IO_FAIL)
	{	
		mtc_peer_raise_fail(handle->parent.peer);
		return MTC_FC_FAIL_LINK;
	}
	else if (status != MTC_LINK_IO_OK)
	{
		return MTC_FC_TEMP_FAIL;
	}
	
	do 
	{
		//Try to receive
		status = mtc_link_receive(handle->parent.peer->link, &link_data);
		
		if (status == MTC_LINK_IO_FAIL)
		{	
			mtc_peer_raise_fail(handle->parent.peer);
			return MTC_FC_FAIL_LINK;
		}
		else if (status != MTC_LINK_IO_OK)
		{
			return MTC_FC_TEMP_FAIL;
		}
		
		//Route the message
		affected = mtc_da_route_internal
			(handle->parent.peer->da, handle->parent.peer, &link_data);
	} while (((MtcDest *) handle) != affected);
	
	return handle->status;
}

void mtc_fc_handle_cancel(MtcFCHandle *handle)
{
	MtcPeer *peer = handle->parent.peer;
	
	mtc_da_remove_dest(peer->da, (MtcDest *) handle);
	mtc_peer_remove_fail_notify(peer, &(handle->notify));
}

//Event handling on client side

static void mtc_event_handle_wreck(MtcEventHandle *handle)
{
	//Remove
	mtc_da_remove_dest(handle->parent.peer->da, (MtcDest *) handle);
	mtc_peer_remove_fail_notify(handle->parent.peer, &(handle->notify));
	
	//Call callback
	(* handle->cb) (MTC_EVT_FAIL_LINK, NULL, handle->data);
}

static void mtc_event_handle_link_fail_cb(void *data)
{
	MtcEventHandle *handle = (MtcEventHandle *) data;
	
	//Disconnect event
	mtc_event_handle_wreck(handle);
}

void mtc_peer_connect_event
	(MtcPeer *self, uint64_t dest_id, uint32_t tag, 
	MtcEventHandle *handle)
{
	//Add the handle
	mtc_da_insert_dest(self->da, (MtcDest *) handle);
	
	//Initialize the handle
	handle->parent.type = MTC_DEST_EVT_HANDLE;
	handle->parent.peer = self;
	handle->msg_in_reply = MTC_EVENT_TAG_GET_MSG_IN_REPLY(tag);
	handle->notify.cb = mtc_event_handle_link_fail_cb;
	handle->notify.data = handle;
	mtc_peer_add_fail_notify(self, &(handle->notify));
	handle->disconnect_id = 0;
		
	//Connect event
	mtc_link_queue_signal(self->link, dest_id, handle->parent.dest_id,
		MTC_MEMBER_PTR_EVT | MTC_EVENT_TAG_GET_ID(tag));
}

void mtc_event_handle_disconnect(MtcEventHandle *handle)
{
	//Remove
	mtc_da_remove_dest(handle->parent.peer->da, (MtcDest *) handle);
	mtc_peer_remove_fail_notify(handle->parent.peer, &(handle->notify));
	
	//Disconnect connected event
	if (handle->disconnect_id)
	{
		mtc_link_queue_signal
			(handle->parent.peer->link, 
			handle->disconnect_id, 
			handle->parent.dest_id, 
			MTC_SIGNAL_FAIL);
	}
}

//Code for objects

MtcObjectHandle *mtc_object_handle_new
	(MtcDA *da, int static_id, void *object_data,
	MtcClassInfo info, MtcFnImpl *fn_impls)
{
	MtcObjectHandle *self;
	int i;
	
	self = (MtcObjectHandle *) mtc_alloc
		(sizeof(MtcObjectHandle) 
			+ ((info.n_evts - 2) * sizeof(MtcEventPin)));
	
	//Add it to the destination allocator
	if (static_id > 0)
	{
		if (static_id > da->n_objects)
			mtc_error("static_id (%d) too large",
				static_id);
		
		if (da->objects[static_id - 1])
			mtc_error("An object already exists with static_id (%d)",
				static_id);
		
		da->objects[static_id - 1] = self;
		self->parent.dest_id = static_id;
	}
	else
	{
		mtc_da_insert_dest(da, (MtcDest *) self);
	}
	
	//Initialize
	self->parent.type = MTC_DEST_OBJECT_HANDLE;
	self->parent.peer = NULL;
	
	self->da = da;
	self->object_data = object_data;
	self->info = info;
	self->fns = fn_impls;
	
	for (i = 0; i < info.n_evts; i++)
	{
		mtc_event_pin_init_as_sentinel(self->event_rings + i);
	}
	
	return self;
}

void mtc_object_handle_destroy(MtcObjectHandle *self)
{
	MtcEventPin *iter, spare;
	int i;
	
	//Remove the handle
	if (self->parent.dest_id & MTC_DEST_DYNAMIC_BIT)
	{
		mtc_da_remove_dest(self->da, (MtcDest *) self);
	}
	else
	{
		self->da->objects[self->parent.dest_id - 1] = NULL;
	}
	
	//Disconnect all events
	for (i = 0; i < self->info.n_evts; i++)
	{
		for (iter = self->event_rings[i].event_next;
			iter != self->event_rings + i;
			iter = spare.event_next)
		{
			mtc_link_queue_signal
				(iter->parent.peer->link,
				iter->handle_id,
				iter->parent.dest_id,
				MTC_SIGNAL_FAIL);
			
			mtc_da_remove_event(self->da, iter, &spare);
		}
	}
	
	mtc_free(self);
}

void mtc_object_handle_raise_event
	(MtcObjectHandle *self, uint32_t tag, MtcMsg *msg)
{
	MtcEventPin *iter;
	int idx = MTC_EVENT_TAG_GET_ID(tag);
	
	if (msg)
	{
		for (iter = self->event_rings[idx].event_next;
			iter != self->event_rings + idx;
			iter = iter->event_next)
		{
			mtc_link_queue_msg
				(iter->parent.peer->link,
				iter->handle_id,
				iter->parent.dest_id,
				msg);
		}
	}
	else
	{
		for (iter = self->event_rings[idx].event_next;
			iter != self->event_rings + idx;
			iter = iter->event_next)
		{
			mtc_link_queue_signal
				(iter->parent.peer->link,
				iter->handle_id,
				iter->parent.dest_id,
				MTC_SIGNAL_OK);
		}
	}
}

//Routing received messages
static MtcDest *mtc_da_route_internal
	(MtcDA *self, MtcPeer *peer, MtcLinkInData *data)
{
	MtcDest *dest = NULL;
	
	//Find the destination
	if (data->dest & MTC_DEST_DYNAMIC_BIT)
	{
		dest = (MtcDest *) 
			mtc_afl_lookup(&(self->dests), data->dest & UINT32_MAX);
		if (dest)
		{
			if (dest->dest_id != data->dest)
				dest = NULL;
		}
	}
	else
	{
		if (data->dest <= self->n_objects && data->dest > 0)
			dest = (MtcDest *) self->objects[data->dest - 1];
	}
	
	//Filter out non-matching peers if peer filter is there
	if (dest ? (dest->peer ? dest->peer != peer : 0) : 1)
	{
		dest = NULL;
		goto remote_fail;
	}
	
	switch (dest->type)
	{
	case MTC_DEST_FC_HANDLE:
		{
			MtcFCHandle *handle = (MtcFCHandle *) dest;
			
			//See what happened
			if (data->msg)
			{
				if (handle->msg_in_reply)
				{
					//Success
					mtc_fc_handle_set_status
						(handle, MTC_FC_SUCCESS, data->msg);
				}
				else
				{
					//Broke
					mtc_fc_handle_set_status
						(handle, MTC_FC_FAIL_CODE, NULL);
					
					goto remote_fail;
				}
			}
			else
			{
				if (data->signum == MTC_SIGNAL_STOP)
				{
					//Link stopped
					mtc_fc_handle_set_status
						(handle, MTC_FC_LINK_STOP, NULL);
				}
				else if (data->signum == MTC_SIGNAL_OK 
					&& (! handle->msg_in_reply))
				{
					//Success
					mtc_fc_handle_set_status
						(handle, MTC_FC_SUCCESS, NULL);
				}
				else
				{
					//Broke
					mtc_fc_handle_set_status
						(handle, MTC_FC_FAIL_CODE, NULL);
					goto remote_fail;
				}
			}
		}
		break;
	case MTC_DEST_EVT_HANDLE:
		{
			MtcEventHandle *handle = (MtcEventHandle *) dest;
			
			//First we expect a signal indicating that event has been
			//connected successfully
			if (! handle->disconnect_id)
			{
				//Only one signal is acceptable
				if (data->msg || data->signum != MTC_SIGNAL_OK)
				{
					mtc_event_handle_wreck(handle);
					goto remote_fail;
				}
				else
				{
					handle->disconnect_id = data->reply_to;
				}
			}
			//Event may be raised
			else if (data->msg)
			{
				if (handle->msg_in_reply)
				{
					//Raised
					(* handle->cb) 
						(MTC_EVT_RAISE, data->msg, handle->data);
				}
				else
				{
					//Broke
					mtc_event_handle_wreck(handle);
					goto remote_fail;
				}
			}
			else
			{
				if (data->signum == MTC_SIGNAL_STOP)
				{
					//Link stopped
					(* handle->cb) 
						(MTC_EVT_LINK_STOP, NULL, handle->data);
				}
				else if (data->signum == MTC_SIGNAL_OK 
					&& (! handle->msg_in_reply))
				{
					//Raised
					(* handle->cb) 
						(MTC_EVT_RAISE, NULL, handle->data);
				}
				else
				{
					//Broke
					mtc_event_handle_wreck(handle);
					goto remote_fail;
				}
			}
		}
		break;
	case MTC_DEST_OBJECT_HANDLE:
		{
			MtcObjectHandle *handle = (MtcObjectHandle *) dest;
			
			if (data->msg)
			{
				uint32_t member_ptr;
				
				//Read member pointer
				member_ptr = mtc_msg_read_member_ptr(data->msg);
				
				if (member_ptr & MTC_MEMBER_PTR_FN)
				{
					uint32_t fn_num;
					
					//Function call
					fn_num = member_ptr & (~ MTC_MEMBER_PTR_MASK);
					
					if (fn_num >= handle->info.n_fns)
					{
						//Invalid function number
						goto remote_fail;
					}
					
					(* handle->fns[fn_num])
						(handle, peer, data->reply_to, data->msg);
				}
				else
				{
					//Invalid member pointer
					goto remote_fail;
				}
			}
			else
			{
				//The signal number is the member pointer
				if (data->signum & MTC_MEMBER_PTR_EVT)
				{
					//Event connection
					uint32_t evt_num;
					MtcEventPin *pin;
					
					if (! data->reply_to)
						goto remote_fail;
					
					evt_num = data->signum & (~ MTC_MEMBER_PTR_MASK);
					
					if (evt_num >= handle->info.n_evts)
					{
						//Invalid function number
						goto remote_fail;
					}
					
					//Complete event connection
					pin = mtc_da_connect_event
						(self, peer, handle->event_rings + evt_num, 
						data->reply_to);
					mtc_link_queue_signal
						(peer->link, data->reply_to, 
						pin->parent.dest_id, MTC_SIGNAL_OK);
				}
				else
				{
					//Invalid member pointer
					goto remote_fail;
				}
			}
		}
		break;
	case MTC_DEST_EVT_PIN:
		{
			MtcEventPin *pin = (MtcEventPin *) dest;
			MtcEventPin spare;
			
			//Just remove the pin no matter what we receive
			mtc_da_remove_event(self, pin, &spare);
			
			goto remote_fail;
		}
		break;
	}
	
	return dest;
	
remote_fail:

	if ((data->msg || data->signum != MTC_SIGNAL_FAIL) && data->reply_to)
	{
		mtc_link_queue_signal
			(peer->link, data->reply_to, data->dest, 
			MTC_SIGNAL_FAIL);
	}
	
	if (data->msg)
	{
		mtc_msg_unref(data->msg);
	}

	return dest;
}

void mtc_da_route(MtcDA *self, MtcPeer *peer, MtcLinkInData *data)
{
	mtc_da_route_internal(self, peer, data);
}
