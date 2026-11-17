#include <fcntl.h>
#include <stddef.h>

extern int sceKernelOpen(const char *, int, unsigned);
extern long sceKernelWrite(int, const void *, size_t);
extern int sceKernelClose(int);
extern void ps5_fp_ieee(void);

/* Application CRT diagnostics run before libc and TLS initialization. */
void pe_application_stage(const char *message)
{
    static int initialized;
    if (!initialized) {
        ps5_fp_ieee();
        initialized = 1;
    }

    int fd = sceKernelOpen("/mnt/usb0/homebrew/LOD-test/startup.log",
                           O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (fd < 0)
        return;

    size_t length = 0;
    while (message[length])
        ++length;
    while (length) {
        long written = sceKernelWrite(fd, message, length);
        if (written <= 0 || (size_t)written > length)
            break;
        message += written;
        length -= (size_t)written;
    }
    sceKernelClose(fd);
}
