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
//TODO: Events to be implemented later
/*
#define MTC_MEMBER_PTR_EVT_CONNECT        ((uint32_t) 3L << 28)
#define MTC_MEMBER_PTR_EVT_CONNECTED      ((uint32_t) 4L << 28)
#define MTC_MEMBER_PTR_EVT_DISCONNECT     ((uint32_t) 5L << 28)
#define MTC_MEMBER_PTR_EVT_RAISE          ((uint32_t) 6L << 28)
*/
#define MTC_MEMBER_PTR_ERROR              ((uint32_t) 7L << 28)
#define MTC_MEMBER_PTR_MASK               ((uint32_t) 15L << 28)

#define MTC_MEMBER_PTR_IS_FN(ptr) \
	(((ptr) & MTC_MEMBER_PTR_MASK) == MTC_MEMBER_PTR_FN)

/*
#define MTC_MEMBER_PTR_IS_EVT_CONNECT(ptr) \
	(((ptr) & MTC_MEMBER_PTR_MASK) == MTC_MEMBER_PTR_EVT)
	*/

#define MTC_MEMBER_PTR_GET_IDX(ptr) \
	((ptr) & (~ MTC_MEMBER_PTR_MASK))

uint32_t mtc_msg_read_member_ptr(MtcMsg *msg);
MtcMsg *mtc_msg_with_member_ptr_only(uint32_t member_ptr);

//TODO: Delete
/*
typedef enum
{
	MTC_DA_FLAG_MSG_IN_REPLY = 1 << 0
} MtcDAFlags;
*/

//TODO: These require a rewrite
#define MTC_READ_ARGS(fn, msg, argp, handle, peer, reply_to) \
do { \
	if (fn ## __read(msg, argp) < 0) \
	{ \
		mtc_link_queue_signal \
			(peer->link, reply_to, handle->parent.dest_id, MTC_SIGNAL_FAIL); \
		return; \
	} \
} while (0)

#define MTC_READ_ARGS_STD(fn) \
do { \
	if (fn ## __read(msg, &args) < 0) \
	{ \
		mtc_link_queue_signal \
			(peer->link, reply_to, handle->parent.dest_id, MTC_SIGNAL_FAIL); \
		return; \
	} \
} while (0)

typedef struct 
{
	int n_fns;
	int n_evts;
} MtcClassInfo;

//TODO: Delete
/*
#define MTC_EVENT_TAG_CREATE(id, msg_in_reply) \
	((((uint32_t) (id)) << 1) | ((uint32_t) (msg_in_reply)))

#define MTC_EVENT_TAG_GET_ID(tag) ((tag) >> 1)

#define MTC_EVENT_TAG_GET_MSG_IN_REPLY(tag) ((tag) & 1)
*/

/**
 * \}
 */
