/* utils.h
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
 
//Entire file is private to the library.

/**
 * \addtogroup mtc_internal
 * \{
 * 
 * This section documents a collection of important internal functions.
 */

//Adds integer to pointer
#define MTC_PTR_ADD(ptr, bytes) ((void *) (((int8_t *) (ptr)) + (bytes)))

//Pointer casting macros
#define MTC_UINT_TO_PTR(v) MTC_INT_TO_PTR(v))
#define MTC_UINT16_TO_PTR(v) MTC_INT16_TO_PTR(v)
#define MTC_UINT32_TO_PTR(v) MTC_INT32_TO_PTR(v)

#define MTC_PTR_TO_UINT(v) ((unsigned int) MTC_PTR_TO_INT(v))
#define MTC_PTR_TO_UINT16(v) ((uint16_t) MTC_PTR_TO_INT16(v))
#define MTC_PTR_TO_UINT32(v) ((uint32_t) MTC_PTR_TO_INT32(v))

#define MTC_VAR_ARRAY_SIZE 2

//Errors
#ifdef __GNUC__

#define mtc_error(...) \
	do { \
		fprintf(stderr, "MTC: %s: %s:%d: ERROR:", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		mtc_warn_break(1); \
	} while (0)

#define mtc_warn(...) \
	do { \
		fprintf(stderr, "MTC: %s: %s:%d: WARNING:", __PRETTY_FUNCTION__, __FILE__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		mtc_warn_break(0); \
	} while (0)

#else

#define mtc_error(...) \
	do { \
		fprintf(stderr, "MTC: %s:%d: ERROR:", __FILE__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		mtc_warn_break(1); \
	} while (0)

#define mtc_warn(...) \
	do { \
		fprintf(stderr, "MTC: %s:%d: WARNING:", __FILE__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		fprintf(stderr, "\n"); \
		mtc_warn_break(0); \
	} while (0)

#endif	

/**If you want to break at an error or warning from MTC library
 * use debugger to break at this function.
 * 
 * \param to_abort In case of error its value is 1, else it is 0.
 */
void mtc_warn_break(int to_abort);

//Allocation
#define mtc_free free
#define mtc_tryalloc malloc
#define mtc_tryrealloc realloc

/* Allocates memory of size bytes. If memory allocation fails the program is
 * aborted.
 * \param size Number of bytes to allocate
 * \return Newly allocated memory, free with free()
 */
void *mtc_alloc(size_t size);

/* Reallocates memory old_mem to size bytes. If memory allocation fails program is
 * aborted.
 * \param old_mem Original memory to resize
 * \param size Number of bytes to allocate
 * \return Newly allocated memory, free with free()
 */
void *mtc_realloc(void *old_mem, size_t size);

/* Allocates two memory blocks of size1 and size2 bytes. 
 * If memory allocation fails the program is aborted.
 * \param size1 Number of bytes to allocate for first memory block
 * \param size2 Number of bytes to allocate for second memory block
 * \param mem2_return Return location for second memory block
 * \return First memory block, free with free() to free both memory blocks.
 */
void *mtc_alloc2(size_t size1, size_t size2, void **mem2_return);

/* Allocates two memory blocks of size1 and size2 bytes. 
 * If memory allocation fails NULL is returned and mem2_return 
 * argument is unmodified.
 * \param size1 Number of bytes to allocate for first memory block
 * \param size2 Number of bytes to allocate for second memory block
 * \param mem2_return Return location for second memory block
 * \return First memory block, free with free() to free both memory blocks.
 */
void *mtc_tryalloc2(size_t size1, size_t size2, void **mem2_return);

#define mtc_alloc_boundary (2 * sizeof(void *))

#define mtc_align_offset(offset) \
	do { \
		(offset) += (mtc_alloc_boundary - 1); \
		(offset) -= (offset) % mtc_alloc_boundary; \
	} while (0)

#define mtc_align_ptr(ptr, offset) \
	do { \
		int aligned = (offset); \
		aligned += (mtc_alloc_boundary - 1); \
		aligned -= aligned % mtc_alloc_boundary; \
		ptr = MTC_PTR_ADD(ptr, aligned); \
	} while (0)

//Like strdup() but aborts on allocation failures
char *mtc_strdup(const char *str);
//This is self explanatory
void *mtc_memdup(const void *mem, size_t len);
//MtcVector
typedef struct
{
	char *data;
	size_t len, alloc_len;
} MtcVector;

#define MTC_VECTOR_MIN_ALLOC_LEN (sizeof(int) * 4)
#define mtc_vector_prepend(vector, size) mtc_vector_move_mem((vector), 0, (size), (vector)->len)
#define mtc_vector_first(vector, type) ((type *) (vector)->data)
#define mtc_vector_last(vector, type) ((type *) MTC_PTR_ADD((vector)->data, (vector)->len - sizeof(type)))
#define mtc_vector_lim_ptr(vector) MTC_PTR_ADD((vector)->data, (vector)->len)
#define mtc_vector_n_elements(vector, type) ((vector)->len / sizeof(type))
#define mtc_vector_alloc_len(vector, type) ((vector)->alloc_len)

void mtc_vector_init(MtcVector *vector);
void mtc_vector_destroy(MtcVector *vector);
void mtc_vector_resize(MtcVector *vector, size_t new_len);
void mtc_vector_grow(MtcVector *vector, size_t by);
void mtc_vector_shrink(MtcVector *vector, size_t by);
void mtc_vector_move_mem(MtcVector *vector, size_t from, size_t to, size_t size);

/**
 * \}
 */

