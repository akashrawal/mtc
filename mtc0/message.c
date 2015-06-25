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
	dstream->bytes = self->blocks->mem;
	dstream->bytes_lim = dstream->bytes + self->blocks->size;
	dstream->blocks = self->blocks + 1;
	dstream->blocks_lim = self->blocks + self->n_blocks;
}

MtcMsg *mtc_msg_new(size_t n_bytes, size_t n_blocks)
{
	MtcMsg *self;
	void *byte_stream;
	size_t msg_offset;
	
	MtcMBlock *m_iter, *m_lim;
	
	//Allocate memory
	msg_offset = mtc_offset_align(n_bytes);
	byte_stream = mtc_rcmem_alloc
		(msg_offset + sizeof(MtcMsg) 
		+ ((n_blocks + 1) * sizeof(MtcMBlock)));
	self = MTC_PTR_ADD(byte_stream, msg_offset);
	                  
	//Initialize 'byte stream'
	self->blocks[0].mem = byte_stream;
	self->blocks[0].size = n_bytes;
	mtc_rcmem_ref(byte_stream);
	
	//Initialize other members
	self->n_blocks = n_blocks + 1;
	self->refcount = 1;
	self->bs_ref = byte_stream;
	
	//Initialize memory block array
	m_iter = self->blocks + 1;
	m_lim = self->blocks + n_blocks + 1;
	
	while(m_iter < m_lim)
	{
		m_iter->mem = NULL;
		m_iter->size = 0;
		
		m_iter++;
	}
	
	return self;
}

MtcMsg *mtc_msg_try_new_allocd(size_t n_bytes, size_t n_blocks, 
		uint32_t *block_sizes)
{
	MtcMsg *self;
	void *byte_stream;
	size_t msg_offset;
	
	MtcMBlock *m_iter, *m_lim;
	
	//Allocate memory
	msg_offset = mtc_offset_align(n_bytes);
	byte_stream = mtc_rcmem_tryalloc
		(msg_offset + sizeof(MtcMsg) 
		+ ((n_blocks + 1) * sizeof(MtcMBlock)));
	if (! byte_stream)
		return NULL;
	self = MTC_PTR_ADD(byte_stream, msg_offset);
	                  
	//Initialize 'byte stream'
	self->blocks[0].mem = byte_stream;
	self->blocks[0].size = n_bytes;
	mtc_rcmem_ref(byte_stream);
	
	//Initialize other members
	self->n_blocks = n_blocks + 1;
	self->refcount = 1;
	self->bs_ref = byte_stream;
	
	//Allocate other memory blocks
	m_iter = self->blocks + 1;
	m_lim = m_iter + n_blocks;
	
	while(m_iter < m_lim)
	{
		m_iter->mem = mtc_rcmem_tryalloc(*block_sizes);
		m_iter->size = *block_sizes;
		
		if (! m_iter->mem)
		{
			for (m_iter--; m_iter > self->blocks; m_iter--)
				mtc_rcmem_unref(m_iter->mem);
			
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
		
		m_iter = self->blocks;
		m_lim = self->blocks + self->n_blocks;
		while(m_iter < m_lim)
		{
			mtc_rcmem_unref(m_iter->mem);
			m_iter++;
		}
		
		if (self->bs_ref)
			mtc_rcmem_unref(self->bs_ref);
		else
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
	
	//Write the number of blocks
	n_blocks = self->n_blocks;
	mtc_segment_write_uint32(segment, n_blocks);
	
	//Add the memory blocks
	mtc_dstream_get_segment(dstream, 0, n_blocks, &sub_seg);
	for (i = 0; i < n_blocks; i++)
	{
		sub_seg.blocks[i] = self->blocks[i];
		mtc_rcmem_ref(self->blocks[i].mem);
	}
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
	
	//Initialize other members
	self->n_blocks = n_blocks;
	self->refcount = 1;
	self->bs_ref = NULL;
		
	//Copy in all blocks
	for (i = 0; i < n_blocks; i++)
	{
		self->blocks[i] = sub_seg.blocks[i];
		mtc_rcmem_ref(sub_seg.blocks[i].mem);
	}
	
	return self;
}
