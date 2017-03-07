#pragma once

unsigned long long _bench_seq(const uint8_t * bytes, int len);
unsigned long long _bench_vec(const uint8_t * bytes, int len);
unsigned long long _bench_libunistring(const uint8_t * bytes, int len);
unsigned long long _bench_u8u16(const uint8_t * bytes, int len);
unsigned long long _bench_mystringlenutf8(const uint8_t * bytes, int len);
