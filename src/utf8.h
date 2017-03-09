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

