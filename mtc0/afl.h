/* afl.h
 * A data structure with fast key lookup capablity
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

//TODO: Discuss whether to keep this API exposed?

/**\addtogroup mtc_afl
 * \{
 * MtcAfl is simply a data structure for storing items, 
 * each item is uniquely identified by an integer.
 * 
 * That integer is determined at time of insertion, you have no 
 * control over it.
 */

/**An item that can be added to MtcAfl. 
 * To store data, derrive a structure from this and
 * put your data there. Only public members are shown, 
 * and they all are read-only.
 */
typedef struct _MtcAflItem MtcAflItem;
struct _MtcAflItem
{
	///The key assigned to the item
	unsigned int key;
	
	#ifndef DOXYGEN_SKIP_THIS
	MtcAflItem *next;
	#endif
};

///The data structure.
typedef struct
{
	MtcAflItem **table;
	MtcAflItem **free_list;
	size_t alloc_len;
	unsigned int n_elements;
} MtcAfl;

/**Initializes the data structure
 * \param self The data structure. 
 */
void mtc_afl_init(MtcAfl *self);

/**Searches for a key in the data structure. 
 * If found, the corresponding item is returned or NULL is returned.
 * \param self The data structure.
 * \param key The key to search.
 * \return The item corresponding to the key if found, else NULL.
 */
MtcAflItem *mtc_afl_lookup(MtcAfl *self, unsigned int key);

/**Inserts an item into the data structure.
 * You store data by derriving a structure from MtcAflItem and 
 * filling your data members in it like:
 * ~~~~~{.c}
 * struct _MyStruct
 * {
 *     MtcAflItem parent;
 *     .... //your data members
 * };
 * ~~~~~
 * You can use any way to allocate the item, only condition is that
 * the item should exist till it is removed. And never insert an item
 * twice.
 * 
 * On insertion a key is assigned to the data structure which can be 
 * found out at item->key. You should not change it.
 * \param self The data structure
 * \param item the item to insert
 * \return The key assigned to the item
 */
unsigned int mtc_afl_insert(MtcAfl *self, MtcAflItem *item);

/**Removes an item associated with the given key.
 * \param self The data structure
 * \param key The key
 * \return The item if found and removed, or NULL.
 */
MtcAflItem *mtc_afl_remove(MtcAfl *self, unsigned int key);

/**Replaces an item associated with the given key with given 
 * replacement.
 * \param self The data structure
 * \param key The key
 * \param replacement The replacement structure
 * \return The item if found and removed, or NULL.
 */
MtcAflItem *mtc_afl_replace
	(MtcAfl *self, unsigned int key, MtcAflItem *replacement);

/**Returns the number of items in the data structure.
 * \param self The data structure.
 * \return No. of items in the data structure
 */
unsigned int mtc_afl_get_n_items(MtcAfl *self);

/**Gets a list of all inserted items.
 * \param self The data structure.
 * \param items An array large enough to hold pointers to all
 *              items. See mtc_afl_get_n_items(). 
 */
void mtc_afl_get_items(MtcAfl *self, MtcAflItem **items);

/**Destroys the data structure.
 * This doesn't do anything to the items since the data structure 
 * didn't allocate them.
 * \param self The data structure
 */
void mtc_afl_destroy(MtcAfl *self);



/* Dumps the internals of the data structure for debugging the library.
 * This function is not included in API.
 * \param self The data structure
 */
void mtc_afl_dump(MtcAfl *self);

/**
 * \}
 */
