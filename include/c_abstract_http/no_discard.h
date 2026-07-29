#ifndef NO_DISCARD_H
#define NO_DISCARD_H

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define NO_DISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define NO_DISCARD _Check_return_
#else
#define NO_DISCARD
#endif

#ifdef __cplusplus
}
#endif

#endif
