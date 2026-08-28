#ifndef NO_DISCARD_H
#define NO_DISCARD_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#if defined(__GNUC__) || defined(__clang__)
#define NO_DISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
#include <sal.h>
#define NO_DISCARD _Check_return_
#else
#define NO_DISCARD
#endif
/* clang-format on */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif
