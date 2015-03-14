/* message.c
 * Support for creation and reading of message
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

size_t mtc_msg_get_n_blocks(MtcMsg *self)
{
	return self->n_blocks;
}

MtcMBlock *mtc_msg_get_blocks(MtcMsg *self)
{
	return self->blocks;
}

void mtc_msg_iter(MtcMsg *self, MtcDStream *dstream)
{
	dstream->bytes = self->blocks->data;
	dstream->bytes_lim = dstream->bytes + self->blocks->len;
	dstream->blocks = self->blocks + 1;
	dstream->blocks_lim = self->blocks + self->n_blocks;
}

MtcMsg *mtc_msg_new(size_t n_bytes, size_t n_blocks)
{
	MtcMsg *self;
	void *byte_stream;
	
	MtcMBlock *m_iter, *m_lim;
	
	//Allocate memory
	self = mtc_alloc2(sizeof(MtcMsg) + (n_blocks * sizeof(MtcMBlock)),
	                  n_bytes, &byte_stream);
	                  
	//Initialize 'byte stream'
	self->blocks[0].data = byte_stream;
	self->blocks[0].len = n_bytes;
	self->blocks[0].free_func = NULL;
	self->blocks[0].free_func_data = NULL;
	
	//Initialize other members
	self->n_blocks = n_blocks + 1;
	self->refcount = 1;
	self->da_flags = 0;
	
	//Initialize memory block array
	m_iter = self->blocks + 1;
	m_lim = self->blocks + n_blocks + 1;
	
	while(m_iter < m_lim)
	{
		m_iter->data = NULL;
		m_iter->len = 0;
		m_iter->free_func = NULL;
		m_iter->free_func_data = NULL;
		
		m_iter++;
	}
	
	return self;
}

MtcMsg *mtc_msg_try_new_allocd(size_t n_bytes, size_t n_blocks, 
		uint32_t *block_sizes)
{
	MtcMsg *self;
	void *byte_stream;
	
	MtcMBlock *m_iter, *m_lim;
	
	//Allocate memory
	self = mtc_tryalloc2(sizeof(MtcMsg) + (n_blocks * sizeof(MtcMBlock)),
	                  n_bytes, &byte_stream);
	if (! self)
		return NULL;
	                  
	//Assign main block
	self->blocks[0].data = byte_stream;
	self->blocks[0].len = n_bytes;
	self->blocks[0].free_func = NULL;
	self->blocks[0].free_func_data = NULL;
	
	//Initialize other members
	self->n_blocks = n_blocks + 1;
	self->refcount = 1;
	self->da_flags = 0;
	
	//Allocate other memory blocks
	m_iter = self->blocks + 1;
	m_lim = m_iter + n_blocks;
	
	while(m_iter < m_lim)
	{
		m_iter->data = mtc_alloc(*block_sizes + 1);
		m_iter->len = *block_sizes;
		m_iter->free_func = mtc_free;
		m_iter->free_func_data = m_iter->data;
		
		if (! m_iter->data)
		{
			for (m_iter--; m_iter > self->blocks; m_iter--)
				mtc_free(m_iter->data);
			
			mtc_free(self);
			return NULL;
		}
		
		m_iter++;
		block_sizes++;
	}
	
	return self;
}

void mtc_msg_ref(MtcMsg *self)
{
	self->refcount++;
}

void mtc_msg_unref(MtcMsg *self)
{
	self->refcount--;
	
	if (! self->refcount)
	{
		MtcMBlock *m_iter, *m_lim;
		
		m_iter = self->blocks + 1;
		m_lim = self->blocks + self->n_blocks;
		while(m_iter < m_lim)
		{
			if (m_iter->data && m_iter->free_func)
				(* m_iter->free_func)(m_iter->free_func_data);
			m_iter++;
		}
		
		mtc_free(self);
	}
}

MtcDLen mtc_msg_count(MtcMsg *self)
{
	MtcDLen res;
	
	res.n_bytes = 0;
	res.n_blocks = self->n_blocks;
	
	return res;
}

void mtc_msg_write
	(MtcMsg *self, MtcSegment *segment, MtcDStream *dstream)
{
	int i;
	MtcSegment sub_seg;
	uint32_t n_blocks;
	
	mtc_msg_ref(self);
	
	//Write the number of blocks
	n_blocks = self->n_blocks;
	mtc_segment_write_uint32(segment, n_blocks);
	
	//Add the memory blocks
	mtc_dstream_get_segment(dstream, 0, n_blocks, &sub_seg);
	for (i = 0; i < n_blocks; i++)
	{
		sub_seg.blocks[i].data = self->blocks[i].data;
		sub_seg.blocks[i].len  = self->blocks[i].len;
		sub_seg.blocks[i].free_func = NULL;
		sub_seg.blocks[i].free_func_data = NULL;
	}
	sub_seg.blocks[0].free_func = (MtcMFunc) mtc_msg_unref;
	sub_seg.blocks[0].free_func_data = self;
}

MtcMsg *mtc_msg_read(MtcSegment *segment, MtcDStream *dstream)
{
	int i;
	MtcSegment sub_seg;
	uint32_t n_blocks;
	MtcMsg *self;
	
	//Read the number of blocks
	mtc_segment_read_uint32(segment, n_blocks);
	
	//Get the memory blocks
	if (mtc_dstream_get_segment(dstream, 0, n_blocks, &sub_seg) < 0)
		return NULL;
	
	//Allocate memory for message
	self = (MtcMsg *) mtc_tryalloc
		(sizeof(MtcMsg) + (n_blocks * sizeof(MtcMBlock)));
	if (! self)
		return NULL;
		
	//Copy in all blocks
	for (i = 0; i < n_blocks; i++)
	{
		self->blocks[i] = sub_seg.blocks[i];
		sub_seg.blocks[i].data = NULL;
	}
	
	return self;
}
