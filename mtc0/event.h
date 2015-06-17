/////////////////////////////////////////////////////////////////////
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

//Event test data API

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

//MtcEventSource user API

///An event source
typedef struct _MtcEventSource MtcEventSource;

//MtcEventMgr user API

///Event backend manager
typedef struct _MtcEventMgr MtcEventMgr;

void mtc_event_mgr_ref(MtcEventMgr *mgr);

void mtc_event_mgr_unref(MtcEventMgr *mgr);

///Event backend that makes event sources work
typedef struct _MtcEventBackend MtcEventBackend;

MtcEventBackend *mtc_event_mgr_back
	(MtcEventMgr *mgr, MtcEventSource *source);
	
void mtc_event_backend_destroy(MtcEventBackend *backend);

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
	///Called to pass a message to event implementation
	void (*event) (MtcEventSource *source, MtcEventFlags event);
} MtcEventSourceVTable;

void mtc_event_source_init
	(MtcEventSource *source, const MtcEventSourceVTable *vtable);

void mtc_event_source_destroy(MtcEventSource *source);

void mtc_event_source_prepare
	(MtcEventSource *source, MtcEventTest *tests);

//Backend implementer API

///Table of functions for implementation
typedef struct
{
	///Size of the backend structure
	size_t backend_size;
	///Called to create backend object
	void (*backend_init)(MtcEventBackend *backend, MtcEventMgr *mgr);
	///Called to destroy backend object
	void (*backend_destroy)(MtcEventBackend *backend);
	///Called to assign tests to perform
	void (*backend_prepare)
		(MtcEventBackend *backend, MtcEventTest *tests);
	
	///Called to destroy event manager
	void (*destroy)(MtcEventMgr *mgr);
} MtcEventBackendVTable;

void mtc_event_mgr_init(MtcEventMgr *mgr, MtcEventBackendVTable *vtable);

void mtc_event_mgr_destroy(MtcEventMgr *mgr);

void mtc_event_backend_event
	(MtcEventBackend *backend, MtcEventFlags flags);

#define mtc_event_backend_get_source(backend) \
	(((MtcEventBackend *) (backend))->source)

//Implementation detail
struct _MtcEventSource
{
	const MtcEventSourceVTable *vtable;
	MtcEventTest *tests;
	MtcRing backends;
};

struct _MtcEventMgr
{
	MtcEventBackendVTable *vtable;
	int refcount;
};

struct _MtcEventBackend
{
	MtcEventBackendVTable *vtable;
	MtcEventSource *source;
	MtcRing backend_ring;
};
