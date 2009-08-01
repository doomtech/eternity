#ifndef PSNPRINTF_H
#define PSNPRINTF_H

#include <stdlib.h>
#include <stdarg.h>

int psnprintf(char *str, size_t n, const char *format, ...);
int pvsnprintf(char *str, size_t n, const char *format, va_list ap);

/* haleyjd 08/01/09: rewritten to use a structure to get around bizarre
 * GCC bug with va_list * type.
 */
typedef struct psvnfmt_vars_s
{
   char       *pinsertion;
   size_t      nmax;
   const char *fmt;
   int         flags;
   int         width;
   int         precision;
   char        prefix;
   va_list     ap;
} pvsnfmt_vars;

/* Use these directly if you want to avoid overhead of psnprintf
 * Return value is number of characters printed (or number printed
 * if there had been enough room).
 */
int pvsnfmt_char(pvsnfmt_vars *args);

int pvsnfmt_int(pvsnfmt_vars *args);

int pvsnfmt_str(pvsnfmt_vars *args);

int pvsnfmt_double(pvsnfmt_vars *args);

/* These are the flags you need (use logical OR) for the flags parameter of
 * fmt functions above.
 */
#define FLAG_DEFAULT         0x00
#define FLAG_LEFT_ALIGN      0x01 // -
#define FLAG_SIGNED          0x02 // +
#define FLAG_ZERO_PAD        0x04 // 0
#define FLAG_SIGN_PAD        0x08 // ' '
#define FLAG_HASH            0x10 // #

/* Portable strnlen function (doesn't exist on all systems!) */
size_t pstrnlen(const char *s, size_t count);


#endif /* ifdef PSNPRINTF_H */


