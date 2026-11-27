/**
 * @file no_discard.h
 * @brief Macro definitions for compiler-specific warn_unused_result properties.
 */

#ifndef C_ABSTRACT_HTTP_NO_DISCARD_H
#define C_ABSTRACT_HTTP_NO_DISCARD_H

/* clang-format off */
#if defined(_MSC_VER) && _MSC_VER >= 1700
#include <sal.h>
#endif
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#ifndef C_ABSTRACT_HTTP_NO_DISCARD
#if defined(__cplusplus) && __cplusplus >= 201703L
/** @brief Compiler attribute warning when return value is discarded */
#define C_ABSTRACT_HTTP_NO_DISCARD [[nodiscard]]
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
/** @brief Compiler attribute warning when return value is discarded */
#define C_ABSTRACT_HTTP_NO_DISCARD [[nodiscard]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/** @brief Compiler attribute warning when return value is discarded */
#define C_ABSTRACT_HTTP_NO_DISCARD [[nodiscard]]
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Compiler attribute warning when return value is discarded */
#define C_ABSTRACT_HTTP_NO_DISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
/** @brief Compiler attribute warning when return value is discarded */
#define C_ABSTRACT_HTTP_NO_DISCARD _Check_return_
#else
/** @brief Compiler attribute warning when return value is discarded */
#define C_ABSTRACT_HTTP_NO_DISCARD
#endif
#endif /* !C_ABSTRACT_HTTP_NO_DISCARD */

#ifndef C_ABSTRACT_HTTP_NO_DISCARD_TYPE
#if defined(__DOXYGEN__)
/** @brief Type-level nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPE
#elif defined(__cplusplus) && __cplusplus >= 201703L
/** @brief Type-level nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPE [[nodiscard]]
#elif defined(_MSVC_LANG) && _MSVC_LANG >= 201703L
/** @brief Type-level nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPE [[nodiscard]]
#elif defined(__STDC_VERSION__) && __STDC_VERSION__ >= 202311L
/** @brief Type-level nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPE [[nodiscard]]
#elif defined(__clang__)
/** @brief Type-level nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPE __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
/** @brief Type-level nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPE _Check_return_
#else
/** @brief Type-level nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPE
#endif
#endif /* !C_ABSTRACT_HTTP_NO_DISCARD_TYPE */

#ifndef C_ABSTRACT_HTTP_NO_DISCARD_TYPEDEF
#if !defined(__DOXYGEN__) && defined(__clang__)
/** @brief Typedef-level warn_unused_result attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPEDEF __attribute__((warn_unused_result))
#else
/** @brief Typedef-level warn_unused_result attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_TYPEDEF
#endif
#endif /* !C_ABSTRACT_HTTP_NO_DISCARD_TYPEDEF */

#ifndef C_ABSTRACT_HTTP_NO_DISCARD_FN_PTR
#if defined(__DOXYGEN__)
/** @brief Function-pointer nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_FN_PTR
#elif defined(__GNUC__) || defined(__clang__)
/** @brief Function-pointer nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_FN_PTR __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && _MSC_VER >= 1700
/** @brief Function-pointer nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_FN_PTR _Check_return_
#else
/** @brief Function-pointer nodiscard attribute macro */
#define C_ABSTRACT_HTTP_NO_DISCARD_FN_PTR
#endif
#endif /* !C_ABSTRACT_HTTP_NO_DISCARD_FN_PTR */

#ifndef NO_DISCARD
/** @brief Compiler attribute warning when return value is discarded */
#define NO_DISCARD C_ABSTRACT_HTTP_NO_DISCARD
#endif /* !NO_DISCARD */

#ifndef NO_DISCARD_TYPE
/** @brief Type-level nodiscard attribute fallback macro */
#define NO_DISCARD_TYPE C_ABSTRACT_HTTP_NO_DISCARD_TYPE
#endif /* !NO_DISCARD_TYPE */

#ifndef NO_DISCARD_TYPEDEF
/** @brief Typedef-level nodiscard attribute fallback macro */
#define NO_DISCARD_TYPEDEF C_ABSTRACT_HTTP_NO_DISCARD_TYPEDEF
#endif /* !NO_DISCARD_TYPEDEF */

#ifndef NO_DISCARD_FN_PTR
/** @brief Function-pointer nodiscard attribute fallback macro */
#define NO_DISCARD_FN_PTR C_ABSTRACT_HTTP_NO_DISCARD_FN_PTR
#endif /* !NO_DISCARD_FN_PTR */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_NO_DISCARD_H */
