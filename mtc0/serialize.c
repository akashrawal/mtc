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
	len = strlen(val) + 1;
	block->mem = mtc_rcmem_dup((void *) val, len);
	block->size = len;
}

char *mtc_segment_read_string(MtcSegment *seg)
{
	MtcMBlock *block;
	int len;
	char *ch, *lim, *res;
	
	//Fetch it
	block = seg->blocks;
	len = block->size;
	
	//Verify...
	res = ch = (char *) block->mem;
	lim = ch + len - 1;
	for(; ch < lim ? *ch : 0; ch++)
		;
	if (ch != lim)
		return NULL;
	
	//Increment
	seg->blocks++;

	//Reference count
	mtc_rcmem_ref(block->mem);
	
	return res;
}

//'raw' type
void mtc_segment_write_raw(MtcSegment *seg, MtcMBlock val)
{
	MtcMBlock *block;
	
	block = seg->blocks;
	seg->blocks++;
	
	*block = val;
	mtc_rcmem_ref(val.mem);
}

void mtc_segment_read_raw(MtcSegment *seg, MtcMBlock *val)
{
	MtcMBlock *block;
	
	block = seg->blocks;
	seg->blocks++;
	
	*val = *block;
	mtc_rcmem_ref(block->mem);
}

