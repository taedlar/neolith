#ifdef HAVE_CONFIG_H
#include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* crypt(3) is a POSIX C library function (libcrypt) typically used
 * for computing password hashes.
 * 
 * This implementation uses OpenSSL for multi-algorithm password hashing:
 * - MD5:     $1$<salt>$<hash> (for compatibility with old drivers)
 * - SHA256:  $5$<salt>$<hash> (recommended for new passwords)
 *
 * Reference: https://en.wikipedia.org/wiki/Crypt_(C)
 * OpenSSL EVP: https://www.openssl.org/docs/man1.1.1/man3/EVP_Digest.html
 */

#ifdef HAVE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
/* Suppress deprecation warnings for OpenSSL functions */
#define OPENSSL_SUPPRESS_DEPRECATED
#endif /* HAVE_OPENSSL */

/* Buffer to hold the hashed result. The MD5/SHA256 crypt format is at most:
 * $1$<salt (up to 8 chars)>$<hash (22 chars)> = ~35 chars for MD5
 * $5$<salt (up to 16 chars)>$<hex hash (64 chars)> = ~85 chars for SHA256
 * We add 128 bytes to be safe.
 */
static char crypt_buffer[256];

/**
 * Generate a random salt using standard C random.
 *
 * @param salt Buffer to store salt
 * @param len Length to generate
 */
static void generate_salt(char *salt, size_t len) {
  static const char saltchars[] = 
    "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789./";
  static int salt_initialized = 0;
  size_t i;

  if (!salt_initialized) {
    srand((unsigned int)time(NULL));
    salt_initialized = 1;
  }

  for (i = 0; i < len; i++) {
    salt[i] = saltchars[rand() % (sizeof(saltchars) - 1)];
  }
  salt[len] = '\0';
}

/**
 * Simple hex encoding of binary data.
 *
 * @param input The binary data to encode
 * @param input_len Length of input data
 * @param output Buffer to store hex output
 * @param output_len Maximum output buffer size
 */
static void hex_encode(const unsigned char *input, size_t input_len,
                       char *output, size_t output_len) {
  size_t i;
  if (output_len < input_len * 2 + 1) {
    return;
  }
  for (i = 0; i < input_len; i++) {
    snprintf(&output[i * 2], 3, "%02x", input[i]);
  }
  output[input_len * 2] = '\0';
}

/**
 * Base64-like encoding for MD5 hashes (crypt alphabet).
 * Uses: ./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz
 *
 * @param input Binary MD5 digest (16 bytes)
 * @param output Buffer for encoded output (min 23 bytes)
 */
static void crypt_md5_encode(const unsigned char *input, char *output) {
  static const char base64_crypt[] =
    "./0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
  int i;
  unsigned int val;

  for (i = 0; i < 3; i++) {
    val = ((unsigned int)input[0 + i * 5] & 0xff) |
          (((unsigned int)input[1 + i * 5] & 0xff) << 8) |
          (((unsigned int)input[2 + i * 5] & 0xff) << 16);

    output[0] = base64_crypt[val & 0x3f];
    output[1] = base64_crypt[(val >> 6) & 0x3f];
    output[2] = base64_crypt[(val >> 12) & 0x3f];
    output[3] = base64_crypt[(val >> 18) & 0x3f];
    output += 4;
  }

  val = ((unsigned int)input[15] & 0xff);
  output[0] = base64_crypt[val & 0x3f];
  output[1] = base64_crypt[(val >> 6) & 0x3f];
  output[2] = '\0';
}

/**
 * Portable crypt() implementation using OpenSSL MD5/SHA256.
 *
 * Supported algorithms (selected by salt prefix):
 * - MD5:     $1$<salt>$<hash>        (for compatibility with old passwords)
 * - SHA256:  $5$<salt>$<hash>        (recommended for new passwords)
 * - Plain:   Auto-detects salt type or generates SHA256
 *
 * @param key The password to hash
 * @param salt The salt to use (format $1$...$, $5$...$, or plain text)
 *             If NULL or < 2 chars, auto-generates SHA256 salt
 * @returns Pointer to static buffer containing result (never NULL)
 */
char* crypt(const char *key, const char *salt) {
  char final_salt[32];
  size_t key_len, salt_len;
  int ret;
  int use_md5 = 0;  /* Default to SHA256, detect MD5 from salt */

  if (!key) {
    return NULL;
  }

  if (!salt) {
    salt = "";
  }

  key_len = strlen(key);
  salt_len = strlen(salt);

  /* Detect algorithm from salt format and extract salt */
  if (salt_len > 0) {
    if (strncmp(salt, "$1$", 3) == 0) {
      /* MD5 format: $1$<salt>$ */
      use_md5 = 1;
      const char *salt_end = strchr(salt + 3, '$');
      if (salt_end) {
        size_t real_salt_len = salt_end - (salt + 3);
        if (real_salt_len > 0 && real_salt_len <= 8) {
          strncpy(final_salt, salt + 3, real_salt_len);
          final_salt[real_salt_len] = '\0';
        } else {
          generate_salt(final_salt, 8);
        }
      } else {
        generate_salt(final_salt, 8);
      }
    } else if (strncmp(salt, "$5$", 3) == 0) {
      /* SHA256 format: $5$<salt>$ */
      use_md5 = 0;
      const char *salt_end = strchr(salt + 3, '$');
      if (salt_end) {
        size_t real_salt_len = salt_end - (salt + 3);
        if (real_salt_len > 0 && real_salt_len <= 31) {
          strncpy(final_salt, salt + 3, real_salt_len);
          final_salt[real_salt_len] = '\0';
        } else {
          generate_salt(final_salt, 8);
        }
      } else if (strncmp(salt, "$5$", 3) == 0) {
        strncpy(final_salt, salt + 3, sizeof(final_salt) - 1);
        final_salt[sizeof(final_salt) - 1] = '\0';
      } else {
        generate_salt(final_salt, 8);
      }
    } else {
      /* Plain text salt */
      if (salt_len < 2) {
        generate_salt(final_salt, 8);
      } else if (salt_len > 31) {
        strncpy(final_salt, salt, 31);
        final_salt[31] = '\0';
      } else {
        strcpy(final_salt, salt);
      }
    }
  } else {
    /* Generate new salt for SHA256 */
    generate_salt(final_salt, 8);
  }

#ifdef HAVE_OPENSSL
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len;
  EVP_MD_CTX *mdctx;
  const EVP_MD *md;
  char encoded_hash[256];

  mdctx = EVP_MD_CTX_new();
  if (mdctx == NULL) {
    goto fallback;
  }

  /* Select algorithm: MD5 or SHA256 */
  md = use_md5 ? EVP_md5() : EVP_sha256();
  if (md == NULL) {
    EVP_MD_CTX_free(mdctx);
    goto fallback;
  }

  /* Initialize context */
  if (EVP_DigestInit_ex(mdctx, md, NULL) == 0) {
    EVP_MD_CTX_free(mdctx);
    goto fallback;
  }

  /* Update with key */
  if (EVP_DigestUpdate(mdctx, (unsigned char *)key, key_len) == 0) {
    EVP_MD_CTX_free(mdctx);
    goto fallback;
  }

  /* Update with salt */
  if (EVP_DigestUpdate(mdctx, (unsigned char *)final_salt, strlen(final_salt)) == 0) {
    EVP_MD_CTX_free(mdctx);
    goto fallback;
  }

  /* Finalize */
  if (EVP_DigestFinal_ex(mdctx, digest, &digest_len) == 0) {
    EVP_MD_CTX_free(mdctx);
    goto fallback;
  }

  EVP_MD_CTX_free(mdctx);

  /* Encode digest based on algorithm */
  if (use_md5) {
    crypt_md5_encode(digest, encoded_hash);
    ret = snprintf(crypt_buffer, sizeof(crypt_buffer), "$1$%s$%s",
                   final_salt, encoded_hash);
  } else {
    hex_encode(digest, digest_len, encoded_hash, sizeof(encoded_hash));
    ret = snprintf(crypt_buffer, sizeof(crypt_buffer), "$5$%s$%s",
                   final_salt, encoded_hash);
  }

  if (ret >= 0 && (size_t)ret < sizeof(crypt_buffer)) {
    return crypt_buffer;
  }

  /* Fall through if snprintf failed */
fallback:

#endif /* HAVE_OPENSSL */

  /* Fallback: simple hash without OpenSSL */
  {
    unsigned long hash = 5381;
    size_t i;

    for (i = 0; i < key_len; i++) {
      hash = ((hash << 5) + hash) ^ ((unsigned char)key[i]);
    }

    for (i = 0; i < strlen(final_salt); i++) {
      hash = ((hash << 5) + hash) ^ ((unsigned char)final_salt[i]);
    }

    if (use_md5) {
      ret = snprintf(crypt_buffer, sizeof(crypt_buffer),
                     "$1$%s$%08lx", final_salt, hash);
    } else {
      ret = snprintf(crypt_buffer, sizeof(crypt_buffer),
                     "$5$%s$%016lx", final_salt, hash);
    }

    if (ret >= 0 && (size_t)ret < sizeof(crypt_buffer)) {
      return crypt_buffer;
    }

    return NULL;
  }
}
