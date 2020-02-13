# libsrng

It's a bunch of RNG functions I wrote some time ago. The library is a single C file and can be dropped wherever.
Include `libsrng.h` in your project to use it, and compile `libsrng.c` with it. No dependencies. (If your environment
doesn't have the `<stdint.h>` header, replace it with suitable definitions for `uint16_t`, `uint32_t` and `uint64_t`.)

Usage:

```c
uint16_t libsrng_random(uint64_t * state, uint16_t range, unsigned reseed);
```

* `state` is the 64-bit state of the RNG.
* `range` is the range in which it will generate values — for instance, a value of 10 will cause it to generate values
  from 0 to 9. A value of 0 will make it generate a full 16-bit random number (instead of clamping it to some range),
  and a value of 1 will make the function return 0 without generating a new random number at all (can be used to
  reseed the RNG without consuming a random number).
* `reseed` is used to reseed the RNG before generating a new random number; this enables users to have multiple RNG
  streams from the same base seed. The RNG is reseeded by generating a random seed (using a different (and slower)
  RNG) from the given state, as many times as this argument indicates; the final seed becomes the new state.

This library is released to the public domain under [the Unlicense](LICENSE).
