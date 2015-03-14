/* serialize.c
 * Support for serializing basic types
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
/* ********************************************************************/ 

#include "common.h"

int mtc_dstream_get_segment
	(MtcDStream *self, size_t n_bytes, size_t n_blocks, 
	 MtcSegment *res)
{
	if (self->bytes + n_bytes > self->bytes_lim
		|| self->blocks + n_blocks > self->blocks_lim)
		return -1;
	
	res->bytes = self->bytes;
	res->blocks = self->blocks;
	self->bytes += n_bytes;
	self->blocks += n_blocks;
	
	return 0;
}

//Floating point values
void mtc_segment_write_flt32(MtcSegment *seg, MtcValFlt val)
{
	uint32_t inter;
	
	switch (val.type)
	{
	case MTC_FLT_ZERO:
	case MTC_FLT_NORMAL:
		inter = mtc_double_to_flt32(val.val);
		break;
	case MTC_FLT_NAN:
		inter = mtc_flt32_nan;
		break;
	case MTC_FLT_INFINITE:
		inter = mtc_flt32_infinity;
		break;
	case MTC_FLT_NEG_INFINITE:
		inter = mtc_flt32_neg_infinity;
	default:
		mtc_error("Invalid type %d", val.type);
	}
	
	mtc_segment_write_uint32(seg, inter);
}

void mtc_segment_read_flt32(MtcSegment *seg, MtcValFlt *val)
{
	uint32_t inter;
	
	mtc_segment_read_uint32(seg, inter);
	val->type = mtc_flt32_classify(inter);
	val->val = mtc_double_from_flt32(inter);
}

void mtc_segment_write_flt64(MtcSegment *seg, MtcValFlt val)
{
	uint64_t inter;
	
	switch (val.type)
	{
	case MTC_FLT_ZERO:
	case MTC_FLT_NORMAL:
		inter = mtc_double_to_flt64(val.val);
		break;
	case MTC_FLT_NAN:
		inter = mtc_flt64_nan;
		break;
	case MTC_FLT_INFINITE:
		inter = mtc_flt64_infinity;
		break;
	case MTC_FLT_NEG_INFINITE:
		inter = mtc_flt64_neg_infinity;
	default:
		mtc_error("Invalid type %d", val.type);
	}
	
	mtc_segment_write_uint64(seg, inter);
}

void mtc_segment_read_flt64(MtcSegment *seg, MtcValFlt *val)
{
	uint64_t inter;
	
	mtc_segment_read_uint64(seg, inter);
	val->type = mtc_flt64_classify(inter);
	val->val = mtc_double_from_flt64(inter);
}

//Strings

void mtc_segment_write_string(MtcSegment *seg, char *val)
{
	MtcMBlock *block;
	size_t len;
		
	block = seg->blocks;
	seg->blocks++;
	
	//Copy memory
	len = strlen(val);
	block->data = mtc_memdup((void *) val, len + 1);
	block->len = len;
	block->free_func = mtc_free;
	block->free_func_data = block->data;
}

char *mtc_segment_read_string(MtcSegment *seg)
{
	MtcMBlock *block;
	int len;
	char *ch, *lim, *res;
	
	//Fetch it
	block = seg->blocks;
	len = block->len;
	
	//Verify...
	res = ch = (char *) block->data;
	lim = ch + len;
	for(; ch < lim ? *ch : 0; ch++)
		;
	if (ch != lim)
		return NULL;
	
	//Increment
	seg->blocks++;

	//Take ownership of the memory block
	block->data = NULL;
	
	return res;
}

//Memory reference
const MtcMRef mtc_m_ref_dumb = {NULL, NULL};

//'raw' type
void mtc_segment_write_raw(MtcSegment *seg, MtcValRaw val)
{
	MtcMBlock *block;
	
	block = seg->blocks;
	seg->blocks++;
	//Whether to share memory
	if (val.ref)
	{
		block->data = val.mem;
		block->len = val.size;
		block->free_func = val.ref->unref_func;
		block->free_func_data = val.data;
		//Call the ref function if available
		if (val.ref->ref_func)
			(* val.ref->ref_func)(val.data);
	}
	else
	{	
		//Copy memory
		block->data = mtc_memdup(val.mem, val.size);
		block->len = val.size;
		block->free_func = mtc_free;
		block->free_func_data = block->data;
	}
}
void mtc_segment_read_raw(MtcSegment *seg, MtcValRaw *val)
{
	MtcMBlock *block;
	
	block = seg->blocks;
	seg->blocks++;
	val->mem = block->data;
	val->size = block->len;
	val->ref = val->data = NULL;
	
	//Take ownership of the memory block
	block->data = NULL;
}

//Header used by MtcFDLink

//serialize the header for a message
void mtc_header_write_for_msg
	(MtcHeaderBuf *buf, uint64_t dest, uint64_t reply_to, 
	MtcMBlock *blocks, uint32_t n_blocks)
{
	char *buf_c = (char *) buf;
	char *data_iter, *data_lim;
	
	buf_c[0] = 'M';
	buf_c[1] = 'T';
	buf_c[2] = 'C';
	buf_c[3] = 0;
	mtc_uint32_copy_to_le(buf_c + 4, &n_blocks);
	mtc_uint64_copy_to_le(buf_c + 8, &dest);
	mtc_uint64_copy_to_le(buf_c + 16, &reply_to);
	
	data_iter = buf_c + 24;
	data_lim = data_iter + (n_blocks * 4);
	for (; data_iter < data_lim; data_iter += 4, blocks++)
	{
		mtc_uint32_copy_to_le(data_iter, &(blocks->len));
	}
}

//serialize the header for a signal
void mtc_header_write_for_signal
	(MtcHeaderBuf *buf, uint64_t dest, uint64_t reply_to, 
	uint32_t signum)
{
	char *buf_c = (char *) buf;
	
	buf_c[0] = 'M';
	buf_c[1] = 'T';
	buf_c[2] = 'C';
	buf_c[3] = 0;
	buf_c[4] = 0;
	buf_c[5] = 0;
	buf_c[6] = 0;
	buf_c[7] = 0;
	mtc_uint64_copy_to_le(buf_c + 8, &dest);
	mtc_uint64_copy_to_le(buf_c + 16, &reply_to);
	mtc_uint32_copy_to_le(buf_c + 24, &signum);
}

//Deserialize the message header
int mtc_header_read(MtcHeaderBuf *buf, MtcHeaderData *res)
{
	char *buf_c = (char *) buf;
	
	if (buf_c[0] != 'M' 
		|| buf_c[1] != 'T'
		|| buf_c[2] != 'C'
		|| buf_c[3] != 0)
		return 0;
	mtc_uint32_copy_from_le(buf_c + 4, &(res->size));
	mtc_uint64_copy_from_le(buf_c + 8, &(res->dest));
	mtc_uint64_copy_from_le(buf_c + 16, &(res->reply_to));
	mtc_uint32_copy_from_le(buf_c + 24, &(res->data_1));
	
	return 1;
}
