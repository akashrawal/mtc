/* utils.c
 * Internal utilities
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

//If you want to break at an error or warning break at this function.
void mtc_warn_break(int to_abort)
{
	if (to_abort)
		abort();
}

//Memory allocation functions
void *mtc_alloc(size_t size)
{
	void *mem = malloc(size);
	if (! mem)
	{
		mtc_error("Memory allocation failed.");
	}
	return mem;
}

void *mtc_realloc(void *old_mem, size_t size)
{
	void *mem = realloc(old_mem, size);
	if (! mem)
	{
		mtc_error("Memory allocation failed.");
	}
	return mem;
}

void *mtc_alloc2(size_t size1, size_t size2, void **mem2_return)
{
	void *mem1;
	
	//mem2 must be aligned to size of two pointers.
	//Adjust size1 for that.
	size1 = mtc_offset_align(size1);
	
	//Now allocate
	mem1 = malloc(size1 + size2);
	if (! mem1)
	{
		mtc_error("Memory allocation failed.");
	}
	
	*mem2_return = MTC_PTR_ADD(mem1, size1);
	return mem1;
}

void *mtc_tryalloc2(size_t size1, size_t size2, void **mem2_return)
{
	void *mem1;
	
	//mem2 must be aligned to size of two pointers.
	//Adjust size1 for that.
	size1 = mtc_offset_align(size1);
	
	//Now allocate
	mem1 = malloc(size1 + size2);
	if (! mem1)
	{
		return NULL;
	}
	
	*mem2_return = MTC_PTR_ADD(mem1, size1);
	return mem1;
}

char *mtc_strdup(const char *str)
{
	char *res = strdup(str);
	if (!res)
	{
		mtc_error("Memory allocation failed.");
	}
	return res;
}

void *mtc_memdup(const void *mem, size_t len)
{
	void *res = mtc_alloc(len);
	memcpy(res, mem, len);
	return res;
}

//MtcVector

void mtc_vector_init(MtcVector *vector)
{
	vector->len = 0;
	vector->alloc_len = MTC_VECTOR_MIN_ALLOC_LEN;
	vector->data = mtc_alloc(vector->alloc_len);
}

void mtc_vector_destroy(MtcVector *vector)
{
	mtc_free(vector->data);
}

void mtc_vector_resize(MtcVector *vector, size_t new_len)
{
	vector->len = new_len;
	
	if (new_len > vector->alloc_len)
	{
		do
		{
			vector->alloc_len *= 2;
		} while (new_len > vector->alloc_len);
		vector->data = mtc_realloc(vector->data, vector->alloc_len);
	}
	else if (new_len < (vector->alloc_len / 2))
	{
		do
		{
			if (vector->alloc_len == MTC_VECTOR_MIN_ALLOC_LEN)
				break;
			vector->alloc_len /= 2;
		} while (new_len < (vector->alloc_len / 2));
		vector->data = mtc_realloc(vector->data, vector->alloc_len);
	}
}

void mtc_vector_grow(MtcVector *vector, size_t by)
{
	size_t new_len = (vector->len += by);
	
	if (new_len > vector->alloc_len)
	{
		do
		{
			vector->alloc_len *= 2;
		} while (new_len > vector->alloc_len);
		vector->data = mtc_realloc(vector->data, vector->alloc_len);
	}
}

void mtc_vector_shrink(MtcVector *vector, size_t by)
{
	size_t new_len = (vector->len -= by);
	
	if (new_len < (vector->alloc_len / 2))
	{
		do
		{
			if (vector->alloc_len == MTC_VECTOR_MIN_ALLOC_LEN)
				break;
			vector->alloc_len /= 2;
		} while (new_len < (vector->alloc_len / 2));
		vector->data = mtc_realloc(vector->data, vector->alloc_len);
	}
}

void mtc_vector_move_mem(MtcVector *vector, size_t from, size_t to, size_t size)
{
	size_t newsize = to + size;
	assert (from + size <= vector->len);
	
	if (vector->len < newsize)
		mtc_vector_resize(vector, newsize);
	
	if (to < from)
	{
		char *fptr = (char *) MTC_PTR_ADD(vector->data, from);
		char *tptr = (char *) MTC_PTR_ADD(vector->data, to);
		char *flim = (char *) MTC_PTR_ADD(fptr, size);
		for (; fptr < flim; fptr++, tptr++)
		{
			*tptr = *fptr;
		}
	}
	else if (to > from)
	{
		char *flim = (char *) MTC_PTR_ADD(vector->data, from - 1);
		char *fptr = (char *) MTC_PTR_ADD(flim, size);
		char *tptr = (char *) MTC_PTR_ADD(vector->data, to + size - 1);
		
		for (; fptr > flim; fptr--, tptr--)
		{
			*tptr = *fptr;
		}
	}
}
