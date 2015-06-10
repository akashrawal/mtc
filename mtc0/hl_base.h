/* hl_base.h
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

/**
 * \addtogroup mtc_hl
 * \{
 * 
 * This section documents functions and macros useful for using MTC to
 * handle highlevel stuff like function calls and event registration
 */

//The first 4 bytes is an integer referring to a function or event.
//Called member pointer

#define MTC_MEMBER_PTR_NULL               ((uint32_t) 0)
#define MTC_MEMBER_PTR_FN                 ((uint32_t) 1L << 28)
#define MTC_MEMBER_PTR_FN_RETURN          ((uint32_t) 2L << 28)
#define MTC_MEMBER_PTR_ERROR              ((uint32_t) 3L << 28)
#define MTC_MEMBER_PTR_MASK               ((uint32_t) 15L << 28)

#define MTC_MEMBER_PTR_IS_FN(ptr) \
	(((ptr) & MTC_MEMBER_PTR_MASK) == MTC_MEMBER_PTR_FN)

#define MTC_MEMBER_PTR_IS_ERROR(ptr) \
	(((ptr) & MTC_MEMBER_PTR_MASK) == MTC_MEMBER_PTR_ERROR)

#define MTC_MEMBER_PTR_GET_IDX(ptr) \
	((ptr) & (~ MTC_MEMBER_PTR_MASK))
	
//TODO: Add enums as needed
typedef enum
{	
	MTC_ERROR_TEMP = -1,
	MTC_FC_SUCCESS = -2,
	
	MTC_ERROR_INVALID_PEER = 1,
	MTC_ERROR_PEER_RESET = 2,
	MTC_ERROR_INVALID_ARGUMENT = 3
} MtcStatus;

uint32_t mtc_msg_read_member_ptr(MtcMsg *msg);

MtcMsg *mtc_msg_with_member_ptr_only(uint32_t member_ptr);

typedef MtcMsg *(*MtcSerFn) (void *strx);
typedef int (*MtcDeserFn)   (MtcMsg *msg, void *strx);
typedef void (*MtcFreeFn)   (void *strx);

typedef struct 
{
	int        id;
	size_t     in_args_c_size;
	MtcSerFn   in_args_ser;
	MtcDeserFn in_args_deser;
	MtcFreeFn  in_args_free;
	size_t     out_args_c_size;
	MtcSerFn   out_args_ser;
	MtcDeserFn out_args_deser;
	MtcFreeFn  out_args_free;
} MtcFCBinary;

typedef struct 
{
	int             n_fns;
	MtcFCBinary     *fns;
} MtcClassBinary;

/**
 * \}
 */
