/**
 * @file win_compat_sym.h
 * @brief Windows compatibility definitions for legacy MSVC compilers.
 *
 * Ensures proper target architecture macros (_AMD64_, _X86_, etc.) are
 * defined before including <windef.h> and other Windows API headers.
 * Also provides fallback definitions for missing standard types/functions.
 *
 * @author Samuel Marks
 */

#ifndef C_CDD_WIN_COMPAT_SYM_H
#define C_CDD_WIN_COMPAT_SYM_H

#if defined(_WIN32) && defined(_MSC_VER) && !defined(__INTEL_COMPILER)
#if defined(_M_AMD64) && !defined(_AMD64_)
#define _AMD64_
#elif defined(_M_IX86) && !defined(_X86_)
#define _X86_
#elif defined(_M_ARM64) && !defined(_ARM64_)
#define _ARM64_
#elif defined(_M_ARM) && !defined(_ARM_)
#define _ARM_
#endif
/* clang-format off */
#include <string.h>
#if _MSC_VER < 1900
#define snprintf _snprintf
#endif
#endif

#if defined(_MSC_VER)
#define SIZE_T_FMT "Iu"

#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
#include <inttypes.h>
#define SIZE_T_FMT "zu"

#elif defined(__LP64__) || defined(_LP64)
#define SIZE_T_FMT "lu"

#else
#define SIZE_T_FMT "lu"

#endif

#endif /* C_CDD_WIN_COMPAT_SYM_H */

/* clang-format on */
