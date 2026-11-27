/**
 * @file mock_wincrypt.h
 * @brief Internal test-only CryptoAPI (`crypt32.dll`) stubs for non-Win32 unit
 * and OOM tests.
 */

#ifndef C_ABSTRACT_HTTP_TEST_MOCK_WINCRYPT_H
#define C_ABSTRACT_HTTP_TEST_MOCK_WINCRYPT_H

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* clang-format off */
#include <stdlib.h>
#include <string.h>
/* clang-format on */

/** @brief Default mock server DER X.509v3 certificate (`SPKI = 30 03 61 62 63`)
 */
static const unsigned char k_cah_mock_server_cert_der[] = {
    0x30, 0x1C, 0x30, 0x15, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x02,
    0x01, 0x01, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00, 0x30, 0x00,
    0x30, 0x03, 0x61, 0x62, 0x63, 0x30, 0x00, 0x03, 0x01, 0x00};

/** @brief Fault-injection state for CryptoAPI mock operations */
static int s_mock_wincrypt_open_store_fail = 0;
/** @brief Fault-injection flag for CertAddEncodedCertificateToStore */
static int s_mock_wincrypt_add_cert_fail = 0;
/** @brief Fault-injection flag for CertCreateCertificateContext */
static int s_mock_wincrypt_create_cert_fail = 0;
/** @brief Fault-injection mode for CertGetCertificateChain (0=ok, 1=fail,
 * 2=bad_trust, 3=empty_chain) */
static int s_mock_wincrypt_chain_mode = 0;
/** @brief Fault-injection mode for server cert query (0=ok, 1=fail,
 * 2=malformed_der) */
static int s_mock_wincrypt_query_cert_mode = 0;

/** @brief In-memory mock certificate store container */
struct CahMockCertStore {
  CAH_CERT_CONTEXT *certs[8]; /**< Stored certificate contexts */
  size_t count;               /**< Number of stored certificates */
};

static void *CertOpenStore(const char *lpszStoreProvider, DWORD dwEncodingType,
                           size_t hCryptProv, DWORD dwFlags,
                           const void *pvPara) {
  struct CahMockCertStore *st;
  if (lpszStoreProvider || dwEncodingType || hCryptProv || dwFlags || pvPara) {
  }
  if (s_mock_wincrypt_open_store_fail) {
    return NULL;
  }
  st = (struct CahMockCertStore *)calloc(1, sizeof(*st));
  return st;
}

static CAH_CERT_CONTEXT *
CertCreateCertificateContext(DWORD dwCertEncodingType,
                             const unsigned char *pbCertEncoded,
                             DWORD cbCertEncoded) {
  CAH_CERT_CONTEXT *ctx;
  unsigned char *copy;
  if (s_mock_wincrypt_create_cert_fail || !pbCertEncoded ||
      cbCertEncoded == 0U) {
    return NULL;
  }
  ctx = (CAH_CERT_CONTEXT *)calloc(1, sizeof(*ctx));
  if (!ctx) {
    return NULL;
  }
  copy = (unsigned char *)malloc((size_t)cbCertEncoded);
  if (!copy) {
    free(ctx);
    return NULL;
  }
  memcpy(copy, pbCertEncoded, (size_t)cbCertEncoded);
  ctx->dwCertEncodingType = dwCertEncodingType;
  ctx->pbCertEncoded = copy;
  ctx->cbCertEncoded = cbCertEncoded;
  return ctx;
}

static BOOL CertFreeCertificateContext(CAH_CERT_CONTEXT *pCertContext) {
  if (pCertContext) {
    free(pCertContext->pbCertEncoded);
    free(pCertContext);
  }
  return 1;
}

static BOOL CertCloseStore(void *hCertStore, DWORD dwFlags) {
  struct CahMockCertStore *st = (struct CahMockCertStore *)hCertStore;
  size_t i;
  if (dwFlags) {
  }
  if (st) {
    for (i = 0; i < st->count; ++i) {
      CertFreeCertificateContext(st->certs[i]);
    }
    free(st);
  }
  return 1;
}

static BOOL CertAddEncodedCertificateToStore(void *hCertStore,
                                             DWORD dwCertEncodingType,
                                             const unsigned char *pbCertEncoded,
                                             DWORD cbCertEncoded,
                                             DWORD dwAddDisposition,
                                             CAH_CERT_CONTEXT **ppCertContext) {
  struct CahMockCertStore *st = (struct CahMockCertStore *)hCertStore;
  CAH_CERT_CONTEXT *c;
  if (dwAddDisposition) {
  }
  if (!st || s_mock_wincrypt_add_cert_fail || st->count >= 8U) {
    return 0;
  }
  c = CertCreateCertificateContext(dwCertEncodingType, pbCertEncoded,
                                   cbCertEncoded);
  if (!c) {
    return 0;
  }
  st->certs[st->count++] = c;
  if (ppCertContext) {
    *ppCertContext = CertCreateCertificateContext(dwCertEncodingType,
                                                  pbCertEncoded, cbCertEncoded);
  }
  return 1;
}

static CAH_CERT_CONTEXT *
CertEnumCertificatesInStore(void *hCertStore,
                            CAH_CERT_CONTEXT *pPrevCertContext) {
  struct CahMockCertStore *st = (struct CahMockCertStore *)hCertStore;
  size_t idx = 0;
  if (!st || st->count == 0U) {
    if (pPrevCertContext) {
      CertFreeCertificateContext(pPrevCertContext);
    }
    return NULL;
  }
  if (pPrevCertContext) {
    size_t i;
    idx = st->count;
    for (i = 0; i < st->count; ++i) {
      if (st->certs[i]->cbCertEncoded == pPrevCertContext->cbCertEncoded &&
          memcmp(st->certs[i]->pbCertEncoded, pPrevCertContext->pbCertEncoded,
                 (size_t)pPrevCertContext->cbCertEncoded) == 0) {
        idx = i + 1U;
        break;
      }
    }
    CertFreeCertificateContext(pPrevCertContext);
    if (idx >= st->count) {
      return NULL;
    }
  }
  return CertCreateCertificateContext(st->certs[idx]->dwCertEncodingType,
                                      st->certs[idx]->pbCertEncoded,
                                      st->certs[idx]->cbCertEncoded);
}

static void *PFXImportCertStore(const CAH_CRYPTOAPI_BLOB *pPFX,
                                const wchar_t *szPassword, DWORD dwFlags) {
  void *st;
  if (szPassword || dwFlags) {
  }
  /* Mock PFX magic prefix: {0x30, 0x06, 'P', 'F', 'X', '1', 0x00, 0x00} */
  if (!pPFX || !pPFX->pbData || pPFX->cbData < 6U || pPFX->pbData[0] != 0x30U ||
      memcmp(pPFX->pbData + 2, "PFX1", 4) != 0) {
    return NULL;
  }
  st = CertOpenStore(CAH_CERT_STORE_PROV_MEMORY, 0, 0, 0, NULL);
  if (!st) {
    return NULL;
  }
  if (pPFX->cbData >= 8U && pPFX->pbData[6] == 'E') {
    /* Empty PFX store with 0 certs for coverage */
    return st;
  }
  if (!CertAddEncodedCertificateToStore(
          st, CAH_X509_PKCS7_ENCODING, k_cah_mock_server_cert_der,
          (DWORD)sizeof(k_cah_mock_server_cert_der), CAH_CERT_STORE_ADD_ALWAYS,
          NULL)) {
    CertCloseStore(st, 0);
    return NULL;
  }
  return st;
}

static BOOL CertGetCertificateChain(void *hChainEngine,
                                    CAH_CERT_CONTEXT *pCertContext, void *pTime,
                                    void *hAdditionalStore,
                                    const CAH_CERT_CHAIN_PARA *pChainPara,
                                    DWORD dwFlags, void *pvReserved,
                                    CAH_CERT_CHAIN_CONTEXT **ppChainContext) {
  CAH_CERT_CHAIN_CONTEXT *cc;
  CAH_CERT_SIMPLE_CHAIN *sc;
  CAH_CERT_CHAIN_ELEMENT *el;
  if (hChainEngine || pTime || hAdditionalStore || pChainPara || dwFlags ||
      pvReserved) {
  }
  if (!pCertContext || !ppChainContext || s_mock_wincrypt_chain_mode == 1) {
    return 0;
  }
  cc = (CAH_CERT_CHAIN_CONTEXT *)calloc(1, sizeof(*cc));
  if (!cc) {
    return 0;
  }
  cc->cbSize = (DWORD)sizeof(*cc);
  if (s_mock_wincrypt_chain_mode == 3) {
    *ppChainContext = cc;
    return 1;
  }
  sc = (CAH_CERT_SIMPLE_CHAIN *)calloc(1, sizeof(*sc));
  el = (CAH_CERT_CHAIN_ELEMENT *)calloc(1, sizeof(*el));
  sc->rgpElement =
      (CAH_CERT_CHAIN_ELEMENT **)calloc(1, sizeof(CAH_CERT_CHAIN_ELEMENT *));
  cc->rgpChain =
      (CAH_CERT_SIMPLE_CHAIN **)calloc(1, sizeof(CAH_CERT_SIMPLE_CHAIN *));
  if (!sc || !el || !sc->rgpElement || !cc->rgpChain) {
    free(sc ? sc->rgpElement : NULL);
    free(cc->rgpChain);
    free(el);
    free(sc);
    free(cc);
    return 0;
  }
  el->cbSize = (DWORD)sizeof(*el);
  el->pCertContext = CertCreateCertificateContext(
      pCertContext->dwCertEncodingType, pCertContext->pbCertEncoded,
      pCertContext->cbCertEncoded);
  if (!el->pCertContext) {
    free(sc->rgpElement);
    free(cc->rgpChain);
    free(el);
    free(sc);
    free(cc);
    return 0;
  }
  sc->cbSize = (DWORD)sizeof(*sc);
  sc->cElement = 1;
  sc->rgpElement[0] = el;
  cc->cChain = 1;
  cc->rgpChain[0] = sc;
  cc->TrustStatus.dwErrorStatus = (s_mock_wincrypt_chain_mode == 2)
                                      ? 0x00000001UL
                                      : CAH_CERT_TRUST_IS_UNTRUSTED_ROOT;
  *ppChainContext = cc;
  return 1;
}

static void CertFreeCertificateChain(CAH_CERT_CHAIN_CONTEXT *cc) {
  if (cc) {
    DWORD i;
    for (i = 0; i < cc->cChain; ++i) {
      CAH_CERT_SIMPLE_CHAIN *sc = cc->rgpChain[i];
      if (sc) {
        DWORD j;
        for (j = 0; j < sc->cElement; ++j) {
          if (sc->rgpElement[j]) {
            CertFreeCertificateContext(sc->rgpElement[j]->pCertContext);
            free(sc->rgpElement[j]);
          }
        }
        free(sc->rgpElement);
        free(sc);
      }
    }
    free(cc->rgpChain);
    free(cc);
  }
}

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* C_ABSTRACT_HTTP_TEST_MOCK_WINCRYPT_H */
