#include "utf8.h"

#include <stdio.h>

#include "utf8-scalar.c" // copy code for scalar operations


int instruction_set = -1;
#define ISET_SSE4 0x1
#define ISET_AVX 0x2
#define ISET_AVX2 0x4

void detect_instructionset(void)
{
    long eax;
    long ebx;
    long ecx;
    long edx;
    long op = 1;
    asm ("cpuid"
            : "=a" (eax),
              "=b" (ebx),
              "=c" (ecx),
              "=d" (edx)
            : "a" (op));

    instruction_set = 0;
    if (ecx & (1<<19)) { // sse4.1
        instruction_set |= ISET_SSE4;
    }
    if(__builtin_cpu_supports("avx")) {
        instruction_set |= ISET_AVX;
    }
    if(__builtin_cpu_supports("avx2")) {
        instruction_set |= ISET_AVX2;
    }
}

ssize_t count_utf8_codepoints(const uint8_t * encoded, size_t len)
{
    if (instruction_set == -1) {
        detect_instructionset();
    }

    if (len >= 32 && (instruction_set & ISET_AVX2) != 0) {
        // to the MOON!
        return count_utf8_codepoints_avx(encoded, len);
    }
    if (len >= 16 && (instruction_set == ISET_SSE4) != 0) {
        // speed!!
        return count_utf8_codepoints_sse4(encoded, len);
    }

    // oh no, just do it sequentially!
    return count_utf8_codepoints_seq(encoded, len);
}

ssize_t byte_position_at_index_utf8(size_t index, const uint8_t * utf8, size_t len, uint8_t ** table, int * table_size)
{
    size_t code_point_index = 0;
    uint8_t * utf8_start_position = utf8;

    while (1) {
        if (code_point_index == index) {
            return utf8 - utf8_start_position;
        }

        uint8_t c = *utf8++;
        if ((c & 0xc0) == 0) {
            code_point_index += 1;
            continue;
        }
        if ((c & 0xe0) == 0xc0) {
            code_point_index += 1;
            utf8 += 2;
            continue;
        }

        utf8++;
    }

    return -1;
}
