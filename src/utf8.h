#pragma once

#include <stddef.h>

typedef struct decoding_error {
    size_t start;
    size_t end;
    int reason_index;
} decoding_error_t;

/**
 * Returns -1 on error, TODO
 */
ssize_t count_utf8_codepoints_slow(const uint8_t * encoded, size_t len, decoding_error_t * error);

ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len, decoding_error_t * error);
