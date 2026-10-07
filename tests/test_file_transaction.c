#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../installer/file_transaction.h"

#define SOURCE "source"
#define DESTINATION "destination"
#define TEMPORARY "temporary"
#define ROLLBACK "rollback"
#define FAKE_ERROR (-1234)

typedef struct FakeFileSystem {
    int source;
    int destination;
    int temporary;
    int rollback;
    const char *fail_operation;
    const char *fail_from;
    const char *fail_to;
    int same_size;
} FakeFileSystem;

static int *path_slot(FakeFileSystem *file_system, const char *path) {
    if (strcmp(path, SOURCE) == 0)
        return &file_system->source;
    if (strcmp(path, DESTINATION) == 0)
        return &file_system->destination;
    if (strcmp(path, TEMPORARY) == 0)
        return &file_system->temporary;
    if (strcmp(path, ROLLBACK) == 0)
        return &file_system->rollback;
    return NULL;
}

static int should_fail(
    const FakeFileSystem *file_system,
    const char *operation,
    const char *from,
    const char *to
) {
    return file_system->fail_operation != NULL
        && strcmp(file_system->fail_operation, operation) == 0
        && (file_system->fail_from == NULL || strcmp(file_system->fail_from, from) == 0)
        && (file_system->fail_to == NULL || strcmp(file_system->fail_to, to) == 0);
}

static int fake_exists(void *context, const char *path) {
    int *slot = path_slot(context, path);
    return slot == NULL ? FAKE_ERROR : *slot;
}

static int fake_remove(void *context, const char *path) {
    FakeFileSystem *file_system = context;
    int *slot = path_slot(file_system, path);
    if (should_fail(file_system, "remove", path, NULL))
        return FAKE_ERROR;
    if (slot == NULL)
        return FAKE_ERROR;
    *slot = 0;
    return 0;
}

static int fake_rename(void *context, const char *from, const char *to) {
    FakeFileSystem *file_system = context;
    int *from_slot = path_slot(file_system, from);
    int *to_slot = path_slot(file_system, to);
    if (should_fail(file_system, "rename", from, to))
        return FAKE_ERROR;
    if (from_slot == NULL || to_slot == NULL || !*from_slot || *to_slot)
        return FAKE_ERROR;
    *from_slot = 0;
    *to_slot = 1;
    return 0;
}

static int fake_copy(void *context, const char *from, const char *to) {
    FakeFileSystem *file_system = context;
    int *from_slot = path_slot(file_system, from);
    int *to_slot = path_slot(file_system, to);
    if (should_fail(file_system, "copy", from, to))
        return FAKE_ERROR;
    if (from_slot == NULL || to_slot == NULL || !*from_slot)
        return FAKE_ERROR;
    *to_slot = 1;
    return 0;
}

static int fake_same_size(void *context, const char *first, const char *second) {
    FakeFileSystem *file_system = context;
    int *first_slot = path_slot(file_system, first);
    int *second_slot = path_slot(file_system, second);
    if (should_fail(file_system, "same_size", first, second))
        return FAKE_ERROR;
    if (first_slot == NULL || second_slot == NULL || !*first_slot || !*second_slot)
        return FAKE_ERROR;
    return file_system->same_size;
}

static ItlsFileOps fake_ops(FakeFileSystem *file_system) {
    ItlsFileOps operations = {
        file_system,
        fake_exists,
        fake_remove,
        fake_rename,
        fake_copy,
        fake_same_size,
    };
    return operations;
}

static void test_installs_when_destination_is_absent(void) {
    FakeFileSystem file_system = {1, 0, 0, 0, NULL, NULL, NULL, 1};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_transactional_replace(
        &operations, SOURCE, DESTINATION, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert(file_system.source == 1);
    assert(file_system.destination == 1);
    assert(file_system.temporary == 0);
    assert(file_system.rollback == 0);
}

static void test_replaces_existing_destination_and_removes_rollback(void) {
    FakeFileSystem file_system = {1, 1, 0, 0, NULL, NULL, NULL, 1};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_transactional_replace(
        &operations, SOURCE, DESTINATION, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert(file_system.destination == 1);
    assert(file_system.temporary == 0);
    assert(file_system.rollback == 0);
}

static void test_copy_failure_leaves_destination_untouched(void) {
    FakeFileSystem file_system = {
        1, 1, 0, 0, "copy", SOURCE, TEMPORARY, 1
    };
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_transactional_replace(
        &operations, SOURCE, DESTINATION, TEMPORARY, ROLLBACK
    );

    assert(result.error == FAKE_ERROR);
    assert(file_system.destination == 1);
    assert(file_system.temporary == 0);
    assert(file_system.rollback == 0);
}

static void test_activation_failure_restores_destination(void) {
    FakeFileSystem file_system = {
        1, 1, 0, 0, "rename", TEMPORARY, DESTINATION, 1
    };
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_transactional_replace(
        &operations, SOURCE, DESTINATION, TEMPORARY, ROLLBACK
    );

    assert(result.error == FAKE_ERROR);
    assert(result.cleanup_error == 0);
    assert(file_system.destination == 1);
    assert(file_system.temporary == 0);
    assert(file_system.rollback == 0);
}

static void test_interrupted_transaction_is_recovered_before_update(void) {
    FakeFileSystem file_system = {1, 0, 1, 1, NULL, NULL, NULL, 1};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_transactional_replace(
        &operations, SOURCE, DESTINATION, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(file_system.destination == 1);
    assert(file_system.temporary == 0);
    assert(file_system.rollback == 0);
}

static void test_size_mismatch_does_not_touch_destination(void) {
    FakeFileSystem file_system = {1, 1, 0, 0, NULL, NULL, NULL, 0};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_transactional_replace(
        &operations, SOURCE, DESTINATION, TEMPORARY, ROLLBACK
    );

    assert(result.error == ITLS_FILE_ERROR_SIZE_MISMATCH);
    assert(file_system.destination == 1);
    assert(file_system.temporary == 0);
    assert(file_system.rollback == 0);
}

static void test_cleanup_failure_is_reported_after_success(void) {
    FakeFileSystem file_system = {
        1, 1, 0, 0, "remove", ROLLBACK, NULL, 1
    };
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_transactional_replace(
        &operations, SOURCE, DESTINATION, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(result.cleanup_error == FAKE_ERROR);
    assert(file_system.destination == 1);
    assert(file_system.rollback == 1);
}

int main(void) {
    test_installs_when_destination_is_absent();
    test_replaces_existing_destination_and_removes_rollback();
    test_copy_failure_leaves_destination_untouched();
    test_activation_failure_restores_destination();
    test_interrupted_transaction_is_recovered_before_update();
    test_size_mismatch_does_not_touch_destination();
    test_cleanup_failure_is_reported_after_success();
    puts("file transaction tests passed");
    return 0;
}
