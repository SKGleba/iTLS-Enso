#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../installer/ca_bundle.h"

#define SOURCE "source"
#define ACTIVE "active"
#define BACKUP "backup"
#define TEMPORARY "temporary"
#define ROLLBACK "rollback"
#define FAKE_ERROR (-4321)

typedef struct FakeFileSystem {
    int source;
    int active;
    int backup;
    int temporary;
    int rollback;
    const char *fail_operation;
    const char *fail_from;
    const char *fail_to;
} FakeFileSystem;

static int *path_slot(FakeFileSystem *file_system, const char *path) {
    if (strcmp(path, SOURCE) == 0)
        return &file_system->source;
    if (strcmp(path, ACTIVE) == 0)
        return &file_system->active;
    if (strcmp(path, BACKUP) == 0)
        return &file_system->backup;
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
    return slot == NULL ? FAKE_ERROR : *slot != 0;
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
    if (from_slot == NULL || to_slot == NULL || *from_slot == 0 || *to_slot != 0)
        return FAKE_ERROR;
    *to_slot = *from_slot;
    *from_slot = 0;
    return 0;
}

static int fake_copy(void *context, const char *from, const char *to) {
    FakeFileSystem *file_system = context;
    int *from_slot = path_slot(file_system, from);
    int *to_slot = path_slot(file_system, to);
    if (should_fail(file_system, "copy", from, to))
        return FAKE_ERROR;
    if (from_slot == NULL || to_slot == NULL || *from_slot == 0)
        return FAKE_ERROR;
    *to_slot = *from_slot;
    return 0;
}

static int fake_same_size(void *context, const char *first, const char *second) {
    FakeFileSystem *file_system = context;
    int *first_slot = path_slot(file_system, first);
    int *second_slot = path_slot(file_system, second);
    if (first_slot == NULL || second_slot == NULL)
        return FAKE_ERROR;
    return *first_slot != 0 && *second_slot != 0;
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

static void test_first_install_preserves_original_bundle(void) {
    FakeFileSystem file_system = {2, 1, 0, 0, 0, NULL, NULL, NULL};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_install_ca_bundle(
        &operations, SOURCE, ACTIVE, BACKUP, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert(file_system.active == 2);
    assert(file_system.backup == 1);
    assert(file_system.temporary == 0);
    assert(file_system.rollback == 0);
}

static void test_update_keeps_original_backup(void) {
    FakeFileSystem file_system = {3, 2, 1, 0, 0, NULL, NULL, NULL};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_install_ca_bundle(
        &operations, SOURCE, ACTIVE, BACKUP, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(file_system.active == 3);
    assert(file_system.backup == 1);
    assert(file_system.rollback == 0);
}

static void test_first_install_requires_an_original_bundle(void) {
    FakeFileSystem file_system = {2, 0, 0, 0, 0, NULL, NULL, NULL};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_install_ca_bundle(
        &operations, SOURCE, ACTIVE, BACKUP, TEMPORARY, ROLLBACK
    );

    assert(result.error == ITLS_CA_ERROR_MISSING_ORIGINAL);
    assert(file_system.active == 0);
    assert(file_system.backup == 0);
}

static void test_first_install_activation_failure_restores_original(void) {
    FakeFileSystem file_system = {
        2, 1, 0, 0, 0, "rename", TEMPORARY, ACTIVE
    };
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_install_ca_bundle(
        &operations, SOURCE, ACTIVE, BACKUP, TEMPORARY, ROLLBACK
    );

    assert(result.error == FAKE_ERROR);
    assert(result.cleanup_error == 0);
    assert(file_system.active == 1);
    assert(file_system.backup == 0);
    assert(file_system.temporary == 0);
}

static void test_restore_activates_original_and_removes_backup(void) {
    FakeFileSystem file_system = {3, 2, 1, 0, 0, NULL, NULL, NULL};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_restore_ca_bundle(
        &operations, ACTIVE, BACKUP, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert(file_system.active == 1);
    assert(file_system.backup == 0);
    assert(file_system.rollback == 0);
}

static void test_restore_without_backup_is_idempotent(void) {
    FakeFileSystem file_system = {3, 2, 0, 0, 0, NULL, NULL, NULL};
    ItlsFileOps operations = fake_ops(&file_system);
    ItlsReplaceResult result = itls_restore_ca_bundle(
        &operations, ACTIVE, BACKUP, TEMPORARY, ROLLBACK
    );

    assert(result.error == 0);
    assert(file_system.active == 2);
    assert(file_system.backup == 0);
}

int main(void) {
    test_first_install_preserves_original_bundle();
    test_update_keeps_original_backup();
    test_first_install_requires_an_original_bundle();
    test_first_install_activation_failure_restores_original();
    test_restore_activates_original_and_removes_backup();
    test_restore_without_backup_is_idempotent();
    puts("CA bundle transaction tests passed");
    return 0;
}
