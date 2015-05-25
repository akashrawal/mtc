/* event.h
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

//TODO: improve docs

//MtcEventMgr user API

///Event source manager
typedef struct _MtcEventMgr MtcEventMgr;

void mtc_event_mgr_ref(MtcEventMgr *mgr);

void mtc_event_mgr_unref(MtcEventMgr *mgr);

//MtcEventSource user API

///An event source
typedef struct _MtcEventSource MtcEventSource;

void mtc_event_source_ref(MtcEventSource *source);

void mtc_event_source_unref(MtcEventSource *source);

void mtc_event_source_set_active(MtcEventSource *source, int value);

int mtc_event_source_get_active(MtcEventSource *source);

//Event source implementer API

///Messages that can be passed to event implementations
typedef enum 
{
	///One or more of the tests have returned positive.
	MTC_EVENT_CHECK       = 1 << 0,
	///Event loop will now check for events.
	MTC_EVENT_POLL_BEGIN  = 1 << 1,
	///Event loop is not checking for events.
	MTC_EVENT_POLL_END    = 1 << 2
} MtcEventFlags;

///Table of functions for handling events
typedef struct
{
	//TODO: Fix documentation bug
	///Called to pass a message to event implementation
	void (*event) (MtcEventSource *source, MtcEventFlags event);
	
	///Called when event source is being destroyed
	void (*destroy)(MtcEventSource *source);
	
	///Called to set whether event source dispatches
	///(Can be NULL)
	void (*set_active)(MtcEventSource *source, int value);
} MtcEventSourceVTable;

MtcEventSource *mtc_event_mgr_create_source
	(MtcEventMgr *mgr, size_t struct_size, MtcEventSourceVTable *vtable);

///Structure to hold tests for an event-driven framework implementation 
///to perform
typedef struct _MtcEventTest MtcEventTest;
struct _MtcEventTest
{
	///pointer to next test, or NULL
	MtcEventTest *next;
	///Name of the test. Should be well known for event-driven framework 
	///implementations to support. Unused characters should be set to 0.
	char name[8];
	///Test data follows it
};

///Test name for poll() test
#define MTC_EVENT_TEST_POLLFD "pollfd"

///Test data for poll() test
typedef struct 
{
	MtcEventTest parent;
	///File descriptor to poll
	int fd;
	///Events to check on the file descriptor
	int events;
	///After polling, results should be set here
	int revents;
} MtcEventTestPollFD;

///Events supported by the poll() test
typedef enum 
{
	MTC_POLLIN = 1 << 0,
	MTC_POLLOUT = 1 << 1,
	MTC_POLLPRI = 1 << 2,
	MTC_POLLERR = 1 << 3,
	MTC_POLLHUP = 1 << 4,
	MTC_POLLNVAL = 1 << 5
} MtcEventTestPollFDEvents;

void mtc_event_source_prepare
	(MtcEventSource *source, MtcEventTest *tests);

//Framework implementer API

//TODO: Revise interface
///Table of functions for implementation
typedef struct
{
	///Size of implementation structure
	size_t sizeof_impl;
	///Called by event sources to provide tests for polling
	void (*prepare)(MtcEventSource *source, MtcEventTest *tests);
	///Called to initialize event source implementation
	void (*init_source)(MtcEventSource *source, MtcEventMgr *mgr);
	///Called to destroy the source implementation
	void (*destroy_source)(MtcEventSource *source);
	///Called to destroy event manager
	void (*destroy)(MtcEventMgr *mgr);
} MtcEventImpl;

void mtc_event_mgr_init(MtcEventMgr *mgr, MtcEventImpl *vtable);

void mtc_event_mgr_destroy(MtcEventMgr *mgr);

void *mtc_event_source_get_impl(MtcEventSource *source);

void mtc_event_source_event(MtcEventSource *source, MtcEventFlags flags);

//Implementation detail
struct _MtcEventSource
{
	int refcount;
	MtcEventSourceVTable *vtable;
	MtcEventImpl *impl_vtable;
	void *impl_data;
	int active;
};

struct _MtcEventMgr
{
	int refcount;
	MtcEventImpl *vtable;
};
