/* source.h
 * MtcSource that represents a source file in memory
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

#include <common.h>

#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

//TODO: Refactor

//Reader

#ifndef EWOULDBLOCK
#define MTC_IO_TEMP_ERROR(e) ((e == EAGAIN) || (e == EINTR))
#else
#if (EAGAIN == EWOULDBLOCK)
#define MTC_IO_TEMP_ERROR(e) ((e == EAGAIN) || (e == EINTR))
#else
#define MTC_IO_TEMP_ERROR(e) ((e == EAGAIN) || (e == EWOULDBLOCK) || (e == EINTR))
#endif
#endif

//Return status for IO operation
typedef enum
{	
	//No error
	MTC_IO_OK = 0,
	//Temporary error
	MTC_IO_TEMP = -1,
	//End-of-file encountered while reading
	MTC_IO_EOF = -2,
	//Irrecoverable error
	MTC_IO_SEVERE = -3
} MtcIOStatus;

//A simple reader
typedef struct 
{
	void *mem;
	size_t len;
	int fd;
} MtcReader;


//Initializes the reader
static void mtc_reader_init(MtcReader *self, void *mem, size_t len, int fd)
{
	self->mem = mem;
	self->len = len;
	self->fd  = fd;
}

//Reads some data. Returns no. of bytes remaining to be read, or one of 
//MtcIOStatus in case of error.
static MtcIOStatus mtc_reader_read(MtcReader *self)
{
	ssize_t bytes_read;
	
	//Precaution
	if (! self->len)
		return 0;
	
	//Read some data
	bytes_read = read(self->fd, self->mem, self->len);
	
	//Error checking
	if (bytes_read < 0)
	{
		if (MTC_IO_TEMP_ERROR(errno))
			return MTC_IO_TEMP;
		else
			return MTC_IO_SEVERE;
	}
	else if (bytes_read == 0)
		return MTC_IO_EOF;
	else
	{
		//Successful read.
		//Update and return status
		self->mem = MTC_PTR_ADD(self->mem, bytes_read);
		self->len -= bytes_read;
		
		if (self->len)
			return MTC_IO_TEMP;
		else
			return MTC_IO_OK;
	}
}

//Implementation for MtcSource

void mtc_source_ref(MtcSource *self)
{
	self->refcount++;
}

void mtc_source_unref(MtcSource *self)
{
	self->refcount--;
	if (self->refcount == 0)
	{
		if (self->chunks)
		{
			char **iter = self->chunks;
			char **lim = iter + (self->n_chars >> MTC_SOURCE_CHUNK_OFF) + 1;
			while (iter < lim)
			{
				mtc_free(*iter);
				
				iter++;
			}
			mtc_free(self->chunks);
		}
		if (self->name)
			mtc_free(self->name);
		if (self->lines)
			mtc_free(self->lines);
		mtc_free(self);
	}
}

//Create a new source from a stream
MtcSource *mtc_source_new_from_stream(const char *name, int fd)
{
	MtcSource *self;
	MtcReader reader;
	MtcVector vec;
	int pos;
	
	//Allocate and initialize
	self = mtc_alloc(sizeof(MtcSource));
	self->refcount = 1;
	self->name = mtc_strdup(name);
	self->n_chars = 0;
	self->chunks = NULL;
	self->lines = NULL;
	self->n_lines = 0;
	
	//Read contents of the stream
	mtc_vector_init(&vec);
	while(1)
	{
		char *onechunk;
		MtcIOStatus io_stat;
		
		//Allocate and add a chunk. 
		onechunk = mtc_alloc(MTC_SOURCE_CHUNK_LEN);
		mtc_vector_grow(&vec, sizeof(char *));
		*mtc_vector_last(&vec, char *) = onechunk;
		
		//Initialize reader
		mtc_reader_init(&reader, onechunk, MTC_SOURCE_CHUNK_LEN, fd);
		
		//Read
		while ((io_stat = mtc_reader_read(&reader)) == MTC_IO_TEMP)
			;
		
		//Act on results
		if (io_stat == MTC_IO_OK)
		{
			self->n_chars += MTC_SOURCE_CHUNK_LEN;
			continue;
		}
		else 
		{
			//In case of error destroy everything and exit
			if (io_stat == MTC_IO_SEVERE)
			{
				self->chunks = (char **) vec.data;
				mtc_source_unref(self);
				return NULL;
			}
			
			self->n_chars += MTC_SOURCE_CHUNK_LEN - reader.len;
			
			break;
		}
		return NULL;
	}
	self->chunks = (char **) vec.data;
	
	//Now index lines
	mtc_vector_init(&vec);
	for(pos = 0; pos < self->n_chars; pos++)
	{
		if (MTC_SOURCE_NTH_CHAR(self, pos) == '\n')
		{
			mtc_vector_grow(&vec, sizeof(int));
			*mtc_vector_last(&vec, int) = pos;
			self->n_lines++;
		}
	}
	//Just to cope if the last line doesnt have newline character
	if (MTC_SOURCE_NTH_CHAR(self, pos - 1) != '\n')
	{
		mtc_vector_grow(&vec, sizeof(int));
		*mtc_vector_last(&vec, int) = pos;
		self->n_lines++;
	}
	self->lines = (int *) vec.data;
	
	return self;
}

MtcSource *mtc_source_new_from_file(const char *filename)
{
	int fd;
	MtcSource *self;
	
	fd = open(filename, O_RDONLY);
	if (fd < 0)
		return NULL;
	
	self = mtc_source_new_from_stream(filename, fd);
	
	close(fd);
	
	return self;
}

char *mtc_source_dup_line(MtcSource *self, int lineno)
{
	int start, end, src_iter;
	char *res, *res_iter;
	
	//Check whether line exists
	if (lineno >= self->n_lines)
		mtc_error("Assertion failure");
	
	//Calculate the range that we have to copy
	start = MTC_SOURCE_LINE_START(self, lineno);
	end = self->lines[lineno];
	
	//Allocate memory
	res_iter = res = mtc_alloc(end - start + 1);
	
	//Copy line
	for (src_iter = start; src_iter < end; src_iter++, res_iter++)
	{
		*res_iter = MTC_SOURCE_NTH_CHAR(self, src_iter);
	}
	
	//Copy in last null byte
	*res_iter = 0;
	
	return res;
}

//MtcSourcePtr
MtcSourcePtr *mtc_source_ptr_new
	(MtcSourcePtr *next, MtcSourceInvType inv_type, MtcSource *source,
	int lineno, int start, int len)
{
	MtcSourcePtr *res;
	
	res = (MtcSourcePtr *) mtc_alloc(sizeof(MtcSourcePtr));
	
	res->next = next;
	res->inv_type = inv_type;
	res->source = source;
	res->lineno = lineno;
	res->start = start;
	res->len = len;
	mtc_source_ref(source);
	
	return res;
}

MtcSourcePtr *mtc_source_ptr_copy(const MtcSourcePtr *sptr)
{
	MtcSourcePtr *res = NULL, *cur;
	MtcSourcePtr **next_ptr = &res;
	
	for (; sptr; sptr = sptr->next)
	{
		*next_ptr = cur = (MtcSourcePtr *)
			mtc_alloc(sizeof(MtcSourcePtr));
			
		*cur = *sptr;
		next_ptr = &(cur->next);
		mtc_source_ref(sptr->source);
	}
	
	if (res)
		cur->next = NULL;
	
	return res;
}

void mtc_source_ptr_free(MtcSourcePtr *sptr)
{
	MtcSourcePtr *bak;
	
	for (; sptr; sptr = bak)
	{
		bak = sptr->next;
		
		mtc_source_unref(sptr->source);
		mtc_free(sptr);
	}
}

void mtc_source_ptr_append
	(MtcSourcePtr *sptr, MtcSourcePtr *after, 
	MtcSourceInvType iface_inv)
{
	after->inv_type = iface_inv;
	
	for (; sptr->next; sptr = sptr->next)
		;
	
	sptr->next = after;
}

void mtc_source_ptr_append_copy
	(MtcSourcePtr *sptr, const MtcSourcePtr *after, 
	MtcSourceInvType iface_inv)
{
	mtc_source_ptr_append(sptr, mtc_source_ptr_copy(after), iface_inv);
}

void mtc_source_ptr_write(MtcSourcePtr *sptr, FILE *stream)
{
	//Should be with sync with MtcSourceInvType
	const char *inv_to_str[] =
	{
		"At ",
		"Alongwith ",
		"Macro expanded at ",
		"Included at ",
		"Reffered at "
	};
	
	for (; sptr; sptr = sptr->next)
	{
		int i, line_start, line_end;
		
		//Find out about the line
		line_end = sptr->source->lines[sptr->lineno];
		line_start = MTC_SOURCE_LINE_START(sptr->source, sptr->lineno);
		
		//Assertions
		if (sptr->start + sptr->len > line_end)
			mtc_error("Assertion failure");
		
		//Write about the source
		fprintf(stream, "%s %s line %d col %d:\n", 
			inv_to_str[sptr->inv_type],
			sptr->source->name, 
			sptr->lineno + 1, sptr->start);
		
		//Quote the line
		for (i = line_start; i < line_end; i++)
		{
			putc(MTC_SOURCE_NTH_CHAR(sptr->source, i), stream);
		}
		putc('\n', stream);
		
		//Mark the relevant portion
		for (i = line_start; i < line_start + sptr->start; i++)
		{
			char onechar = MTC_SOURCE_NTH_CHAR(sptr->source, i);
			char outchar;
			if (onechar == '\t')
				outchar = '\t';
			else 
				outchar = ' ';
			putc(outchar, stream);
		}
		for (i = 0; i < sptr->len; i++)
		{
			putc('^', stream);
		}
		putc('\n', stream);
	}
}

//MtcSourceMsgList

MtcSourceMsgList *mtc_source_msg_list_new()
{
	MtcSourceMsgList *self;
	
	self = (MtcSourceMsgList *) mtc_alloc(sizeof(MtcSourceMsgList));
	
	self->head = self->tail = NULL;
	self->error_count = self->warn_count = 0;

	return self;
}

void mtc_source_msg_list_add
	(MtcSourceMsgList *self, MtcSourcePtr *location, 
	MtcSourceMsgType type, const char *format, ...)
{
	char *message;
	size_t msg_len;
	FILE *stream;
	va_list arglist;
	MtcSourceMsg *msg;
	
	//Create a memory stream
	stream = open_memstream(&message, &msg_len);
	
	//Write message to it
	va_start(arglist, format);
	vfprintf(stream, format, arglist);
	va_end(arglist);
	
	//Close stream
	fclose(stream);
	
	//Create message
	msg = (MtcSourceMsg *) mtc_alloc(sizeof(MtcSourceMsg));
	msg->location = mtc_source_ptr_copy(location); 
	msg->type = type;
	msg->message = message;
	
	//Add it to the queue
	if (self->head)
		self->tail->next = msg;
	else 
		self->head = msg;
	self->tail = msg;
	msg->next = NULL;
	
	//Update the counts
	if (type == MTC_SOURCE_MSG_ERROR)
		self->error_count++;
	else
		self->warn_count++;
}

void mtc_source_msg_list_dump(MtcSourceMsgList *self, FILE *stream)
{
	MtcSourceMsg *iter;
	
	for (iter = self->head; iter; iter = iter->next)
	{
		//Separate output from previous
		putc('\n', stream);
		
		//Say where this message came from 
		mtc_source_ptr_write(iter->location, stream);
		
		//Is this an error or a warning?
		if (iter->type == MTC_SOURCE_MSG_ERROR)
			fputs("Error: ", stream);
		else
			fputs("Warning: ", stream);
		
		//Write the message
		fputs(iter->message, stream);
		
		//A newline
		putc('\n', stream);
	}
	
	//Add a small summary
	if (self->error_count || self->warn_count)
		fprintf(stream, "Summary: %d error(s), %d warning(s)\n", 
			self->error_count, self->warn_count);
}

void mtc_source_msg_list_free(MtcSourceMsgList *self)
{
	MtcSourceMsg *iter, *bak;
	
	for (iter = self->head; iter; iter = bak)
	{
		//Save the next pointer
		bak = iter->next;
		
		//Destroy the location
		mtc_source_ptr_free(iter->location);
		
		//Destroy the message
		free(iter->message);
		
		//Destroy the message
		mtc_free(iter);
	}
	
	//Destroy this
	mtc_free(self);
}	 
