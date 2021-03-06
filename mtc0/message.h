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
	
	void *bs_ref;
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

/**Counts the amount of dynamic data inside message. 
 * Used by mdlc generated serializers.
 * \param self The message
 * \return Dynamic size of the message
 */
MtcDLen mtc_msg_count(MtcMsg *self);

/**Serializes a message into a 'dual stream'. 
 * \param self The message to serialize
 * \param segment Current segment
 * \param dstream The dual stream to serialize data to
 */
void mtc_msg_write
	(MtcMsg *self, MtcSegment *segment, MtcDStream *dstream);

/**Deserializes a message from a 'dual stream'. 
 * \param segment Current segment
 * \param dstream The dual stream to deserialize data from
 * \return Deserialized message, or NULL if deserialization failed
 */
MtcMsg *mtc_msg_read(MtcSegment *segment, MtcDStream *dstream);

/**
 * \}
 */
