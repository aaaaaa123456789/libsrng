#include "libsrng.h"

struct libsrng_stable_random_state {
  unsigned shift:  32;
  unsigned carry:   8;
  unsigned current: 8;
  unsigned prev:    8;
  unsigned linear:  8;
};

static inline unsigned short libsrng_random_linear(unsigned short);
static inline unsigned char libsrng_random_combined(unsigned long long *);
static inline unsigned long long libsrng_random_combined_multibyte(unsigned long long *, unsigned char);
static inline unsigned short libsrng_random_halfword(unsigned long long *);
static inline unsigned long long libsrng_random_seed(unsigned long long *);
static inline unsigned char libsrng_stable_random(struct libsrng_stable_random_state *);
static inline unsigned short libsrng_random_range(unsigned long long *, unsigned short);

#define HALFWORD_LCG_MULTIPLIER             0x6329
#define HALFWORD_LCG_ADDEND                 0x4321
#define SEED_LCG_MULTIPLIER     0x5851f42d4c957f2dULL
#define SEED_LCG_FIRST_ADDEND   0x0123456789abcdefULL
#define SEED_LCG_SECOND_ADDEND  0x0fedcba987654321ULL

#define STABLE_RANDOM_NEXT_LINEAR(s) ((s) -> linear *= 73, (s) -> linear += 29, (s) -> linear)

unsigned short libsrng_random (unsigned long long * state, unsigned short range, unsigned reseed) {
  if (!state) return 0;
  while (reseed --) *state = libsrng_random_seed(state);
  return libsrng_random_range(state, range);
}

static inline unsigned short libsrng_random_linear (unsigned short previous) {
  return (previous * HALFWORD_LCG_MULTIPLIER + HALFWORD_LCG_ADDEND) & 0xffff;
}

static inline unsigned char libsrng_random_combined (unsigned long long * state) {
  // this conditional should be determined at compile time, and it should be true for virtually any reasonable platform
  if (sizeof *state == sizeof(struct libsrng_stable_random_state)) {
    union {struct libsrng_stable_random_state s; unsigned long long n;} * state_union = (void *) state;
    return libsrng_stable_random(&(state_union -> s));
  } else {
    struct libsrng_stable_random_state temp_state = {.shift = *state, .carry = *state >> 32, .current = *state >> 40,
                                                     .prev = *state >> 48, .linear = *state >> 56};
    unsigned char result = libsrng_stable_random(&temp_state);
    *state = ((unsigned long long) temp_state.shift) | ((unsigned long long) temp_state.carry << 32) | ((unsigned long long) temp_state.current << 40) |
             ((unsigned long long) temp_state.prev << 48) | ((unsigned long long) temp_state.linear << 56);
    return result;
  }
}

static inline unsigned long long libsrng_random_combined_multibyte (unsigned long long * state, unsigned char width) {
  if (width > sizeof(unsigned long long)) return -1;
  unsigned long long result = 0;
  while (width --) result = (result << 8) | libsrng_random_combined(state);
  return result;
}

static inline unsigned short libsrng_random_halfword (unsigned long long * state) {
  unsigned short buffer = libsrng_random_combined_multibyte(state, 2);
  unsigned char count = libsrng_random_combined(state);
  unsigned char shift = count >> 4, multiplier = 3 + ((count & 12) >> 1);
  count = (count & 3) + 2;
  while (count --) buffer = libsrng_random_linear(buffer);
  if (shift) buffer = (buffer << shift) | (buffer >> (16 - shift));
  return (buffer * multiplier) & 0xffff;
}

static inline unsigned long long libsrng_random_seed (unsigned long long * state) {
  unsigned long long first = libsrng_random_combined_multibyte(state, 8);
  first = first * SEED_LCG_MULTIPLIER + SEED_LCG_FIRST_ADDEND;
  unsigned long long second = 0;
  unsigned count;
  for (count = 0; count < 4; count ++) second = (second << 16) + libsrng_random_halfword(state);
  second = second * SEED_LCG_MULTIPLIER + SEED_LCG_SECOND_ADDEND;
  return first ^ second;
}

static inline unsigned char libsrng_stable_random (struct libsrng_stable_random_state * state) {
  unsigned p;
  if (!state -> shift) for (p = 0; p < 4; p ++) state -> shift = (state -> shift << 8) | STABLE_RANDOM_NEXT_LINEAR(state);
  state -> shift ^= state -> shift >> 8;
  state -> shift ^= state -> shift << 9;
  state -> shift ^= state -> shift >> 23;
  if (state -> carry >= 210) state -> carry -= 210;
  p = state -> carry + state -> prev + state -> current;
  if (!p || (p == 719)) {
    state -> prev = STABLE_RANDOM_NEXT_LINEAR(state);
    state -> carry = STABLE_RANDOM_NEXT_LINEAR(state);
    state -> current = STABLE_RANDOM_NEXT_LINEAR(state);
  }
  p = 210 * state -> prev + state -> carry;
  state -> prev = state -> current;
  state -> current = p & 0xff;
  state -> carry = p >> 8;
  STABLE_RANDOM_NEXT_LINEAR(state);
  p = state -> shift >> ((state -> linear >> 3) & 24);
  switch ((state -> linear >> 4) & 3) {
    case 0:
      return p + state -> current;
    case 1:
      return p ^ state -> current;
    case 2:
      return p - state -> current;
    default:
      return state -> current - p;
  }
}

static inline unsigned short libsrng_random_range (unsigned long long * state, unsigned short limit) {
  if (limit == 1) return 0;
  unsigned short result = libsrng_random_halfword(state);
  if (!(limit & (limit - 1))) return result & (limit - 1);
  if (result >= limit) return result % limit;
  unsigned short resampling_limit = 0x10000 % limit;
  while (result < resampling_limit) result = libsrng_random_halfword(state);
  return result % limit;
}
