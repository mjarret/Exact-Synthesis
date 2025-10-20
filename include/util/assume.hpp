#pragma once

// Build policy:
// - Release (NDEBUG defined): compile out assumptions entirely (no runtime code emitted).
// - Debug   (no NDEBUG): assert preconditions to catch violations during development.

#if defined(NDEBUG)
  #define ASSUME(cond) ((void)0)
#else
  #include <cassert>
  #define ASSUME(cond) assert(cond)
#endif
