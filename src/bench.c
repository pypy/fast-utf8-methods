#include "bench.h"

#include <time.h>
#include <assert.h>
#include <stdint.h>

//#include "../thirdparty/u8u16/config/p4_config.h"
//#include "../thirdparty/u8u16/src/u8u16.c"
#include "utf8.c"

typedef unsigned long long int cycles_t;

#ifdef CYCLES
#define CLOCK_DEFS cycles_t cycs;
#define CLOCK_START cycs = read_cycle_counter()
#define CLOCK_END read_cycle_counter() - cycs;

// copied from u6u18 to count cycles on x86
__inline__ unsigned long long int read_cycle_counter () {
  unsigned long long int ts;
  asm volatile("rdtsc\n" : 
               "=A" (ts));
  return(ts);
}
#endif

unsigned long long _bench_seq(const uint8_t * bytes, int len)
{
    CLOCK_DEFS;
    CLOCK_START;
    count_utf8_codepoints_seq(bytes, len, NULL);
    return CLOCK_END;
}

unsigned long long _bench_vec(const uint8_t * bytes, int len)
{
    CLOCK_DEFS;
    CLOCK_START;
    count_utf8_codepoints(bytes, len, NULL);
    return CLOCK_END;
}

const uint8_t *
u8_check (const uint8_t *s, size_t n)
{
  const uint8_t *s_end = s + n;

  while (s < s_end)
    {
      /* Keep in sync with unistr.h and u8-mbtouc-aux.c.  */
      uint8_t c = *s;

      if (c < 0x80)
        {
          s++;
          continue;
        }
      if (c >= 0xc2)
        {
          if (c < 0xe0)
            {
              if (s + 2 <= s_end
                  && (s[1] ^ 0x80) < 0x40)
                {
                  s += 2;
                  continue;
                }
            }
          else if (c < 0xf0)
            {
              if (s + 3 <= s_end
                  && (s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
                  && (c >= 0xe1 || s[1] >= 0xa0)
                  && (c != 0xed || s[1] < 0xa0))
                {
                  s += 3;
                  continue;
                }
            }
          else if (c < 0xf8)
            {
              if (s + 4 <= s_end
                  && (s[1] ^ 0x80) < 0x40 && (s[2] ^ 0x80) < 0x40
                  && (s[3] ^ 0x80) < 0x40
                  && (c >= 0xf1 || s[1] >= 0x90)
                  && (c < 0xf4 || (c == 0xf4 && s[1] < 0x90)))
                {
                  s += 4;
                  continue;
                }
            }
        }
      /* invalid or incomplete multibyte character */
      return s;
    }
  return NULL;
}

unsigned long long _bench_libunistring(const uint8_t * bytes, int len)
{
    CLOCK_DEFS;
    CLOCK_START;
    (void)u8_check(bytes, len);
    return CLOCK_END;
}

#define ONEMASK ((size_t)(-1) / 0xFF)

static size_t
cp_strlen_utf8(const char * _s)
{
	const char * s;
	size_t count = 0;
	size_t u;
	unsigned char b;

	/* Handle any initial misaligned bytes. */
	for (s = _s; (uintptr_t)(s) & (sizeof(size_t) - 1); s++) {
		b = *s;

		/* Exit if we hit a zero byte. */
		if (b == '\0')
			goto done;

		/* Is this byte NOT the first byte of a character? */
		count += (b >> 7) & ((~b) >> 6);
	}

	/* Handle complete blocks. */
	for (; ; s += sizeof(size_t)) {
		/* Prefetch 256 bytes ahead. */
		__builtin_prefetch(&s[256], 0, 0);

		/* Grab 4 or 8 bytes of UTF-8 data. */
		u = *(size_t *)(s);

		/* Exit the loop if there are any zero bytes. */
		if ((u - ONEMASK) & (~u) & (ONEMASK * 0x80))
			break;

		/* Count bytes which are NOT the first byte of a character. */
		u = ((u & (ONEMASK * 0x80)) >> 7) & ((~u) >> 6);
		count += (u * ONEMASK) >> ((sizeof(size_t) - 1) * 8);
	}

	/* Take care of any left-over bytes. */
	for (; ; s++) {
		b = *s;

		/* Exit if we hit a zero byte. */
		if (b == '\0')
			break;

		/* Is this byte NOT the first byte of a character? */
		count += (b >> 7) & ((~b) >> 6);
	}

done:
	return ((s - _s) - count);
}

unsigned long long _bench_mystringlenutf8(const uint8_t * bytes, int len)
{
    CLOCK_DEFS;

    CLOCK_START;
    cp_strlen_utf8((char*)bytes);
    return CLOCK_END;
}
