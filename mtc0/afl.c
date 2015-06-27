/* afl.c
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

#include "common.h"


#define MTC_AFL_INIT_SIZE 16

#define _mtc_ptr_is_free(ptr, table, alloc_len) \
	((((char *) (ptr)) >= ((char *) (table))) && (((char *) (ptr)) <= ((char *) ((table) + (alloc_len)))))

static void expand_table(MtcAfl *self)
{
	size_t new_alloc_len;
	int i;
	
	//Double the size of the table
	new_alloc_len = self->alloc_len * 2;
	self->table = (MtcAflItem **) 
		mtc_realloc(self->table, (new_alloc_len + 1) * sizeof(void *));
	for (i = self->alloc_len; i <= new_alloc_len; i++)
		self->table[i] = NULL;
	
	//Now rescatter the items
	for (i = 0; i < self->alloc_len; i++)
	{
		MtcAflItem *iter, *next;
		
		iter = self->table[i];
		self->table[i] = NULL;
		
		for (; iter; iter = next)
		{
			next = iter->next;
			
			iter->next = self->table[iter->key % new_alloc_len];
			self->table[iter->key % new_alloc_len] = iter;
		}
	}
	
	//Update
	self->alloc_len = new_alloc_len;
	
	//Regenerate free list
	self->free_list = NULL;
	for (i = new_alloc_len; i >= 0; i--)
	{
		if (self->table[i] == NULL)
		{
			self->table[i] = (MtcAflItem *) self->free_list;
			self->free_list = self->table + i;
		}
	}
}

static void collapse_table(MtcAfl *self)
{
	MtcAflItem **new_table;
	size_t new_alloc_len;
	int i;
	
	new_alloc_len = self->alloc_len / 2;
	
	//Move the items
	for (i = new_alloc_len; i < self->alloc_len; i++)
	{
		if (! _mtc_ptr_is_free
			(self->table[i], self->table, self->alloc_len))
		{
			if (_mtc_ptr_is_free
				(self->table[i - new_alloc_len], self->table, 
				self->alloc_len))
			{
				self->table[i - new_alloc_len] = self->table[i];
			}
			else
			{
				MtcAflItem *iter;
				
				for (iter = self->table[i - new_alloc_len]; 
					iter->next;
					iter = iter->next)
					;
				
				iter->next = self->table[i];
			}
		}
	}
	
	//Collapse the table
	new_table = (MtcAflItem **) mtc_realloc
		(self->table, sizeof(void *) * (new_alloc_len + 1));
	
	//Regenerate the free list
	self->free_list = new_table + new_alloc_len;
	new_table[new_alloc_len] = NULL;
	for (i = new_alloc_len - 1; i >= 0; i--)
	{
		if (_mtc_ptr_is_free
			(new_table[i], self->table, self->alloc_len))
		{
			new_table[i] = (MtcAflItem *) self->free_list;
			self->free_list = (new_table + i);
		}
	}
	
	//Update
	self->table = new_table;
	self->alloc_len = new_alloc_len;
}

//Initialize the data structure
void mtc_afl_init(MtcAfl *self)
{	
	int32_t i;
	
	//Allocate initial memory
	self->table = (MtcAflItem **) mtc_alloc(sizeof(void *) * (MTC_AFL_INIT_SIZE + 1));
	self->alloc_len = MTC_AFL_INIT_SIZE;
	self->n_elements = 0;
	
	//Generate free list
	self->free_list = self->table;
	self->table[self->alloc_len] = NULL;
	for (i = self->alloc_len - 1; i >= 0; i--)
	{
		self->table[i] = (MtcAflItem *) (self->table + i + 1);
	}
}

//Searches for a key in the data structure. If found, the corresponding
//item is returned or NULL is returned.
MtcAflItem *mtc_afl_lookup(MtcAfl *self, unsigned int key)
{
	//Get slot
	MtcAflItem *iter = self->table[key % self->alloc_len];
	
	if (_mtc_ptr_is_free(iter, self->table, self->alloc_len))
		return NULL;
	
	//Search
	for(; iter; iter = iter->next)
	{
		if (iter->key == key)
			return iter;
	}
	
	return NULL;
}

//Inserts an item inside the data structure.
unsigned int mtc_afl_insert(MtcAfl *self, MtcAflItem *item)
{
	unsigned int key;
	do
	{
		//check whether slot can be allocated from free_list
		if (*(self->free_list))
		{
			//Pop and use
			key = self->free_list - self->table;
			self->free_list = (MtcAflItem **) *(self->free_list);
			break;
		}
		else
		{
			//Increase table size
			expand_table(self);
		}
	}
	while (1);
	
	//Set the key and add the item
	item->key = key;
	item->next = NULL;
	self->table[key] = item;
	
	self->n_elements++;
	
	return key;
}

//Removes an item associated with the given key.
MtcAflItem *mtc_afl_remove(MtcAfl *self, unsigned int key)
{
	//Get slot
	MtcAflItem **slot = self->table + (key % self->alloc_len);
	MtcAflItem *iter = *slot;
	MtcAflItem *prev = NULL;
	
	if (_mtc_ptr_is_free(iter, self->table, self->alloc_len))
		return NULL;
	
	//Search
	for(; iter; iter = iter->next)
	{
		if (iter->key == key)
			break;
		
		prev = iter;
	}
	
	if (! iter)
		return NULL;
	
	if (prev)
	{
		//Removal for non-first item
		prev->next = iter->next;
	}
	else
	{
		//Removal for first item
		if (! iter->next)
		{
			*slot = (MtcAflItem *) self->free_list;
			self->free_list = slot;
		}
		else
		{
			*slot = iter->next;
		}
	}
	iter->next = NULL;
	self->n_elements--;		
	
	//collapse table?
	if (self->n_elements)
		if ((self->alloc_len / self->n_elements) > MTC_AFL_INIT_SIZE)
			collapse_table(self);
	
	return iter;
}

//Replaces an item associated with the given key with given replacement.
MtcAflItem *mtc_afl_replace
	(MtcAfl *self, unsigned int key, MtcAflItem *replacement)
{
	//Get slot
	MtcAflItem **slot = self->table + (key % self->alloc_len);
	MtcAflItem *iter = *slot;
	MtcAflItem *prev = NULL;
	
	if (_mtc_ptr_is_free(iter, self->table, self->alloc_len))
		return NULL;
	
	//Search
	for(; iter; iter = iter->next)
	{
		if (iter->key == key)
			break;
		
		prev = iter;
	}
	
	if (! iter)
		return NULL;
	
	//Replacement
	if (prev)
	{
		prev->next = replacement;
	}
	else
	{
		*slot = replacement;
	}
	replacement->next = iter->next;
	replacement->key = key;
	
	return iter;
}

//Returns the number of items in the data structure
unsigned int mtc_afl_get_n_items(MtcAfl *self)
{
	return self->n_elements;
}

//Gets a list of all inserted items
void mtc_afl_get_items(MtcAfl *self, MtcAflItem **items)
{
	MtcAflItem *iter;
	int i, j = 0;
	
	for (i = 0; i < self->alloc_len; i++)
	{
		iter = self->table[i];
		if (_mtc_ptr_is_free(iter, self->table, self->alloc_len))
			continue;
		
		for (; iter; iter = iter->next)
		{
			items[j] = iter;
			j++;
		}
	}
}

//Destroys the data structure.
void mtc_afl_destroy(MtcAfl *self)
{	
	//Free the array
	mtc_free(self->table);
}

/* Dumps the internals of the data structure for debugging the library.
 * This function is not included in API.
 * \param self The data structure
 */
void mtc_afl_dump(MtcAfl *self)
{
	MtcAflItem *iter;
	MtcAflItem **fl_iter;
	int i;
	//Print variables
	printf("self = %p\n", self);
	printf("   alloc_len = %d, n_elememts = %d\n", (int) self->alloc_len,
	       (int) self->n_elements);
	
	//Print the slot table
	printf("   Slot table:\n");
	for(i = 0; i < self->alloc_len; i++)
	{
		printf("      %d", i);
		iter = self->table[i];
		if (_mtc_ptr_is_free(iter, self->table, self->alloc_len))
			iter = NULL;
		//Print inserted keys
		for (; iter; iter = iter->next)
			printf(" --> %d", iter->key);
		printf("\n");
	}
	//Print the free list
	printf("   free list (last at top): ");
	for (fl_iter = self->free_list; *fl_iter; fl_iter = ((MtcAflItem **) *fl_iter))
		printf("%d, ", (int) (fl_iter - self->table));
	printf("end\n");
}


