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

#include <ctype.h>
#include <stdarg.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>


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
		if (self->chars)
			mtc_free(self->chars);
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
	MtcVector vec;
	int pos;
	
	//Allocate and initialize
	self = mtc_alloc(sizeof(MtcSource));
	self->refcount = 1;
	self->name = mtc_strdup(name);
	self->chars = NULL;
	self->n_chars = 0;
	self->lines = NULL;
	self->n_lines = 0;
	
	//Read contents of the stream
	{
		off_t res;
		
		if ((res = lseek(fd, 0, SEEK_END)) < 0)
			mtc_error("lseek: %s", strerror(errno));
		
		self->n_chars = res;
		self->chars = (char *) mtc_alloc(res + 1);
		
		if (lseek(fd, 0, SEEK_SET) < 0)
			mtc_error("lseek: %s", strerror(errno));
		
		if (read(fd, self->chars, self->n_chars) != self->n_chars)
			mtc_error("read: %s", strerror(errno));
		
		if (self->chars[self->n_chars - 1] != '\n')
		{
			self->chars[self->n_chars] = '\n';
			self->n_chars++;
		}
	}
	
	//Now index lines
	mtc_vector_init(&vec);
	mtc_vector_grow(&vec, sizeof(int));
	*mtc_vector_last(&vec, int) = 0;
	for(pos = 0; pos < self->n_chars; pos++)
	{
		if (self->chars[pos] == '\n')
		{
			mtc_vector_grow(&vec, sizeof(int));
			*mtc_vector_last(&vec, int) = pos + 1;
			self->n_lines++;
		}
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

int mtc_source_get_lineno(MtcSource *self, int pos)
{
	int left = 0;
	int right = self->n_lines;
	
	if ((pos >= self->n_chars) || (pos < 0))
		mtc_error("pos out of range");
	
	while ((right - left) > 1)
	{
		int mid = (left + right) / 2;
		
		if (pos < self->lines[mid])
			right = mid;
		else
			left = mid;
	}
	
	return left;
}

//TODO: null bytes?
char *mtc_source_dup_line(MtcSource *self, int lineno)
{
	int start, len;
	char *res;
	
	//Check whether line exists
	if (lineno >= self->n_lines)
		mtc_error("Assertion failure");
	
	//Calculate the range that we have to copy
	start = MTC_SOURCE_LINE_START(self, lineno);
	len = MTC_SOURCE_LINE_LEN(self, lineno);
	
	//Allocate and copy
	res = mtc_alloc(len + 1);
	memcpy(res, self->chars + start, len);
	res[len] = '\0';
	
	return res;
}

//MtcSourcePtr
MtcSourcePtr *mtc_source_ptr_new
	(MtcSourcePtr *next, MtcSourceInvType inv_type, MtcSource *source,
	int start, int len)
{
	MtcSourcePtr *res;
	
	res = (MtcSourcePtr *) mtc_alloc(sizeof(MtcSourcePtr));
	
	res->next = next;
	res->inv_type = inv_type;
	res->source = source;
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
		int start_line, end_line, disp_start, disp_lim, i, j, line_beg;
		
		start_line = mtc_source_get_lineno(sptr->source, sptr->start);
		disp_start = MTC_SOURCE_LINE_START(sptr->source, start_line);
		end_line = mtc_source_get_lineno
			(sptr->source, sptr->start + sptr->len);
		disp_lim = MTC_SOURCE_LINE_START(sptr->source, end_line + 1);
		
		//Write about the source
		fprintf(stream, "%s %s line %d col %d:\n", 
			inv_to_str[sptr->inv_type],
			sptr->source->name, 
			start_line + 1, sptr->start - disp_start);
		
		//Quote line
		for (line_beg = i = disp_start; i < disp_lim; i++)
		{
			int onechar = sptr->source->chars[i];
			putc(onechar, stream);
			
			//Mark relevant portion
			if (onechar == '\n')
			{
				for (j = line_beg; j <= i; j++)
				{
					onechar = sptr->source->chars[j];
					if (isspace(onechar) && onechar != ' ')
						putc(onechar, stream);
					else if ((j >= sptr->start)
						&& (j < (sptr->start + sptr->len)))
						putc('^', stream);
					else
						putc(' ', stream);
				}
				line_beg = j;
			}
		}
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
		mtc_free(iter->message);
		
		//Destroy the message
		mtc_free(iter);
	}
	
	//Destroy this
	mtc_free(self);
}	 
