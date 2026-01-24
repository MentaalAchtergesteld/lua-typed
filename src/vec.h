#pragma once
#include <stdlib.h>
#include <memory.h>

#include "arena.h"

#define HDR_SIZE (4 * sizeof(size_t));
#define vec(T) T *

#define vec_length(VEC) (*(size_t **)VEC)[-1]
#define vec_size(VEC) (*(size_t **)VEC)[-2]
#define vec_allocator(VEC) (*(MemArena ***)VEC)[-3]


#define vec_new(T, ALLOC) (T *)_vec_new(ALLOC)

static inline void *_vec_new(MemArena *alloc) {
	size_t *ptr = (size_t *)arena_push(alloc, 4 * sizeof(size_t), false);
	void *vec = (void *)(ptr + 4);
	vec_allocator(&vec) = alloc;
	return vec;
}

#define vec_append(VEC, EL) \
do { \
	if (vec_length(VEC) == vec_size(VEC)) { \
		size_t _old_cnt = vec_size(VEC); \
		size_t _new_cnt = _old_cnt ? _old_cnt * 2 : 32; \
		size_t _elem_sz = sizeof(**(VEC)); \
		\
		size_t _old_bytes = HDR_SIZE + (_old_cnt * _elem_sz); \
		size_t _new_bytes = HDR_SIZE + (_new_cnt * _elem_sz); \
		\
		void *_base = (size_t *)(*(VEC)) - 4; \
		_base = arena_resize(vec_allocator(VEC), _base, _old_bytes, _new_bytes); \
		*(VEC) = (void *)((size_t *)_base + 4); \
		vec_size(VEC) = _new_cnt; \
	} \
	(*(VEC))[vec_length(VEC)++] = (EL); \
} while (0)

#define vec_copy(VEC, DEST) _vec_copy((void*)VEC, sizeof(*(VEC)), DEST)

static inline void *_vec_copy(void *v, size_t item_size, MemArena *dest) {
	if (!v) return NULL;

	size_t len = vec_length(v);
	size_t hdr_size = HDR_SIZE;
	size_t data_size = len * item_size;

	size_t *ptr = (size_t *)arena_push(dest, hdr_size + data_size, false);

	ptr[0] = (size_t)dest;
	ptr[1] = len;
	ptr[2] = len;

	void *new_vec = (void *)(ptr + 4);
	memcpy(new_vec, v, data_size);
	return new_vec;
}
