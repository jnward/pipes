/*
 * MSVC-compatible rand()/srand() replacement.
 *
 * The original sspipes.scr was built with Microsoft's CRT, whose rand()
 * is the LCG:  seed = seed * 214013 + 2531011;  return (seed >> 16) & 0x7fff
 * (RAND_MAX = 0x7fff).  musl/emscripten's rand() is a different generator,
 * so every RNG-driven decision (pipe turns, joint choices, teapot odds,
 * material picks) would diverge from a real Windows machine given the same
 * seed.  All original translation units are compiled with
 * -Drand=msvc_rand -Dsrand=msvc_srand so they use this generator without
 * any source edits.
 *
 * msvc_srand is also exported to JS so a reproducible seed can be set from
 * the page (see index.html); by default the shell seeds it the same way the
 * original framework does (millisecond field of the current time — see
 * ss_RandInit in COMMON/UTIL.CXX).
 */

#include <emscripten/emscripten.h>

static unsigned int msvc_seed = 1; /* MSVC CRT default seed */

EMSCRIPTEN_KEEPALIVE
void msvc_srand(unsigned int seed)
{
    msvc_seed = seed;
}

EMSCRIPTEN_KEEPALIVE
int msvc_rand(void)
{
    msvc_seed = msvc_seed * 214013u + 2531011u;
    return (int)((msvc_seed >> 16) & 0x7fff);
}
