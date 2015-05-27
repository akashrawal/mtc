/* hl_base.c
 * All the highlevel serialization stuff
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

uint32_t mtc_msg_read_member_ptr(MtcMsg *msg)
{
	MtcMBlock byte_stream;
	uint32_t res;
	
	byte_stream = mtc_msg_get_blocks(msg)[0];
	
	if (byte_stream.len < 4)
		return MTC_MEMBER_PTR_NULL;
	
	mtc_uint32_copy_from_le(byte_stream.data, &res);
	
	return res;
}

MtcMsg *mtc_msg_with_member_ptr_only(uint32_t member_ptr)
{
	MtcMsg *msg;
	MtcMBlock byte_stream;
	
	msg = mtc_msg_new(4, 0);
	
	byte_stream = mtc_msg_get_blocks(msg)[0];
	
	mtc_uint32_copy_to_le(byte_stream.data, &member_ptr);
	
	return msg;
}
