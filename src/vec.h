#include <stdlib.h>
#include "arena.h"

typedef struct {
	size_t size;
	size_t capacity;
} VectorHeader;

#define vec_hdr(v) ((VectorHeader*)((char*)(v) - sizeof(VectorHeader)))

#define vec_size(v) ((v) ? vec_hdr(v)->size : 0) 
#define vec_cap(v) ((v) ? vec_hdr(v)->capacity : 0)

#define vec_create(type, cap) (type*)_vec_create(cap, sizeof(type))
#define vec_push(v, value) do {\
	if (vec_size(v) >= vec_cap(v)) {\
		_vec_grow((void**)&(v), sizeof((v)[0])); /* NOLINT(bugprone-sizeof-expression) */\
	}\
	(v)[vec_hdr(v)->size++] = (value);\
} while (0)\

#define vec_free(v) do {\
	if (v) { free(vec_hdr(v)); (v) = NULL; }\
} while (0)\

static inline void *_vec_create(size_t init_cap, size_t elem_size) {
	size_t hdr_size = sizeof(VectorHeader);
	size_t data_size = init_cap * elem_size;

	char *ptr = (char*)malloc(hdr_size + data_size);
	if (!ptr) return NULL;

	VectorHeader *h = (VectorHeader*)ptr;
	h->size = 0;
	h->capacity = init_cap;

	return (void*)(ptr + hdr_size);
}

static inline void _vec_grow(void** vec_ptr, size_t elem_size) {
	void *vec = *vec_ptr;
	size_t new_cap = 0;

	if (vec) {
		new_cap = vec_hdr(vec)->capacity * 2;
	} else {
		new_cap = 8;
	}

	size_t new_total_size = sizeof(VectorHeader) + (new_cap * elem_size);

	void *old_ptr = vec ? vec_hdr(vec) : NULL;
	char *new_ptr = (char *)realloc(old_ptr, new_total_size);

	VectorHeader *hdr = (VectorHeader*)new_ptr;
	hdr->capacity = new_cap;
	if (!vec) hdr->size = 0;

	*vec_ptr = (void *)(new_ptr + sizeof(VectorHeader));
}
