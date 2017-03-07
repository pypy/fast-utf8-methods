#include <time.h>

#include <stdint.h>

// copied from u6u18 to count cycles on x86
__inline__ unsigned long long int read_cycle_counter () {
  unsigned long long int ts;
  asm volatile("rdtsc\n" : 
               "=A" (ts));
  return(ts);
}

unsigned long long _bench_seq(const uint8_t * bytes, int len)
{
    unsigned long long cycs;
    cycs = read_cycle_counter();
    count_utf8_codepoints(bytes, len, NULL);
    return read_cycle_counter() - cycs;
}


