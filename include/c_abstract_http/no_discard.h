/**
 * @file no_discard.h
 * @brief Macro definitions for compiler-specific warn_unused_result properties.
 */

#ifndef C_ABSTRACT_HTTP_NO_DISCARD_H
#define C_ABSTRACT_HTTP_NO_DISCARD_H

/* clang-format off */
#if !defined(NO_DISCARD) && defined(_MSC_VER) && _MSC_VER >= 1700
#if !defined(__cplusplus) || __cplusplus < 201703L
#if !defined(_MSVC_LANG) || _MSVC_LANG < 201703L
#include <sal.h>
#endif
#endif
#endif
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef NO_DISCARD
#if defined(__cplusplus) && __cplusplus >= 201703L
/** @brief Compiler attribute warning when return value is discarded */
#define NO_DISCARD [[nodiscard]]
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
/** @brief Compiler attribute warning when return value is discarded */
#define NO_DISCARD [[nodiscard]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/** @brief Compiler attribute warning when return value is discarded */
#define NO_DISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Compiler attribute warning when return value is discarded */
#define NO_DISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
/** @brief Compiler attribute warning when return value is discarded */
#define NO_DISCARD _Check_return_
#else
/** @brief Compiler attribute warning when return value is discarded */
#define NO_DISCARD
#endif
#endif /* !NO_DISCARD */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_NO_DISCARD_H */
