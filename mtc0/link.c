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

//TODO: Scan this file for further modularization
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
	self->in_status = MTC_LINK_STATUS_BROKEN;
	self->out_status = MTC_LINK_STATUS_BROKEN;
}

//Resumes data reception
void mtc_link_resume_in(MtcLink *self)
{
	if (self->in_status == MTC_LINK_STATUS_BROKEN)
		mtc_error("Attempted to resume a broken link %p", self);
	self->in_status = MTC_LINK_STATUS_OPEN;
}

//Resumes data transmission
void mtc_link_resume_out(MtcLink *self)
{
	if (self->out_status == MTC_LINK_STATUS_BROKEN)
		mtc_error("Attempted to resume a broken link %p", self);
	self->out_status = MTC_LINK_STATUS_OPEN;
}

//Schedules a message to be sent through the link.
void mtc_link_queue
	(MtcLink *self, MtcMsg *msg, int stop)
{
	(* self->vtable->queue) (self, msg, stop);
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
	
	return res;
}

//Gets the tests for event loop integration
MtcLinkTest *mtc_link_get_tests(MtcLink *self)
{
	return (self->vtable->get_tests(self));
}

//Evaluates the results of the tests.
int mtc_link_eval_test_result(MtcLink *self)
{
	return (self->vtable->eval_test_result(self));
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
	
	return self;
}

/*-------------------------------------------------------------------*/
//MtcFDLink

//Stores information about current reading status
typedef enum 
{
	//Starting
	MTC_FD_LINK_INIT_READ = 0,
	//Reading header
	MTC_FD_LINK_HDR = 1,
	//Reading message with only main block
	MTC_FD_LINK_SIMPLE = 2,
	//Reading message's block size index
	MTC_FD_LINK_IDX = 3,
	//Reading message data
	MTC_FD_LINK_DATA = 4
} MtcFDLinkReadStatus;

//Data to be sent
typedef struct _MtcFDLinkSendJob MtcFDLinkSendJob;
struct _MtcFDLinkSendJob
{
	MtcFDLinkSendJob *next;
	
	MtcMsg *msg;
	
	int stop_flag;
	unsigned int n_blocks;
	
	//The header data
	MtcHeaderBuf hdr;
};

#define MTC_IOV_MIN 16

//A link that operates on file descriptor
typedef struct
{
	MtcLink parent;
	
	//Underlying file descriptors
	int out_fd, in_fd;
	
	//Whether to close file descriptors
	int close_fd;
	
	//Stuff for sending
	struct
	{
		struct iovec *mem;
		int alen, start, len, ulim, clip;
	} iov;
	struct
	{
		MtcFDLinkSendJob *head, *tail;
	} jobs;
	
	//Stuff for receiving
	MtcReader reader;
	MtcReaderV reader_v;
	MtcFDLinkReadStatus read_status;
	MtcHeaderData header_data;
	void *mem; //< buffer for BSI and IO vector
	MtcMsg *msg; //< Message structure
	
	//Event loop integration
	MtcLinkTestPollFD tests[2];
	
	//Preallocated buffers
	MtcHeaderBuf header;
} MtcFDLink;

//Functions to manage IO vector
static struct iovec *mtc_fd_link_alloc_iov
	(MtcFDLink *self, int n_blocks)
{
	struct iovec *res;
	
	if (self->iov.start + self->iov.len + n_blocks > self->iov.alen)
	{
		//Resize if IO vector is not sufficiently large.
		int new_alen, i;
		struct iovec *new_iov, *src;
		
		new_alen = self->iov.alen * 2;
		new_iov = (struct iovec *) mtc_alloc
			(sizeof(struct iovec) * new_alen);
		
		src = self->iov.mem + self->iov.start;
		for (i = 0; i < self->iov.len; i++)
			new_iov[i] = src[i];
		
		mtc_free(self->iov.mem);
		self->iov.mem = new_iov;
		self->iov.start = 0;
		self->iov.alen = new_alen;
	}
	
	//Allocate
	res = self->iov.mem + self->iov.start + self->iov.len;
	self->iov.len += n_blocks;
	
	return res;
}

static int mtc_fd_link_pop_iov(MtcFDLink *self, int n_bytes)
{
	int n_blocks = 0;
	struct iovec *vector;
	
	vector = self->iov.mem + self->iov.start;
	
	//Count finished blocks and update unfinished block
	while (n_blocks < self->iov.len ? n_bytes >= vector->iov_len : 0)
	{
		n_bytes -= vector->iov_len;
		n_blocks++;
		vector++;
	}
	if (n_bytes > 0)
	{
		vector->iov_base = MTC_PTR_ADD(vector->iov_base, n_bytes);
		vector->iov_len -= n_bytes;
	}
	
	//Update IO vector
	self->iov.start += n_blocks;
	self->iov.len -= n_blocks;
	if (self->iov.clip >= 0)
	{
		self->iov.clip -= n_blocks;
		if (self->iov.clip < 0)
			mtc_error("Assertion failure");
	}
	
	//Collapse IO vector if necessary
	if (self->iov.alen > MTC_IOV_MIN
		&& self->iov.len <= self->iov.alen * 0.25)
	{
		int new_alen, i;
		struct iovec *new_iov, *src;
		
		new_alen = self->iov.alen / 2;
		new_iov = (struct iovec *) mtc_alloc
			(sizeof(struct iovec) * new_alen);
		
		src = self->iov.mem + self->iov.start;
		for (i = 0; i < self->iov.len; i++)
			new_iov[i] = src[i];
		
		mtc_free(self->iov.mem);
		self->iov.mem = new_iov;
		self->iov.start = 0;
		self->iov.alen = new_alen;
	}
	else if (self->iov.start >= self->iov.alen / 2)
	{
		int i;
		struct iovec *src;
		
		src = self->iov.mem + self->iov.start;
		for (i = 0; i < self->iov.len; i++)
			self->iov.mem[i] = src[i];
		
		self->iov.start = 0;
	}
	
	return n_blocks;
}

static void mtc_fd_link_init_iov(MtcFDLink *self)
{
	self->iov.mem = (struct iovec *) 
		mtc_alloc(sizeof(struct iovec) * MTC_IOV_MIN);
	self->iov.alen = MTC_IOV_MIN;
	self->iov.start = 0;
	self->iov.len = 0;
	self->iov.clip = -1;
}

//Schedules a message to be sent through the link.
static void mtc_fd_link_queue
	(MtcLink *link, MtcMsg *msg, int stop)
{	
	MtcFDLink *self = (MtcFDLink *) link;
	
	MtcFDLinkSendJob *job;
	MtcMBlock *blocks;
	uint32_t n_blocks;
	uint32_t hdr_len; 
	uint32_t i;
	struct iovec *iov;
	
	mtc_msg_ref(msg);
	
	//Get the data to be sent
	n_blocks = mtc_msg_get_n_blocks(msg);
	blocks = mtc_msg_get_blocks(msg);
	
	//Allocate a new structure...
	hdr_len = mtc_header_sizeof(n_blocks);
	job = (MtcFDLinkSendJob *) mtc_alloc
		(sizeof(MtcFDLinkSendJob) - sizeof(MtcHeaderBuf) + hdr_len);
	
	//Initialize job
	if (self->jobs.head)
		self->jobs.tail->next = job;
	else 
		self->jobs.head = job;
	self->jobs.tail = job;
	job->next = NULL;
	job->msg = msg;
	job->stop_flag = stop ? 1 : 0;
	job->n_blocks = n_blocks + 1;
	mtc_header_write(&(job->hdr), blocks, n_blocks, stop);
	
	//Fill data into IOV
	iov = mtc_fd_link_alloc_iov(self, n_blocks + 1);
	iov[0].iov_base = &(job->hdr);
	iov[0].iov_len = hdr_len;
	iov++;
	for (i = 0; i < n_blocks; i++)
	{
		iov[i].iov_base = blocks[i].data;
		iov[i].iov_len = blocks[i].len;
	}
	
	//Setup stop
	if (stop)
	{
		if (self->iov.clip < 0)
			self->iov.clip = self->iov.len;
	}
}

//Determines whether mtc_link_send will try to send any data.
static int mtc_fd_link_can_send(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	
	if (self->iov.clip > 0)
		return 1;
	else if (self->iov.clip == 0)
		return 0;
	else if (self->iov.len > 0)
		return 1;
	else
		return 0;
}

//Tries to send all queued data
static MtcLinkIOStatus mtc_fd_link_send(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	
	int blocks_out = 0;
	ssize_t bytes_out;
	int repeat_count;
	
	//Run the operation
	for (repeat_count = 0; ; repeat_count++)
	{
		struct iovec *vector;
		int n_blocks;
		int repeat = 0;
		
		n_blocks = self->iov.clip >= 0 ? self->iov.clip : self->iov.len;
		vector = self->iov.mem + self->iov.start;
		
		if (n_blocks > 0)
		{
			if (self->iov.ulim > 0 && n_blocks > self->iov.ulim)
			{
				n_blocks = self->iov.ulim;
				repeat = 1;
			}
			bytes_out = writev(self->out_fd, vector, n_blocks);
		}
		else
			bytes_out = 0;
		
		//Handle errors
		if (bytes_out < 0)
		{
			repeat = 0;
			if (! repeat_count)
			{
				if (MTC_IO_TEMP_ERROR(errno))
					return MTC_LINK_IO_TEMP;
				else
					return MTC_LINK_IO_FAIL;
			}
		}
		else
			blocks_out += mtc_fd_link_pop_iov(self, bytes_out);
		
		if (! repeat)
			break;
	}
	
	//Garbage collection
	{
		MtcFDLinkSendJob *iter, *next;
		for (iter = self->jobs.head; iter; iter = next)
		{
			next = iter->next;
			
			if (blocks_out < iter->n_blocks)
				break;
			
			blocks_out -= iter->n_blocks;
			mtc_msg_unref(iter->msg);
			mtc_free(iter);
		}
		
		self->jobs.head = iter;
		if (iter)
			iter->n_blocks -= blocks_out;
		else
			self->jobs.tail = NULL;
		
		if (self->jobs.head && self->iov.len)
			mtc_error("Assertion failure");
	}
	
	//Next stop signal and return status
	if (self->iov.clip == 0)
	{
		MtcFDLinkSendJob *iter;
		int counter = 0;
		
		for (iter = self->jobs.head; iter; iter = iter->next)
		{
			counter += iter->n_blocks;
			if (iter->stop_flag)
				break;
		}
		
		if (iter)
			self->iov.clip = counter;
		else
			self->iov.clip = -1;
		
		return MTC_LINK_IO_STOP;
	}
	else if (self->jobs.head)
		return MTC_LINK_IO_TEMP;
	else
		return MTC_LINK_IO_OK;
}

//Tries to receive a message or a signal.
static MtcLinkIOStatus mtc_fd_link_receive
	(MtcLink *link, MtcLinkInData *data)
{
	MtcFDLink *self = (MtcFDLink *) link;
	MtcIOStatus io_res = MTC_IO_OK;
	MtcHeaderData *header = &(self->header_data);
	MtcMBlock *blocks;
	
	switch (self->read_status)
	{
	case MTC_FD_LINK_INIT_READ:
		//Prepare the reader to read header
		mtc_reader_init(&(self->reader),
			(void *) &(self->header), mtc_header_min_size, self->in_fd);
		
		//Save status
		self->read_status = MTC_FD_LINK_HDR;
		
		//Fallthrough
	case MTC_FD_LINK_HDR:
		//Try to read generic header.
		io_res = mtc_reader_read(&(self->reader));
		
		//If reading not finished bail out.
		if (io_res != MTC_IO_OK)
			break;
		
		//Now start reading...
		
		//Convert all byte orders
		if(! mtc_header_read(&(self->header), header))
		{
			mtc_warn("Invalid header on link %p, breaking the link.",
			         self);
			return MTC_LINK_IO_FAIL;
		}
		
		//For message the program continues
		
		//Check size of main block
		if (! header->data_1)
		{
			mtc_warn("Size of main memory block is zero "
					 "for message received on link %p, breaking link",
					 self);
			return MTC_LINK_IO_FAIL;
		}
		
		//If the message has extra blocks follow another way
		if (header->size != 1)
			goto complex_msg;
			
		//For message having only main block, continue...
		
		//Try to create an allocated message
		self->msg = mtc_msg_try_new_allocd(header->data_1, 0, NULL);
		if (! self->msg)
		{
			mtc_warn("Failed to allocate message structure "
			         "for message received on link %p, breaking link.",
			         self);
			return MTC_LINK_IO_FAIL;
		}
		
		//Prepare the reader to read the message main block.
		//There's only one block.
		blocks = mtc_msg_get_blocks(self->msg);
		mtc_reader_init
			(&(self->reader), blocks->data, blocks->len, self->in_fd);
		
		//Save status
		self->read_status = MTC_FD_LINK_SIMPLE;
		
		//Fallthrough
	case MTC_FD_LINK_SIMPLE:
		//Read message main block
		io_res = mtc_reader_read(&(self->reader));
		
		//If reading not finished bail out.
		if (io_res != MTC_IO_OK)
			break;
		
		//Message reading finished, jump to end of switch
		//there is code to return the message
		goto return_msg;
		
	complex_msg:
		//we now need to read the block size index.
		//but we will also need to read message data in future.
		//Allocate memory large enough for both purposes
		//but don't crash if it fails.
		{
			//I think this is large enough
			size_t alloc_size = header->size * sizeof(struct iovec);
			self->mem = mtc_tryalloc(alloc_size);
			if (! self->mem)
			{
				mtc_warn("Memory allocation failed for %ld bytes "
						 "for message received on link %p, breaking link",
						 (long) alloc_size, self);
				return MTC_LINK_IO_FAIL;
			}
		}
		
		//Prepare the reader to read BSI
		mtc_reader_init
				(&(self->reader), 
				self->mem, sizeof(uint32_t) * (header->size - 1), 
				self->in_fd);
	
		//Save status
		self->read_status = MTC_FD_LINK_IDX;
		
		//Fallthrough
	case MTC_FD_LINK_IDX:
		//Read the index
		io_res = mtc_reader_read(&(self->reader));
		
		//If reading not finished bail out.
		if (io_res != MTC_IO_OK)
			break;
		
		//Assertions and byte order conversion
		{
			uint32_t *iter, *lim;
			
			iter = (uint32_t *) self->mem;
			lim = iter + header->size - 1;
			
			for (; iter < lim; iter++)
			{
				*iter = mtc_uint32_from_le(*iter);
				if (! *iter)
				{
					mtc_warn("In block size index received on link %p, "
					         "element %ld has value 0, breaking link.",
					         self, 
					         (long) (iter - (uint32_t *) self->mem));
					return MTC_LINK_IO_FAIL;
				}
			}
		}
		
		//Create message reader
		self->msg = mtc_msg_try_new_allocd
			(header->data_1, header->size - 1, (uint32_t *) self->mem);
		if (! self->msg)
		{
			mtc_warn("Failed to allocate message structure "
			         "for message received on link %p, breaking link.",
			         self);
			return MTC_LINK_IO_FAIL;
		}
		
		//Prepare reader_v to read message data
		{
			MtcMBlock *blocks, *blocks_lim;
			struct iovec *vector;
			
			//First create the IO vector. 
			//There are atleast two memory blocks.
			vector = (struct iovec *) self->mem;
			blocks = mtc_msg_get_blocks(self->msg);
			blocks_lim = blocks + header->size;
			
			while (blocks < blocks_lim)
			{
				vector->iov_base = blocks->data;
				vector->iov_len = blocks->len;
				
				blocks++;
				vector++;
			}
			
			//Now prepare
			mtc_reader_v_init(&(self->reader_v),
							  (struct iovec *) self->mem,
							  header->size,
							  self->in_fd);
		}
		
		//Save status
		self->read_status = MTC_FD_LINK_DATA;
		
		//Fallthrough
	case MTC_FD_LINK_DATA:
		//Try to read message data
		io_res = (ssize_t) mtc_reader_v_read(&(self->reader_v));
		
		//If reading not finished bail out.
		if (io_res != MTC_IO_OK)
			break;
		
		//Message reading finished
		
		//Free the memory allocated for IO vector
		mtc_free(self->mem);
		self->mem = NULL;
		
	return_msg:
		//Return data
		data->msg = self->msg;
		data->stop = header->stop;
		
		//Reset status
		self->read_status = MTC_FD_LINK_INIT_READ;
		self->msg = NULL;
		
		return MTC_LINK_IO_OK;
	}
	
	if ((io_res == MTC_IO_SEVERE) || (io_res == MTC_IO_EOF))
		return MTC_LINK_IO_FAIL;
	else
		return MTC_LINK_IO_TEMP;
}

MtcLinkTest *mtc_fd_link_get_tests(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	int out_idx = self->in_fd == self->out_fd ? 0 : 1;
	
	self->tests[0].events = MTC_POLLIN;
	self->tests[0].revents = 0;
	self->tests[1].events = 0;
	self->tests[1].revents = 0;
	
	if (mtc_fd_link_can_send(link))
		self->tests[out_idx].events |= MTC_POLLOUT;
	
	return (MtcLinkTest *) self->tests;
}

int mtc_fd_link_eval_test_result(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	int out_idx = self->in_fd == self->out_fd ? 0 : 1;
	int res = 0;
	
	//Error condition
	if (self->tests[0].revents & (MTC_POLLERR | MTC_POLLHUP | MTC_POLLNVAL)
		|| self->tests[1].revents & (MTC_POLLERR | MTC_POLLHUP | MTC_POLLNVAL))
		return MTC_LINK_TEST_BREAK;
	
	//Others
	if (self->tests[0].revents & (MTC_POLLIN))
		res |= MTC_LINK_TEST_RECV;
	
	if (self->tests[out_idx].revents & (MTC_POLLOUT))
		res |= MTC_LINK_TEST_SEND;
	
	return res;
}

static void mtc_fd_link_finalize(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	MtcFDLinkSendJob *iter, *next;
	
	//Destroy IO vector and all jobs.
	mtc_free(self->iov.mem);
	for (iter = self->jobs.head; iter; iter = next)
	{
		next = iter->next;
		
		mtc_msg_unref(iter->msg);
		mtc_free(iter);
	}
	
	//Destroy the 'additional' buffer
	if (self->mem)
	{
		mtc_free(self->mem);
		self->mem = NULL;
	}
	
	//Destroy partially read message if any
	if (self->msg)
	{
		mtc_msg_unref(self->msg);
		self->msg = NULL;
	}
	
	//Close file descriptors
	if (self->close_fd)
	{
		close(self->in_fd);
		if (self->in_fd != self->out_fd)
			close(self->out_fd);
	}
}


//VTable
const static MtcLinkVTable mtc_fd_link_vtable = {
	mtc_fd_link_queue,
	mtc_fd_link_can_send,
	mtc_fd_link_send,
	mtc_fd_link_receive,
	mtc_fd_link_get_tests,
	mtc_fd_link_eval_test_result,
	mtc_fd_link_finalize
};

//Constructor
MtcLink *mtc_fd_link_new(int out_fd, int in_fd)
{
	MtcFDLink *self;
	
	self = (MtcFDLink *) mtc_link_create
			(sizeof(MtcFDLink), &mtc_fd_link_vtable);
	
	//Set file descriptors
	self->out_fd = out_fd;
	self->in_fd = in_fd;
	self->close_fd = 0;
	
	//Initialize sending data
	mtc_fd_link_init_iov(self);
	self->jobs.head = NULL;
	self->jobs.tail = NULL;
	self->iov.ulim = sysconf(_SC_IOV_MAX);
	
	//Initialize reading data
	self->read_status = MTC_FD_LINK_INIT_READ;
	self->mem = self->msg = NULL;
	
	//Initialize test data
	//events and revents will be initialized later. 
	{
		MtcLinkTest constdata = 
			{NULL, {'p', 'o', 'l', 'l', 'f', 'd', '\0', '\0'}};
		
		self->tests[0].parent = constdata;
		self->tests[0].fd = in_fd;
		if (in_fd != out_fd)
		{
			self->tests[0].parent.next = (MtcLinkTest *) self->tests + 1;
			
			self->tests[1].parent = constdata;
			self->tests[1].fd = out_fd;
		}
	}
	
	return (MtcLink *) self;
}

int mtc_fd_link_get_out_fd(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	
	if (link->vtable != &mtc_fd_link_vtable)
		mtc_error("%p is not MtcFDLink", link);
		
	return self->out_fd;
}

int mtc_fd_link_get_in_fd(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	
	if (link->vtable != &mtc_fd_link_vtable)
		mtc_error("%p is not MtcFDLink", link);
		
	return self->in_fd;
}

int mtc_fd_link_get_close_fd(MtcLink *link)
{
	MtcFDLink *self = (MtcFDLink *) link;
	
	if (link->vtable != &mtc_fd_link_vtable)
		mtc_error("%p is not MtcFDLink", link);
	
	return self->close_fd;
}

void mtc_fd_link_set_close_fd(MtcLink *link, int val)
{
	MtcFDLink *self = (MtcFDLink *) link;
	
	if (link->vtable != &mtc_fd_link_vtable)
		mtc_error("%p is not MtcFDLink", link);
	
	self->close_fd = (val ? 1 : 0);
}
