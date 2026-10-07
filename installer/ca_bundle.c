#include "ca_bundle.h"

#include <stddef.h>

static ItlsReplaceResult make_result(int error, int cleanup_error) {
    ItlsReplaceResult result = {error, cleanup_error};
    return result;
}

static int operations_are_valid(const ItlsFileOps *operations) {
    return operations != NULL
        && operations->exists != NULL
        && operations->remove != NULL
        && operations->rename != NULL
        && operations->copy != NULL
        && operations->same_size != NULL;
}

static int record_cleanup_error(int current_error, int candidate_error) {
    if (current_error == 0 && candidate_error < 0)
        return candidate_error;
    return current_error;
}

static int remove_if_present(const ItlsFileOps *operations, const char *path) {
    int exists = operations->exists(operations->context, path);
    if (exists < 0)
        return exists;
    if (!exists)
        return 0;
    return operations->remove(operations->context, path);
}

ItlsReplaceResult itls_install_ca_bundle(
    const ItlsFileOps *operations,
    const char *source,
    const char *active,
    const char *backup,
    const char *temporary,
    const char *rollback
) {
    int backup_exists;
    int active_exists;
    int status;
    int cleanup_error = 0;

    if (
        !operations_are_valid(operations)
        || source == NULL
        || active == NULL
        || backup == NULL
        || temporary == NULL
        || rollback == NULL
    ) {
        return make_result(ITLS_FILE_ERROR_INVALID_ARGUMENT, 0);
    }

    backup_exists = operations->exists(operations->context, backup);
    if (backup_exists < 0)
        return make_result(backup_exists, 0);

    if (backup_exists) {
        return itls_transactional_replace(
            operations,
            source,
            active,
            temporary,
            rollback
        );
    }

    active_exists = operations->exists(operations->context, active);
    if (active_exists < 0)
        return make_result(active_exists, 0);
    if (!active_exists)
        return make_result(ITLS_CA_ERROR_MISSING_ORIGINAL, 0);

    status = remove_if_present(operations, temporary);
    if (status < 0)
        return make_result(status, 0);

    status = remove_if_present(operations, rollback);
    if (status < 0)
        return make_result(status, 0);

    status = operations->copy(operations->context, source, temporary);
    if (status < 0) {
        cleanup_error = record_cleanup_error(
            cleanup_error,
            remove_if_present(operations, temporary)
        );
        return make_result(status, cleanup_error);
    }

    status = operations->same_size(operations->context, source, temporary);
    if (status <= 0) {
        int error = status < 0 ? status : ITLS_FILE_ERROR_SIZE_MISMATCH;
        cleanup_error = record_cleanup_error(
            cleanup_error,
            remove_if_present(operations, temporary)
        );
        return make_result(error, cleanup_error);
    }

    status = operations->rename(operations->context, active, backup);
    if (status < 0) {
        cleanup_error = record_cleanup_error(
            cleanup_error,
            remove_if_present(operations, temporary)
        );
        return make_result(status, cleanup_error);
    }

    status = operations->rename(operations->context, temporary, active);
    if (status < 0) {
        cleanup_error = record_cleanup_error(
            cleanup_error,
            operations->rename(operations->context, backup, active)
        );
        cleanup_error = record_cleanup_error(
            cleanup_error,
            remove_if_present(operations, temporary)
        );
        return make_result(status, cleanup_error);
    }

    return make_result(0, 0);
}

ItlsReplaceResult itls_restore_ca_bundle(
    const ItlsFileOps *operations,
    const char *active,
    const char *backup,
    const char *temporary,
    const char *rollback
) {
    ItlsReplaceResult result;
    int active_exists;
    int backup_exists;
    int cleanup_error = 0;
    int cleanup_status;

    if (
        !operations_are_valid(operations)
        || active == NULL
        || backup == NULL
        || temporary == NULL
        || rollback == NULL
    ) {
        return make_result(ITLS_FILE_ERROR_INVALID_ARGUMENT, 0);
    }

    backup_exists = operations->exists(operations->context, backup);
    if (backup_exists < 0)
        return make_result(backup_exists, 0);
    if (!backup_exists) {
        active_exists = operations->exists(operations->context, active);
        if (active_exists < 0)
            return make_result(active_exists, 0);
        if (!active_exists)
            return make_result(ITLS_CA_ERROR_MISSING_ORIGINAL, 0);

        cleanup_error = record_cleanup_error(
            cleanup_error,
            remove_if_present(operations, temporary)
        );
        cleanup_error = record_cleanup_error(
            cleanup_error,
            remove_if_present(operations, rollback)
        );
        return make_result(0, cleanup_error);
    }

    result = itls_transactional_replace(
        operations,
        backup,
        active,
        temporary,
        rollback
    );
    if (result.error < 0)
        return result;

    cleanup_status = operations->remove(operations->context, backup);
    result.cleanup_error = record_cleanup_error(
        result.cleanup_error,
        cleanup_status
    );
    return result;
}
