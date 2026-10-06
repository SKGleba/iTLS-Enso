#define _XOPEN_SOURCE 700

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../installer/ca_bundle.h"
#include "../installer/file_transaction.h"

#define TEST_PATH_SIZE 1024

static int posix_exists(void *context, const char *path) {
    struct stat status;
    (void)context;

    if (stat(path, &status) == 0)
        return 1;
    if (errno == ENOENT)
        return 0;
    return -errno;
}

static int posix_remove(void *context, const char *path) {
    (void)context;
    if (unlink(path) == 0)
        return 0;
    return -errno;
}

static int posix_rename(void *context, const char *from, const char *to) {
    (void)context;
    if (rename(from, to) == 0)
        return 0;
    return -errno;
}

static int close_descriptor(int descriptor, int current_error) {
    if (descriptor < 0)
        return current_error;
    if (close(descriptor) < 0 && current_error == 0)
        return -errno;
    return current_error;
}

static int posix_copy(void *context, const char *from, const char *to) {
    unsigned char buffer[4096];
    int input = -1;
    int output = -1;
    int result = 0;
    (void)context;

    input = open(from, O_RDONLY);
    if (input < 0)
        return -errno;

    output = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (output < 0) {
        result = -errno;
        goto cleanup;
    }

    for (;;) {
        ssize_t read_size = read(input, buffer, sizeof(buffer));
        ssize_t offset = 0;

        if (read_size < 0) {
            result = -errno;
            break;
        }
        if (read_size == 0)
            break;

        while (offset < read_size) {
            ssize_t write_size = write(
                output,
                buffer + offset,
                (size_t)(read_size - offset)
            );
            if (write_size < 0) {
                result = -errno;
                break;
            }
            if (write_size == 0) {
                result = -EIO;
                break;
            }
            offset += write_size;
        }

        if (result < 0)
            break;
    }

    if (result == 0 && fsync(output) < 0)
        result = -errno;

cleanup:
    result = close_descriptor(output, result);
    result = close_descriptor(input, result);
    if (result < 0)
        unlink(to);
    return result;
}

static int posix_same_size(void *context, const char *first, const char *second) {
    struct stat first_status;
    struct stat second_status;
    (void)context;

    if (stat(first, &first_status) < 0)
        return -errno;
    if (stat(second, &second_status) < 0)
        return -errno;
    return first_status.st_size == second_status.st_size;
}

static ItlsFileOps posix_operations(void) {
    ItlsFileOps operations = {
        NULL,
        posix_exists,
        posix_remove,
        posix_rename,
        posix_copy,
        posix_same_size,
    };
    return operations;
}

static void make_path(
    char *output,
    size_t output_size,
    const char *directory,
    const char *name
) {
    int written = snprintf(output, output_size, "%s/%s", directory, name);
    assert(written >= 0);
    assert((size_t)written < output_size);
}

static void write_text(const char *path, const char *text) {
    int descriptor = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    size_t size = strlen(text);
    size_t offset = 0;

    assert(descriptor >= 0);
    while (offset < size) {
        ssize_t written = write(descriptor, text + offset, size - offset);
        assert(written > 0);
        offset += (size_t)written;
    }
    assert(close(descriptor) == 0);
}

static void assert_text(const char *path, const char *expected) {
    char buffer[256];
    int descriptor = open(path, O_RDONLY);
    ssize_t size;

    assert(descriptor >= 0);
    size = read(descriptor, buffer, sizeof(buffer) - 1);
    assert(size >= 0);
    buffer[size] = '\0';
    assert(close(descriptor) == 0);
    assert(strcmp(buffer, expected) == 0);
}

static void assert_absent(const char *path) {
    struct stat status;
    assert(stat(path, &status) < 0);
    assert(errno == ENOENT);
}

static void test_ca_install_update_restore(const char *directory) {
    char source[TEST_PATH_SIZE];
    char active[TEST_PATH_SIZE];
    char backup[TEST_PATH_SIZE];
    char temporary[TEST_PATH_SIZE];
    char rollback[TEST_PATH_SIZE];
    ItlsFileOps operations = posix_operations();
    ItlsReplaceResult result;

    make_path(source, sizeof(source), directory, "source.pem");
    make_path(active, sizeof(active), directory, "active.pem");
    make_path(backup, sizeof(backup), directory, "backup.pem");
    make_path(temporary, sizeof(temporary), directory, "temporary.pem");
    make_path(rollback, sizeof(rollback), directory, "rollback.pem");

    write_text(source, "updated-one");
    write_text(active, "original");

    result = itls_install_ca_bundle(
        &operations,
        source,
        active,
        backup,
        temporary,
        rollback
    );
    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert_text(active, "updated-one");
    assert_text(backup, "original");

    write_text(source, "updated-two");
    result = itls_install_ca_bundle(
        &operations,
        source,
        active,
        backup,
        temporary,
        rollback
    );
    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert_text(active, "updated-two");
    assert_text(backup, "original");

    result = itls_restore_ca_bundle(
        &operations,
        active,
        backup,
        temporary,
        rollback
    );
    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert_text(active, "original");
    assert_absent(backup);
    assert_absent(temporary);
    assert_absent(rollback);

    assert(unlink(source) == 0);
    assert(unlink(active) == 0);
}

static void test_interrupted_replacement_recovery(const char *directory) {
    char source[TEST_PATH_SIZE];
    char destination[TEST_PATH_SIZE];
    char temporary[TEST_PATH_SIZE];
    char rollback[TEST_PATH_SIZE];
    ItlsFileOps operations = posix_operations();
    ItlsReplaceResult result;

    make_path(source, sizeof(source), directory, "replace-source");
    make_path(destination, sizeof(destination), directory, "replace-destination");
    make_path(temporary, sizeof(temporary), directory, "replace-temporary");
    make_path(rollback, sizeof(rollback), directory, "replace-rollback");

    write_text(source, "new");
    write_text(destination, "old");
    write_text(temporary, "stale");
    assert(rename(destination, rollback) == 0);

    result = itls_transactional_replace(
        &operations,
        source,
        destination,
        temporary,
        rollback
    );
    assert(result.error == 0);
    assert(result.cleanup_error == 0);
    assert_text(destination, "new");
    assert_absent(temporary);
    assert_absent(rollback);

    assert(unlink(source) == 0);
    assert(unlink(destination) == 0);
}

int main(void) {
    char directory_template[] = "/tmp/itls-installer-test.XXXXXX";
    int descriptor = mkstemp(directory_template);
    const char *directory = directory_template;

    assert(descriptor >= 0);
    assert(close(descriptor) == 0);
    assert(unlink(directory_template) == 0);
    assert(mkdir(directory_template, 0700) == 0);
    test_ca_install_update_restore(directory);
    test_interrupted_replacement_recovery(directory);
    assert(rmdir(directory) == 0);
    puts("installer filesystem tests passed");
    return 0;
}
