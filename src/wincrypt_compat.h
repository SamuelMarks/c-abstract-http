/**
 * @file wincrypt_compat.h
 * @brief Minimal `<windows.h>`-free Win32 CryptoAPI (`crypt32.dll`) ABI types
 * and imports for WinHTTP and WinINet TLS CA, mTLS, and SPKI verification.
 */

#ifndef C_ABSTRACT_HTTP_WINCRYPT_COMPAT_H
#define C_ABSTRACT_HTTP_WINCRYPT_COMPAT_H

/* clang-format off */
#include <stddef.h>
#include <wchar.h>
/* clang-format on */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#if !defined(_WIN32) || (defined(_MSC_VER) && _MSC_VER < 1600 &&               \
                         !defined(_WINSOCK2API_) && !defined(_WINDEF_))
#ifndef CAH_WIN32_BASIC_TYPES_DEFINED
/** @brief Guard indicating fallback Win32 DWORD/BOOL typedefs are active */
#define CAH_WIN32_BASIC_TYPES_DEFINED 1
/** @brief Unsigned 32-bit integer fallback typedef */
typedef unsigned long DWORD;
/** @brief Signed integer boolean fallback typedef */
typedef int BOOL;
#endif
#endif

/** @brief X.509 ASN.1 + PKCS#7 certificate encoding bitmask */
#define CAH_X509_PKCS7_ENCODING 0x00010001UL
/** @brief CryptoAPI in-memory certificate store provider constant (`2`) */
#define CAH_CERT_STORE_PROV_MEMORY ((const char *)(size_t)2U)
/** @brief CryptoAPI `CERT_STORE_ADD_ALWAYS` disposition (`4`) */
#define CAH_CERT_STORE_ADD_ALWAYS 4UL
/** @brief CryptoAPI `CERT_TRUST_IS_UNTRUSTED_ROOT` status flag (`0x20`) */
#define CAH_CERT_TRUST_IS_UNTRUSTED_ROOT 0x00000020UL
/** @brief CryptoAPI offline chain-building flag (`0x00000004`) */
#define CAH_CERT_CHAIN_CACHE_ONLY_URL_RETRIEVAL 0x00000004UL

/** @brief ABI-compatible CryptoAPI `CRYPT_DATA_BLOB` descriptor */
typedef struct _CAH_CRYPTOAPI_BLOB {
  DWORD cbData;          /**< Byte length of `pbData` */
  unsigned char *pbData; /**< Raw byte buffer pointer */
} CAH_CRYPTOAPI_BLOB;

/** @brief ABI-compatible CryptoAPI `CERT_CONTEXT` structure */
typedef struct _CAH_CERT_CONTEXT {
  DWORD dwCertEncodingType;     /**< Encoding type flags */
  unsigned char *pbCertEncoded; /**< DER-encoded X.509 certificate bytes */
  DWORD cbCertEncoded;          /**< Byte length of `pbCertEncoded` */
  void *pCertInfo;              /**< Opaque `PCERT_INFO` pointer */
  void *hCertStore;             /**< Owning `HCERTSTORE` handle */
} CAH_CERT_CONTEXT;

/** @brief ABI-compatible CryptoAPI `CERT_TRUST_STATUS` structure */
typedef struct _CAH_CERT_TRUST_STATUS {
  DWORD dwErrorStatus; /**< Bitmask of chain error flags */
  DWORD dwInfoStatus;  /**< Bitmask of chain informational flags */
} CAH_CERT_TRUST_STATUS;

/** @brief ABI-compatible CryptoAPI `CERT_CHAIN_ELEMENT` structure */
typedef struct _CAH_CERT_CHAIN_ELEMENT {
  DWORD cbSize;                         /**< Struct size in bytes */
  CAH_CERT_CONTEXT *pCertContext;       /**< Certificate context at this hop */
  CAH_CERT_TRUST_STATUS TrustStatus;    /**< Per-element trust status */
  void *pRevocationInfo;                /**< Optional revocation info */
  void *pIssuanceUsage;                 /**< Optional issuance OIDs */
  void *pApplicationUsage;              /**< Optional EKU OIDs */
  const wchar_t *pwszExtendedErrorInfo; /**< Optional extended error string */
} CAH_CERT_CHAIN_ELEMENT;

/** @brief ABI-compatible CryptoAPI `CERT_SIMPLE_CHAIN` structure */
typedef struct _CAH_CERT_SIMPLE_CHAIN {
  DWORD cbSize;                        /**< Struct size in bytes */
  CAH_CERT_TRUST_STATUS TrustStatus;   /**< Aggregate chain trust status */
  DWORD cElement;                      /**< Number of elements in chain */
  CAH_CERT_CHAIN_ELEMENT **rgpElement; /**< Array of element pointers */
  void *pTrustListInfo;                /**< Optional CTL info */
  BOOL fHasRevocationFreshnessTime;    /**< Revocation freshness indicator */
  DWORD dwRevocationFreshnessTime;     /**< Revocation freshness seconds */
} CAH_CERT_SIMPLE_CHAIN;

/** @brief ABI-compatible CryptoAPI `CERT_CHAIN_CONTEXT` structure */
typedef struct _CAH_CERT_CHAIN_CONTEXT {
  DWORD cbSize;                       /**< Struct size in bytes */
  CAH_CERT_TRUST_STATUS TrustStatus;  /**< Overall chain trust status */
  DWORD cChain;                       /**< Number of simple chains */
  CAH_CERT_SIMPLE_CHAIN **rgpChain;   /**< Simple chains (`[0]` is primary) */
  DWORD cLowerQualityChainContext;    /**< Count of fallback chains */
  void **rgpLowerQualityChainContext; /**< Array of fallback chains */
  BOOL fHasRevocationFreshnessTime;   /**< Revocation freshness indicator */
  DWORD dwRevocationFreshnessTime;    /**< Revocation freshness seconds */
  DWORD dwCreateFlags;                /**< Chain creation flags */
  unsigned char ChainId[16];          /**< Chain GUID bytes */
} CAH_CERT_CHAIN_CONTEXT;

/** @brief ABI-compatible CryptoAPI `CERT_CHAIN_PARA` structure */
typedef struct _CAH_CERT_CHAIN_PARA {
  DWORD cbSize;          /**< Struct size in bytes */
  DWORD dwUsageType;     /**< `USAGE_MATCH_TYPE_AND` (`0`) */
  DWORD cUsageOids;      /**< Number of EKU OIDs */
  char **rgpszUsageOids; /**< Array of EKU OID strings */
} CAH_CERT_CHAIN_PARA;

#if defined(_WIN32)
__declspec(dllimport) void *__stdcall CertOpenStore(
    const char *lpszStoreProvider, DWORD dwEncodingType, size_t hCryptProv,
    DWORD dwFlags, const void *pvPara);
__declspec(dllimport) BOOL __stdcall CertCloseStore(void *hCertStore,
                                                    DWORD dwFlags);
__declspec(dllimport) BOOL __stdcall CertAddEncodedCertificateToStore(
    void *hCertStore, DWORD dwCertEncodingType,
    const unsigned char *pbCertEncoded, DWORD cbCertEncoded,
    DWORD dwAddDisposition, CAH_CERT_CONTEXT **ppCertContext);
__declspec(dllimport) CAH_CERT_CONTEXT *__stdcall CertCreateCertificateContext(
    DWORD dwCertEncodingType, const unsigned char *pbCertEncoded,
    DWORD cbCertEncoded);
__declspec(dllimport)
BOOL __stdcall CertFreeCertificateContext(CAH_CERT_CONTEXT *pCertContext);
__declspec(dllimport) CAH_CERT_CONTEXT *__stdcall CertEnumCertificatesInStore(
    void *hCertStore, CAH_CERT_CONTEXT *pPrevCertContext);
__declspec(dllimport) void *__stdcall PFXImportCertStore(
    const CAH_CRYPTOAPI_BLOB *pPFX, const wchar_t *szPassword, DWORD dwFlags);
__declspec(dllimport) BOOL __stdcall CertGetCertificateChain(
    void *hChainEngine, CAH_CERT_CONTEXT *pCertContext, void *pTime,
    void *hAdditionalStore, const CAH_CERT_CHAIN_PARA *pChainPara,
    DWORD dwFlags, void *pvReserved, CAH_CERT_CHAIN_CONTEXT **ppChainContext);
__declspec(dllimport) void __stdcall CertFreeCertificateChain(
    CAH_CERT_CHAIN_CONTEXT *pChainContext);
#endif

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_WINCRYPT_COMPAT_H */
