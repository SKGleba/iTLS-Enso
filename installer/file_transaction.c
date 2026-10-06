#include "file_transaction.h"

#include <stddef.h>

static ItlsReplaceResult make_result(int error, int cleanup_error) {
    ItlsReplaceResult result = {error, cleanup_error};
    return result;
}

static int remove_if_present(const ItlsFileOps *operations, const char *path) {
    int exists = operations->exists(operations->context, path);
    if (exists < 0)
        return exists;
    if (!exists)
        return 0;
    return operations->remove(operations->context, path);
}

static int record_cleanup_error(int current_error, int candidate_error) {
    if (current_error == 0 && candidate_error < 0)
        return candidate_error;
    return current_error;
}

ItlsReplaceResult itls_transactional_replace(
    const ItlsFileOps *operations,
    const char *source,
    const char *destination,
    const char *temporary,
    const char *rollback
) {
    int destination_exists;
    int rollback_exists;
    int status;
    int cleanup_error = 0;
    int destination_moved = 0;

    if (
        operations == NULL
        || operations->exists == NULL
        || operations->remove == NULL
        || operations->rename == NULL
        || operations->copy == NULL
        || operations->same_size == NULL
        || source == NULL
        || destination == NULL
        || temporary == NULL
        || rollback == NULL
    ) {
        return make_result(ITLS_FILE_ERROR_INVALID_ARGUMENT, 0);
    }

    destination_exists = operations->exists(operations->context, destination);
    if (destination_exists < 0)
        return make_result(destination_exists, 0);

    rollback_exists = operations->exists(operations->context, rollback);
    if (rollback_exists < 0)
        return make_result(rollback_exists, 0);

    if (rollback_exists) {
        if (destination_exists) {
            status = operations->remove(operations->context, rollback);
            if (status < 0)
                return make_result(status, 0);
        } else {
            status = operations->rename(
                operations->context,
                rollback,
                destination
            );
            if (status < 0)
                return make_result(status, 0);
            destination_exists = 1;
        }
    }

    status = remove_if_present(operations, temporary);
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

    if (destination_exists) {
        status = operations->rename(
            operations->context,
            destination,
            rollback
        );
        if (status < 0) {
            cleanup_error = record_cleanup_error(
                cleanup_error,
                remove_if_present(operations, temporary)
            );
            return make_result(status, cleanup_error);
        }
        destination_moved = 1;
    }

    status = operations->rename(operations->context, temporary, destination);
    if (status < 0) {
        if (destination_moved) {
            cleanup_error = record_cleanup_error(
                cleanup_error,
                operations->rename(
                    operations->context,
                    rollback,
                    destination
                )
            );
        }
        cleanup_error = record_cleanup_error(
            cleanup_error,
            remove_if_present(operations, temporary)
        );
        return make_result(status, cleanup_error);
    }

    if (destination_moved) {
        cleanup_error = record_cleanup_error(
            cleanup_error,
            operations->remove(operations->context, rollback)
        );
    }

    return make_result(0, cleanup_error);
}
