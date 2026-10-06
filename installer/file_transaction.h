#ifndef ITLS_FILE_TRANSACTION_H
#define ITLS_FILE_TRANSACTION_H

#define ITLS_FILE_ERROR_INVALID_ARGUMENT (-0x7001)
#define ITLS_FILE_ERROR_SIZE_MISMATCH (-0x7002)

typedef struct ItlsFileOps {
    void *context;
    int (*exists)(void *context, const char *path);
    int (*remove)(void *context, const char *path);
    int (*rename)(void *context, const char *from, const char *to);
    int (*copy)(void *context, const char *from, const char *to);
    int (*same_size)(void *context, const char *first, const char *second);
} ItlsFileOps;

typedef struct ItlsReplaceResult {
    int error;
    int cleanup_error;
} ItlsReplaceResult;

ItlsReplaceResult itls_transactional_replace(
    const ItlsFileOps *operations,
    const char *source,
    const char *destination,
    const char *temporary,
    const char *rollback
);

#endif
