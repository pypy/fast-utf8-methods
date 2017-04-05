#pragma once

#include "utf8.h"

/**
 * The private functions & data structures.
 * Can be used internally...
 */

typedef struct fu8_idxtab {
    int character_step;
    size_t * byte_positions;
    size_t bytepos_table_length;
} fu8_idxtab_t;

typedef struct fu8_idx_lookup {
    size_t codepoint_index;
    size_t codepoint_offset;
    size_t codepoint_length;
    const uint8_t * utf8;
    size_t byte_offset;
    size_t byte_length;
    struct fu8_idxtab ** table;
} fu8_idx_lookup_t;

struct fu8_idxtab * _fu8_alloc_idxtab(int, int);
void _fu8_itab_set_bucket(struct fu8_idxtab *, int, size_t, size_t);

/**
 * Index an utf8 strings.
 */
ssize_t _fu8_index_seq(fu8_idx_lookup_t *);
ssize_t _fu8_index_sse4(fu8_idx_lookup_t *);
ssize_t _fu8_index_avx2(fu8_idx_lookup_t *);
