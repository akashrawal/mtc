/* link.c
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

#include "common.h"

#include <errno.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/uio.h>

//MtcLink


//Gets the currernt incoming connection status of the link.
MtcLinkStatus mtc_link_get_in_status(MtcLink *self)
{
	return self->in_status;
}

//Gets the currernt outgoing connection status of the link.
MtcLinkStatus mtc_link_get_out_status(MtcLink *self)
{
	return self->out_status;
}

//Considers the incoming connection of the link to be broken. 
void mtc_link_break(MtcLink *self)
{
	if ((self->in_status != MTC_LINK_STATUS_BROKEN)
		|| (self->out_status != MTC_LINK_STATUS_BROKEN))
	{
		self->in_status = MTC_LINK_STATUS_BROKEN;
		self->out_status = MTC_LINK_STATUS_BROKEN;
	
		if (self->vtable->action_hook)
			(* self->vtable->action_hook)(self);
	}
}

//Resumes data reception
void mtc_link_resume_in(MtcLink *self)
{
	int delta = 0;
	
	if (self->in_status == MTC_LINK_STATUS_BROKEN)
		mtc_error("Attempted to resume a broken link %p", self);
		
	if (self->in_status != MTC_LINK_STATUS_OPEN)
		delta = 1;
	
	self->in_status = MTC_LINK_STATUS_OPEN;
	
	if (delta == 1)
		if (self->vtable->action_hook)
			(* self->vtable->action_hook)(self);
}

//Resumes data transmission
void mtc_link_resume_out(MtcLink *self)
{
	int delta = 0;
	
	if (self->out_status == MTC_LINK_STATUS_BROKEN)
		mtc_error("Attempted to resume a broken link %p", self);
	
	if (self->out_status != MTC_LINK_STATUS_OPEN)
		delta = 1;
	
	self->out_status = MTC_LINK_STATUS_OPEN;
	
	if (delta == 1)
		if (self->vtable->action_hook)
			(* self->vtable->action_hook)(self);
}

//Schedules a message to be sent through the link.
void mtc_link_queue
	(MtcLink *self, MtcMsg *msg, int stop)
{
	(* self->vtable->queue) (self, msg, stop);
	
	if (self->vtable->action_hook)
		(* self->vtable->action_hook)(self);
}

//Gets whether mtc_link_send will try to send any data.
int mtc_link_can_send(MtcLink *self)
{
	return (* self->vtable->can_send) (self);
}

//Tries to send all queued data
MtcLinkIOStatus mtc_link_send(MtcLink *self)
{
	MtcLinkIOStatus res;
	
	//Do not write on broken link
	if (self->out_status == MTC_LINK_STATUS_BROKEN)
		return MTC_LINK_IO_FAIL;
	
	//Do not write on stopped link
	if (self->out_status == MTC_LINK_STATUS_STOPPED)
		return MTC_LINK_IO_STOP;
	
	res = (* self->vtable->send) (self);
	
	if (res == MTC_LINK_IO_FAIL)
	{
		self->out_status = MTC_LINK_STATUS_BROKEN;
		self->in_status = MTC_LINK_STATUS_BROKEN;
	}
	else if (res == MTC_LINK_IO_STOP)
	{
		self->out_status = MTC_LINK_STATUS_STOPPED;
	}
	
	if (self->vtable->action_hook)
		(* self->vtable->action_hook)(self);
	
	return res;
}


//Tries to receive one message or one signal.
MtcLinkIOStatus mtc_link_receive(MtcLink *self, MtcLinkInData *data)
{
	MtcLinkIOStatus res;
	
	//Do not read on broken link
	if (self->in_status == MTC_LINK_STATUS_BROKEN)
		return MTC_LINK_IO_FAIL;
	
	//Do not read on stopped link
	if (self->in_status == MTC_LINK_STATUS_STOPPED)
		return MTC_LINK_IO_STOP;
	
	res = (* self->vtable->receive) (self, data);
	
	if (res == MTC_LINK_IO_FAIL)
	{
		self->in_status = MTC_LINK_STATUS_BROKEN;
		self->out_status = MTC_LINK_STATUS_BROKEN;
	}
	else if (res == MTC_LINK_IO_OK 
		&& (data->stop))
	{
		self->in_status = MTC_LINK_STATUS_STOPPED;
	}
	
	if (self->vtable->action_hook)
		(* self->vtable->action_hook)(self);
	
	return res;
}

void mtc_link_set_events_enabled(MtcLink *self, int val)
{
	val = val ? 1 : 0;
	
	if (val != self->events_enabled)
	{
		self->events_enabled = val;
		
		(* self->vtable->set_events_enabled)(self, val);
	}
}

static void mtc_link_event_source_init(MtcLink *self)
{
	MtcLinkEventSource *source = mtc_link_get_event_source(self);
	
	mtc_event_source_init((MtcEventSource *) source, 
		&(self->vtable->event_source_vtable));
	
	source->link = self;
	source->received = NULL;
	source->broken = NULL;
	source->stopped = NULL;
	source->data = NULL;
	
	self->events_enabled = 0;
}

static void mtc_link_event_source_destroy(MtcLink *self)
{
	MtcLinkEventSource *source = mtc_link_get_event_source(self);
	
	source->link = NULL;
	
	mtc_event_source_destroy((MtcEventSource *) source);
}

//Increments the reference count of the link by one.
void mtc_link_ref(MtcLink *self)
{
	self->refcount++;
}

//Decrements the reference count of the link by one.
void mtc_link_unref(MtcLink *self)
{
	self->refcount--;
	if (! self->refcount)
	{	
		//Run 'finalize' virtual function.
		(* (self->vtable->finalize))(self);
		
		//Free event source
		mtc_link_event_source_destroy(self);
		
		//Free ourselves
		mtc_free(self);
	}
}

//Gets reference count of the link.
int mtc_link_get_refcount(MtcLink *self)
{
	return self->refcount;
}

MtcLink *mtc_link_create(size_t size, const MtcLinkVTable *vtable)
{
	MtcLink *self;

	self = (MtcLink *) mtc_alloc(size);

	self->refcount = 1;
	self->in_status = self->out_status = MTC_LINK_STATUS_OPEN;
	self->vtable = vtable;
	mtc_link_event_source_init(self);
	
	return self;
}


