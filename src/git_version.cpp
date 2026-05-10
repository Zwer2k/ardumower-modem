#include "git_version.h"

// Try to use generated header first (from pio_git_version.py)
#if __has_include("git_version_generated.h")
  #include "git_version_generated.h"
#endif

// Fallback to compiler defines if available
#ifndef GIT_HASH
  #ifdef GIT_HASH_DEFINE
    #define GIT_HASH GIT_HASH_DEFINE
  #else
    #define GIT_HASH "@GIT_HASH@"
  #endif
#endif

#ifndef GIT_TIME
  #ifdef GIT_TIME_DEFINE
    #define GIT_TIME GIT_TIME_DEFINE
  #else
    #define GIT_TIME "@GIT_TIME@"
  #endif
#endif

#ifndef GIT_TAG
  #ifdef GIT_TAG_DEFINE
    #define GIT_TAG GIT_TAG_DEFINE
  #else
    //#define GIT_TAG "@GIT_TAG@"
  #endif
#endif

const char *git_hash = GIT_HASH;
const char *git_time = GIT_TIME;
#ifdef GIT_TAG
const char *git_tag = GIT_TAG;
#else
const char *git_tag = "@GIT_TAG@";
#endif
