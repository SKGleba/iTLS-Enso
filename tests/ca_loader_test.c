#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/x509.h>

int main(int argc, char **argv) {
    BIO *bio = NULL;
    X509_STORE *store = NULL;
    X509 *certificate = NULL;
    unsigned char *fingerprints = NULL;
    size_t parsed = 0;
    size_t added = 0;
    int result = EXIT_FAILURE;

    if (argc != 2) {
        fprintf(stderr, "usage: %s <ca-bundle>\n", argv[0]);
        return EXIT_FAILURE;
    }

    bio = BIO_new_file(argv[1], "r");
    store = X509_STORE_new();
    if (bio == NULL || store == NULL) {
        ERR_print_errors_fp(stderr);
        goto cleanup;
    }

    for (;;) {
        ERR_clear_error();
        certificate = PEM_read_bio_X509_AUX(bio, NULL, NULL, NULL);
        if (certificate == NULL) {
            if (BIO_eof(bio)) {
                ERR_clear_error();
                break;
            }
            fprintf(stderr, "certificate parse failed after index %zu\n", parsed);
            ERR_print_errors_fp(stderr);
            goto cleanup;
        }

        parsed++;
        {
            unsigned char fingerprint[EVP_MAX_MD_SIZE];
            unsigned int fingerprint_size = 0;
            unsigned char *resized;
            size_t index;

            if (X509_digest(certificate, EVP_sha256(), fingerprint, &fingerprint_size) != 1
                || fingerprint_size != 32) {
                fprintf(stderr, "certificate fingerprint failed at index %zu\n", parsed);
                ERR_print_errors_fp(stderr);
                goto cleanup;
            }

            for (index = 0; index < added; index++) {
                if (memcmp(fingerprints + index * 32, fingerprint, 32) == 0) {
                    fprintf(stderr, "duplicate certificate at index %zu\n", parsed);
                    goto cleanup;
                }
            }

            resized = realloc(fingerprints, parsed * 32);
            if (resized == NULL) {
                fprintf(stderr, "fingerprint allocation failed at index %zu\n", parsed);
                goto cleanup;
            }
            fingerprints = resized;
            memcpy(fingerprints + added * 32, fingerprint, 32);
        }

        if (X509_STORE_add_cert(store, certificate) != 1) {
            fprintf(stderr, "certificate add failed at index %zu\n", parsed);
            ERR_print_errors_fp(stderr);
            goto cleanup;
        }

        added++;
        X509_free(certificate);
        certificate = NULL;
    }

    printf("parsed=%zu added=%zu\n", parsed, added);
    result = parsed == added && parsed != 0 ? EXIT_SUCCESS : EXIT_FAILURE;

cleanup:
    free(fingerprints);
    X509_free(certificate);
    X509_STORE_free(store);
    BIO_free(bio);
    return result;
}
