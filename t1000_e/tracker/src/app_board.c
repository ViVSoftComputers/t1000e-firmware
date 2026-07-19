
#include "app_board.h"

/* SES RTL retarget stubs — only needed for SEGGER Embedded Studio builds.
 * GCC builds use -specs=nano.specs which provides its own stubs. */
#ifdef __SEGGER_CC
#include <stdio.h>

FILE *stdout = 0;
FILE *stderr = 0;
FILE *stdin  = 0;

int __SEGGER_RTL_X_file_write(__SEGGER_RTL_FILE *stream, const char *s, unsigned len) { (void)stream;(void)s;(void)len; return 0; }
int __SEGGER_RTL_X_file_read(__SEGGER_RTL_FILE *stream, char *s, unsigned len) { (void)stream;(void)s;(void)len; return 0; }
int __SEGGER_RTL_X_file_stat(__SEGGER_RTL_FILE *stream) { (void)stream; return -1; }
int __SEGGER_RTL_X_file_seek(__SEGGER_RTL_FILE *stream, long offset, int whence) { (void)stream;(void)offset;(void)whence; return -1; }
__SEGGER_RTL_FILE *__SEGGER_RTL_X_file_open(const char *filename, const char *mode) { (void)filename;(void)mode; return 0; }
int __SEGGER_RTL_X_file_close(__SEGGER_RTL_FILE *stream) { (void)stream; return 0; }
int __SEGGER_RTL_X_file_flush(__SEGGER_RTL_FILE *stream) { (void)stream; return 0; }
int __SEGGER_RTL_X_file_error(__SEGGER_RTL_FILE *stream) { (void)stream; return 0; }
int __SEGGER_RTL_X_file_eof(__SEGGER_RTL_FILE *stream) { (void)stream; return 1; }
void __SEGGER_RTL_X_file_clrerr(__SEGGER_RTL_FILE *stream) { (void)stream; }
#endif

BoardVersion_t BoardVersion = {
    .Fields.SwMajor = 1,
    .Fields.SwMinor = 2,

    .Fields.HwMajor = 1,
    .Fields.HwMinor = 1,
};

BoardVersion_t smtc_board_version_get( void )
{
    return BoardVersion;
}
