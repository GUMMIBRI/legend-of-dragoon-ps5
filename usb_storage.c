#define _POSIX_C_SOURCE 200809L

#include "usb_storage.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

static const int directory_flags = O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC;

extern void pe_application_stage(const char *message);

static void log_usb_failure(const char *operation)
{
    int error = errno;
    char message[160];
    snprintf(message, sizeof message, "Dragoon USB: %s failed (errno %d)\n",
             operation, error);
    pe_application_stage(message);
    errno = error;
}

int dragoon_usb_open_root(void)
{
    /* ShadowMount exposes the USB volume inside the application sandbox. */
    int mount = openat(AT_FDCWD, "/mnt/usb0", directory_flags);
    int homebrew = -1;
    int title = -1;
    int root = -1;
    const char *operation = "open /mnt/usb0";
    if (mount >= 0) {
        struct stat mount_stat;
        struct stat root_stat;

        operation = "stat USB mount";
        if (fstat(mount, &mount_stat) < 0)
            goto done;

        operation = "open homebrew";
        homebrew = openat(mount, "homebrew", directory_flags);
        if (homebrew < 0)
            goto done;
        operation = "open LOD-test";
        title = openat(homebrew, "LOD-test", directory_flags);
        if (title < 0)
            goto done;
        operation = "open lod";
        root = openat(title, "lod", directory_flags);
        if (root < 0)
            goto done;
        operation = "validate lod device";
        if (fstat(root, &root_stat) < 0) {
            int error = errno;
            close(root);
            root = -1;
            errno = error;
        } else if (root_stat.st_dev != mount_stat.st_dev) {
            close(root);
            root = -1;
            errno = EXDEV;
        }
    }

done:
    {
        if (root < 0)
            log_usb_failure(operation);
        int error = errno;
        if (title >= 0)
            close(title);
        if (homebrew >= 0)
            close(homebrew);
        if (mount >= 0)
            close(mount);
        errno = error;
    }
    return root;
}

int dragoon_usb_enter(void)
{
    static const char *const directories[] = {
        "disc", "bios", "saves", "logs", "tmp", "cache", "textures", "backgrounds"
    };
    int root = dragoon_usb_open_root();
    if (root < 0)
        return -1;

    int result = -1;
    const char *operation = "stat lod";
    struct stat root_stat;
    if (fstat(root, &root_stat) < 0)
        goto done;

    for (size_t i = 0; i < sizeof directories / sizeof directories[0]; ++i) {
        operation = directories[i];
        int fd = openat(root, directories[i], directory_flags);
        if (fd < 0)
            goto done;
        struct stat directory_stat;
        int status = fstat(fd, &directory_stat);
        int error = errno;
        close(fd);
        if (status < 0) {
            errno = error;
            goto done;
        }
        if (directory_stat.st_dev != root_stat.st_dev) {
            errno = EXDEV;
            goto done;
        }
    }

    operation = "set USB environment";
    if (
        setenv("HOME", DRAGOON_DATA_ROOT, 1) < 0 ||
        setenv("XDG_DATA_HOME", DRAGOON_DATA_ROOT, 1) < 0 ||
        setenv("XDG_CONFIG_HOME", DRAGOON_DATA_ROOT, 1) < 0 ||
        setenv("TMPDIR", DRAGOON_DATA_ROOT "/tmp", 1) < 0 ||
        setenv("XDG_CACHE_HOME", DRAGOON_DATA_ROOT "/cache", 1) < 0 ||
        setenv("MESA_SHADER_CACHE_DIR", DRAGOON_DATA_ROOT "/cache", 1) < 0 ||
        setenv("PSX_RESIDENT_CACHE", DRAGOON_DATA_ROOT "/cache", 1) < 0 ||
        setenv("PSXRECOMP_HD_TEXTURE_ROOT", DRAGOON_DATA_ROOT "/textures", 1) < 0)
        goto done;
    result = 0;

done:
    {
        if (result < 0)
            log_usb_failure(operation);
        int error = errno;
        close(root);
        errno = error;
    }
    return result;
}
