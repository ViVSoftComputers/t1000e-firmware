/* Minimal SES retarget stubs for the SEGGER RTL libc.
 * Provides stdio symbols that the libc's fileops.o needs,
 * without pulling in UART or full retarget infrastructure.
 *
 * IMPORTANT: We deliberately do NOT include <stdio.h> here because
 * the SES libc defines stdout/stderr/stdin as macros that expand
 * to &__stdout etc., which would conflict with providing the
 * actual symbol that fileops.o references. */

typedef struct { int dummy; } FILE;

/* fileops.o references these as bare symbols */
FILE stdout;
FILE stderr;
FILE stdin;

int __SEGGER_RTL_X_file_write(void *opaque, const char *buf, int len, int *written)
{
    (void)opaque; (void)buf; (void)len;
    if(written) *written = 0;
    return 0;
}

int __SEGGER_RTL_X_file_read(void *opaque, char *buf, int len, int *rd)
{
    (void)opaque; (void)buf; (void)len;
    if(rd) *rd = 0;
    return -1;
}

int __SEGGER_RTL_X_file_stat(void *opaque, const char *name, void *statbuf)
{
    (void)opaque; (void)name; (void)statbuf;
    return -1;
}

int __SEGGER_RTL_X_file_seek(void *opaque, int offset, int whence, int *pos)
{
    (void)opaque; (void)offset; (void)whence;
    if(pos) *pos = 0;
    return -1;
}

void __SEGGER_RTL_X_file_close(void *opaque)
{
    (void)opaque;
}

int __SEGGER_RTL_X_file_open(const char *name, int mode, void **opaque)
{
    (void)name; (void)mode;
    *opaque = 0;
    return -1;
}
