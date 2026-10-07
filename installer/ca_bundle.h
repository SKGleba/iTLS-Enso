#ifndef ITLS_CA_BUNDLE_H
#define ITLS_CA_BUNDLE_H

#include "file_transaction.h"

#define ITLS_CA_ERROR_MISSING_ORIGINAL (-0x7301)

ItlsReplaceResult itls_install_ca_bundle(
    const ItlsFileOps *operations,
    const char *source,
    const char *active,
    const char *backup,
    const char *temporary,
    const char *rollback
);

ItlsReplaceResult itls_restore_ca_bundle(
    const ItlsFileOps *operations,
    const char *active,
    const char *backup,
    const char *temporary,
    const char *rollback
);

#endif
