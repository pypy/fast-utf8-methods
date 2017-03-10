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

typedef struct fu8_idxtab {
    // TODO think about locking this
    int character_step;
    size_t * byte_positions;
    size_t bytepos_table_length;
} fu8_idxtab_t;

#include <stdlib.h>

fu8_idxtab_t * _fu8_alloc_idxtab(int cp_count, int character_step)
{
    if (cp_count <= character_step) {
        return NULL;
    }
    long s = (cp_count/character_step) * sizeof(size_t);
    char * c = calloc(sizeof(fu8_idxtab_t)+s, 1);
    fu8_idxtab_t * i = (fu8_idxtab_t*)c;
    i->character_step = character_step;
    i->byte_positions = (size_t*)(c + sizeof(fu8_idxtab_t));
    return i;
}

void fu8_free_idxtab(struct fu8_idxtab * t)
{
    // why manage this in C?
    // it might at some point have a dynamic table inside fu8_idxtab_t,
    // then we can handle this easily here without modifying the API
    free(t); t = NULL;
}

#define FU8_ITAB_STEP(len) len / 16
void _fu8_itab_set_bucket(struct fu8_idxtab * tab, int bucket, size_t len, size_t cpidx)
{
    tab->byte_positions[bucket] = (size_t)len;
}

ssize_t _fu8_build_idxtab(size_t cpidx, size_t index_off,
                          const uint8_t * utf8, size_t bytelen,
                          size_t cplen,
                          struct fu8_idxtab ** tab) {
    size_t code_point_index = index_off;
    const uint8_t * utf8_start_position = utf8;
    const uint8_t * utf8_end_position = utf8 + bytelen;

    struct fu8_idxtab * itab = tab[0];
    if (itab == NULL) {
        tab[0] = itab = _fu8_alloc_idxtab(cplen, 1000);
    }

    int bucket_step = -1;
    int bucket = -1;
    if (itab) {
        bucket_step = itab->character_step;
        bucket = index_off / bucket_step;
        //printf("step %d/%d bucket ioff %ld\n", bucket, bucket_step, index_off);
    }

    while (utf8 < utf8_end_position) {
        //printf("%d %llx ok\n", code_point_index, utf8);
        if (code_point_index == cpidx) {
            //printf("return %llx %llx %llx\n", utf8_start_position, utf8, utf8_end_position);
            return utf8 - utf8_start_position;
        }

        if (bucket_step != -1 && code_point_index != 0 && (code_point_index % bucket_step) == 0) {
            _fu8_itab_set_bucket(itab, bucket++, utf8 - utf8_start_position, code_point_index);
        }

        uint8_t c = *utf8++;
        //printf("%x\n", c);
        code_point_index += 1;
        if ((c & 0xc0) == 0) {
            continue;
        }
        if ((c & 0xe0) == 0xc0) {
            utf8 += 1;
            continue;
        }
        if ((c & 0xf0) == 0xe0) {
            utf8 += 2;
            continue;
        }
        if ((c & 0xf8) == 0xf0) {
            utf8 += 3;
            continue;
        }
    }

    return -1; // out of bounds!!
}

size_t _fu8_idxtab_lookup_bytepos_i(struct fu8_idxtab * tab, size_t cpidx);

ssize_t _fu8_idx2bytepos(size_t index, size_t index_off,
                        const uint8_t * utf8, size_t bytelen, size_t cplen,
                        struct fu8_idxtab ** tab)
{

    assert(index != 0 && "index must not be 0");
    // note that itab STILL can be NULL

    if (tab[0] == NULL || index_off != 0) {
        return _fu8_build_idxtab(index, index_off, utf8, bytelen, cplen, tab);
    }
    size_t off = _fu8_idxtab_lookup_bytepos_i(tab[0], index);
    //printf("found %llx\n", off);
    return _fu8_build_idxtab(index, off, utf8, bytelen, cplen, tab);
}

size_t _fu8_idxtab_lookup_bytepos_i(struct fu8_idxtab * tab, size_t cpidx)
{
    if (cpidx == 0) {
        return 0;
    }
    int step = tab->character_step;
    int tidx = cpidx / step;
    size_t val = tab->byte_positions[tidx];
    while (tidx > 0) {
        if (val != 0) {
            //printf("%llx at %d %d/%d\n", val, tidx, cpidx, step);
            return val;
        }
        tidx--;
        val = tab->byte_positions[tidx];
    }
    // no clue, start at the beginning!
    return 0;

    //int lp, rp; // left position, right position
    //int mp; // middle position
    //int count;
    //lp = 0;
    //rp = 16;

    //if (cpidx == 0) {
    //    return -1;
    //}

    //size_t valid_left = -1;

    //do {
    //    count = (rp - lp);
    //    mp = lp + count / 2;

    //    size_t lval = tab->codepoint_positions[lp];
    //    size_t mval = tab->codepoint_positions[mp];
    //    size_t rval = tab->codepoint_positions[rp];
    //    printf("l %d m %d r %d\nlv %d mv %d rv %d\n", lp, mp, rp, lval, mval, rval);
    //    if (lval != 0 && lval <= cpidx) {
    //        valid_left = lp;
    //    } else if (lval == 0) {
    //        // nothing is known about the left most value
    //        break;
    //    }

    //    if (mval == cpidx) {
    //        return mp;
    //    }

    //    if (mval == 0 || mval < cpidx) {
    //        // nothing is known about the middle value,
    //        // or mval is smaller the searched code point index
    //        rp = mp;
    //        continue;
    //    } else {
    //        lp = mp;
    //        continue;
    //    }

    //} while (count > 1);

    //return valid_left;
}

ssize_t fu8_idx2bytepos(size_t index,
                        const uint8_t * utf8, size_t bytelen,
                        size_t cplen,
                        struct fu8_idxtab ** tab)
{
    if (index == 0) { return 0; }
    if (index >= cplen) { return -1; }
    return _fu8_idx2bytepos(index, 0, utf8, bytelen, cplen, tab);
}
