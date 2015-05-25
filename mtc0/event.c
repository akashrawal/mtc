/* event.c
 * Event-driven framework abstraction
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

//MtcEventMgr user API

void mtc_event_mgr_ref(MtcEventMgr *mgr)
{
	mgr->refcount++;
}

void mtc_event_mgr_unref(MtcEventMgr *mgr)
{
	mgr->refcount--;
	if (mgr->refcount <= 0)
	{
		(* mgr->vtable->destroy)(mgr);
	}
}

//MtcEventSource user API

void mtc_event_source_ref(MtcEventSource *source)
{
	source->refcount++;
}

void mtc_event_source_unref(MtcEventSource *source)
{
	source->refcount--;
	if (source->refcount <= 0)
	{
		(* source->vtable->destroy)(source);
		(* source->impl_vtable->destroy_source)(source);
		mtc_free(source);
	}
}

void mtc_event_source_set_active(MtcEventSource *source, int value)
{
	int old_value = source->active;
	source->active = value ? 1 : 0;
	
	if (source->active != old_value && source->vtable->set_active)
		(* source->vtable->set_active)(source, value);
}

int mtc_event_source_get_active(MtcEventSource *source)
{
	return source->active;
}

//Event source implementer API
MtcEventSource *mtc_event_mgr_create_source
	(MtcEventMgr *mgr, size_t struct_size, MtcEventSourceVTable *vtable)
{
	MtcEventSource *source;
	void *impl;
	
	if (struct_size < sizeof(MtcEventSource))
		mtc_error("Event source structure size cannot be "
			"less than sizeof(MtcEventSource)");
	
	source = (MtcEventSource *) mtc_alloc2
		(struct_size, mgr->vtable->sizeof_impl, &impl);
	
	source->refcount = 1;
	source->vtable = vtable;
	source->impl_vtable = mgr->vtable;
	source->impl_data = impl;
	source->active = 0;
	
	(* mgr->vtable->init_source)(source, mgr);
	
	return source;
}

void mtc_event_source_prepare
	(MtcEventSource *source, MtcEventTest *tests)
{
	(* source->impl_vtable->prepare)(source, tests);
}

//Framework implementer API

void mtc_event_mgr_init(MtcEventMgr *mgr, MtcEventImpl *vtable)
{
	mgr->refcount = 1;
	mgr->vtable = vtable;
}

void mtc_event_mgr_destroy(MtcEventMgr *mgr)
{
	//Nothing to do here so far
}

void *mtc_event_source_get_impl(MtcEventSource *source)
{
	return source->impl_data;
}

void mtc_event_source_event(MtcEventSource *source, MtcEventFlags event)
{
	(* source->vtable->event)(source, event);
}
