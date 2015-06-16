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

//Event test data API

//MtcEventSource user API

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

MtcEventBackend *mtc_event_mgr_back
	(MtcEventMgr *mgr, MtcEventSource *source)
{
	MtcEventBackend *backend;
	MtcRing *ring, *sentinel;
	
	//Create backend object
	backend = (MtcEventBackend *) mtc_alloc(mgr->vtable->backend_size);
	backend->vtable = mgr->vtable;
	backend->source = source;
	
	//Add to event source
	ring = &(backend->backend_ring);
	sentinel = &(source->backends);
	ring->next = sentinel;
	ring->prev = sentinel->prev;
	ring->next->prev = ring;
	ring->prev->next = ring;
	
	//Call init and prepare if needed
	(*backend->vtable->backend_init)(backend, mgr);
	if (source->tests)
	{
		(*backend->vtable->backend_prepare)(backend, source->tests);
	}
	
	return backend;
}
	
void mtc_event_backend_destroy(MtcEventBackend *backend)
{
	MtcRing *ring;
	
	//Call destructor
	(*backend->vtable->backend_destroy)(backend);
	
	//Remove from event source
	ring = &(backend->backend_ring);
	ring->next->prev = ring->prev;
	ring->prev->next = ring->next;
	
	//Free
	mtc_free(backend);
}

//Event source implementer API
void mtc_event_source_init
	(MtcEventSource *source, const MtcEventSourceVTable *vtable)
{
	source->vtable = vtable;
	source->tests = NULL;
	MtcRing *sentinel = &(source->backends);
	sentinel->next = sentinel;
	sentinel->prev = sentinel;
}

void mtc_event_source_destroy(MtcEventSource *source)
{
	MtcRing *sentinel = &(source->backends);
	
	//Destroy all backends
	while (sentinel->next != sentinel)
	{
		MtcEventBackend	 *backend = mtc_encl_struct
			(sentinel->next, MtcEventBackend, backend_ring);
		
		mtc_event_backend_destroy(backend);
	}
}

void mtc_event_source_prepare
	(MtcEventSource *source, MtcEventTest *tests)
{
	MtcRing *sentinel = &(source->backends), *ring;
	
	source->tests = tests;
	
	for (ring = sentinel->next; ring != sentinel; ring = ring->next)
	{
		MtcEventBackend	 *backend = mtc_encl_struct
			(ring, MtcEventBackend, backend_ring);
		
		(* backend->vtable->backend_prepare)(backend, tests);
	}
}

//Backend implementer API

void mtc_event_mgr_init(MtcEventMgr *mgr, MtcEventBackendVTable *vtable)
{
	mgr->refcount = 1;
	mgr->vtable = vtable;
}

void mtc_event_mgr_destroy(MtcEventMgr *mgr)
{
	//Nothing to do here so far
}

void mtc_event_backend_event
	(MtcEventBackend *backend, MtcEventFlags flags)
{
	(* backend->source->vtable->event)(backend->source, flags);
}
