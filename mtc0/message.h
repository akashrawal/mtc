/* message.h
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
 
/**
 * \addtogroup mtc_msg
 * \{
 * 
 * A message is one container for 'dual stream' serialized data.
 */

///A structure representing a message.
typedef struct
{
	//Reference count
	int refcount;
	
	uint32_t n_blocks;
	MtcMBlock blocks[];
} MtcMsg;

/**Gets the number of memory blocks inside the message. This number is
 * the length of the block stream plus one. 
 * \param self The message
 * \return The number of memory blocks inside the message.
 */
size_t mtc_msg_get_n_blocks(MtcMsg *self);

/**Gets the memory blocks inside the message. 
 * The first memory block contains the byte stream, the subsequent ones
 * form the block stream. 
 * \param self The message
 * \return The array of memory blocks inside the message.
 */
MtcMBlock *mtc_msg_get_blocks(MtcMsg *self);

/**Enables iteration over the message in the 'dual stream' style. 
 * \param self The message
 * \param dstream Pointer to the 'dual stream' structure
 */
void mtc_msg_iter(MtcMsg *self, MtcDStream *dstream);

/**Creates a new message.
 * \param n_bytes Size of the byte stream
 * \param n_blocks Size of the block stream
 */
MtcMsg *mtc_msg_new(size_t n_bytes, size_t n_blocks);

/* Tries to create a new message, fully allocated. Ignores block_sizes
 * if n_blocks = 0. on failure it returns NULL.
 */
MtcMsg *mtc_msg_try_new_allocd
	(size_t n_bytes, size_t n_blocks, uint32_t *block_sizes);

/**Increments the reference count of the message by 1
 * \param self The message
 */        
void mtc_msg_ref(MtcMsg *self);

/**Decrements the reference count of the message by 1. If reference count
 * drops to 0, the message is destroyed.
 * \param self The message
 */
void mtc_msg_unref(MtcMsg *self);



/**Signals are integers that have special meanings when transmitted over
 * a link. 
 */
typedef enum
{
	///No problem
	MTC_SIGNAL_OK = 0,
	///MTC traffic over
	MTC_SIGNAL_STOP = 1,
	///Make whatever you are doing fail straightaway
	MTC_SIGNAL_FAIL = 2
} MtcSignum;

///\}
