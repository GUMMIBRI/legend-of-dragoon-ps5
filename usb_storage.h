#ifndef DRAGOON_USB_STORAGE_H
#define DRAGOON_USB_STORAGE_H

#define DRAGOON_APP_ROOT "/mnt/usb0/homebrew/LOD-test"
#define DRAGOON_DATA_ROOT DRAGOON_APP_ROOT "/lod"
#define DRAGOON_LOG_ROOT DRAGOON_DATA_ROOT "/logs"

/* Call before initializing any subsystem that reads or writes game files.
 * Returns -1 with errno on failure. The caller must abort startup.
 * The successful descriptor is owned by the caller.
 */
int dragoon_usb_open_root(void);

/* Enter the USB data root and configure asset and temporary directories.
 * Invoke before starting the game runtime or any worker threads.
 */
int dragoon_usb_enter(void);

#endif
