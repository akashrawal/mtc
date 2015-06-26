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

/**
 * \addtogroup mtc_event
 * \{
 * 
 * This contains an abstract interface for event-driven framework.
 * 
 * Events are handled by MtcEventSource objects. These can be
 * derrived to implement various kinds ov events. Event's test 
 * conditions are provided by MtcEventTest structures.
 * 
 * Event sources need to be implemented by a backend. 
 * For that you need event backend managers (MtcEventMgr) for the
 * event loop you are using, which canb e found in other libraries or
 * you can implement them yourself. 
 */

//Event test data API

///Structure to hold test conditions for an event-driven 
///framework implementation to check for
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

/**Tests whether test->name is equal to name
 * \param test A test condition
 * \param name String to compare 
 * \return 1 if they are identical, 0 otherwise
 */
int mtc_event_test_check_name(MtcEventTest *test, const char *name);

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

/**Convenient function to initialize MtcEventTestPollFD struct
 * \param test Structure to initialize
 * \param fd File descriptor
 * \param events Events to check for (see MtcEventTestPollFDEvents)
 */
void mtc_event_test_pollfd_init
	(MtcEventTestPollFD *test, int fd, int events);

///Events supported by the poll() test
typedef enum 
{
	///File descriptor is ready for writing
	MTC_POLLOUT = 1 << 1,
	///For everything else
	MTC_POLLIN = 1 << 0
} MtcEventTestPollFDEvents;

//MtcEventSource user API

///An event source
typedef struct _MtcEventSource MtcEventSource;

//MtcEventMgr user API

///Event backend manager
typedef struct _MtcEventMgr MtcEventMgr;

/**Increments reference count by 1
 * \param mgr Event backend manager
 */
void mtc_event_mgr_ref(MtcEventMgr *mgr);

/**Decrements reference count by 1
 * \param mgr Event backend manager
 */
void mtc_event_mgr_unref(MtcEventMgr *mgr);

///Event backend that makes event sources work
typedef struct _MtcEventBackend MtcEventBackend;

/**Assigns a backend to an event source.
 * \param mgr An event backend manager
 * \param source An event source
 * \return A new backend object that will drive the event source
 */
MtcEventBackend *mtc_event_mgr_back
	(MtcEventMgr *mgr, MtcEventSource *source);

/**Destroys the backend object.
 * \param backend A backend object from mtc_event_mgr_back()
 */
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
	///Messages required by event source
	MtcEventFlags req_mask;
} MtcEventSourceVTable;

/**Initializes an event source.
 * \param source Pointer to structure to initialize
 * \param vtable Functions for implementing event source
 */
void mtc_event_source_init
	(MtcEventSource *source, const MtcEventSourceVTable *vtable);

/**Performs required cleanup on the event source.
 * You must call this function before deallocating its memory.
 * \param source An event source
 */
void mtc_event_source_destroy(MtcEventSource *source);

/**Assigns test conditions for event source to check for
 * \param source An event source
 * \param tests Test conditions
 */
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

/**Initializes an event backend manager. 
 * \param mgr Pointer to the structure to initialize
 * \param vtable Functions for implementing a backend
 */
void mtc_event_mgr_init(MtcEventMgr *mgr, MtcEventBackendVTable *vtable);

/**Performs cleanup on event manager
 * \param mgr An event backend manager
 */
void mtc_event_mgr_destroy(MtcEventMgr *mgr);

/**Informs the attached event source of something related happening
 * \param backend An event backend
 * \param flags Flags specifying which events happened.
 */
void mtc_event_backend_event
	(MtcEventBackend *backend, MtcEventFlags flags);

/**Gets the associated source of the backend
 * \param backend An event backend
 * \return An event source
 */
#define mtc_event_backend_get_source(backend) \
	(((MtcEventBackend *) (backend))->source)

/**Gets the mask of flags that should be supported by the backend.
 * \param backend An event backend
 * \return mask of flags that should be supported by the backend.
 */
#define mtc_event_backend_get_req_mask(backend) \
	(mtc_event_backend_get_source(backend)->vtable->req_mask)

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

/**
 * \}
 */
