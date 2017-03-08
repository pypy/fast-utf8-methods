#pragma once

#include <unistd.h>
#include <stdint.h>
#include <stddef.h>

/**
 * Returns -1 on error, TODO
 */
ssize_t count_utf8_codepoints_seq(const uint8_t * encoded, size_t len);

ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len);
ssize_t count_utf8_codepoints_sse4(const uint8_t * encoded, size_t len);
ssize_t count_utf8_codepoints_avx(const uint8_t * encoded, size_t len);
