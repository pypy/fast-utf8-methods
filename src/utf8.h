#pragma once

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Returns -1 if the given string is not a valid utf8 encoded string.
 * Otherwise returns the amount code point in the given string.
 * len: length in bytes (8-bit)
 *
 * The above documentation also applies for several vectorized implementations
 * found below.
 *
 * count_utf8_codepoints dispatches amongst several
 * implementations (e.g. seq, SSE4, AVX)
 */
ssize_t count_utf8_codepoints_seq(const uint8_t * encoded, size_t len);
ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len);
ssize_t count_utf8_codepoints_sse4(const uint8_t * encoded, size_t len);
ssize_t count_utf8_codepoints_avx(const uint8_t * encoded, size_t len);


/**
 * Looks up the byte position of the utf8 code point at the index.
 * Assumptions:
 *
 *  * utf8 parameter is utf8 encoded, otherwise the result is undefined.
 *
 * If table is not NULL, this routine builds up a lookup table to speed up indexing.
 *
 * Table layout:
 *
 * TODO
 */
ssize_t byte_position_at_index_utf8(size_t index, const uint8_t * utf8, size_t len, uint8_t ** table, int * table_size);
