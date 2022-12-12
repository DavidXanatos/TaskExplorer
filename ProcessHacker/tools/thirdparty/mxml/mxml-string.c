/*
 * String functions for Mini-XML, a small XML file parsing library.
 *
 * https://www.msweet.org/mxml
 *
 * Copyright © 2003-2019 by Michael R Sweet.
 *
 * Licensed under Apache License v2.0.  See the file "LICENSE" for more
 * information.
 */

/*
 * Include necessary headers...
 */

#include "config.h"


/*
 * The va_copy macro is part of C99, but many compilers don't implement it.
 * Provide a "direct assignment" implmentation when va_copy isn't defined...
 */

#ifndef va_copy
#  ifdef __va_copy
#    define va_copy(dst,src) __va_copy(dst,src)
#  else
#    define va_copy(dst,src) memcpy(&dst, &src, sizeof(va_list))
#  endif /* __va_copy */
#endif /* va_copy */


#ifndef HAVE_SNPRINTF
/*
 * '_mxml_snprintf()' - Format a string.
 */

int					/* O - Number of bytes formatted */
_mxml_snprintf(char       *buffer,	/* I - Output buffer */
               size_t     bufsize,	/* I - Size of output buffer */
           const char *format,	/* I - Printf-style format string */
           ...)			/* I - Additional arguments as needed */
{
  va_list	ap;			/* Argument list */
  int		bytes;			/* Number of bytes formatted */


  va_start(ap, format);
  bytes = vsnprintf(buffer, bufsize, format, ap);
  va_end(ap);

  return (bytes);
}
#endif /* !HAVE_SNPRINTF */


/*
 * '_mxml_strdup()' - Duplicate a string.
 */

#ifndef HAVE_STRDUP
char *					/* O - New string pointer */
_mxml_strdup(const char *s)		/* I - String to duplicate */
{
  char	*t;				/* New string pointer */


  if (s == NULL)
    return (NULL);

  if ((t = malloc(strlen(s) + 1)) == NULL)
    return (NULL);

  return (strcpy(t, s));
}
#endif /* !HAVE_STRDUP */


/*
 * '_mxml_strdupf()' - Format and duplicate a string.
 */

char *					/* O - New string pointer */
_mxml_strdupf(const char *format,	/* I - Printf-style format string */
              ...)			/* I - Additional arguments as needed */
{
  va_list	ap;			/* Pointer to additional arguments */
  char		*s;			/* Pointer to formatted string */


 /*
  * Get a pointer to the additional arguments, format the string,
  * and return it...
  */

  va_start(ap, format);
#ifdef HAVE_VASPRINTF
  if (vasprintf(&s, format, ap) < 0)
    s = NULL;
#else
  s = _mxml_vstrdupf(format, ap);
#endif /* HAVE_VASPRINTF */
  va_end(ap);

  return (s);
}


#ifndef HAVE_STRLCAT
/*
 * '_mxml_strlcat()' - Safely concatenate a string.
 */

size_t					/* O - Number of bytes copied */
_mxml_strlcat(char       *dst,		/* I - Destination buffer */
              const char *src,		/* I - Source string */
              size_t     dstsize)	/* I - Size of destination buffer */
{
  size_t	srclen;			/* Length of source string */
  size_t	dstlen;			/* Length of destination string */


 /*
  * Figure out how much room is left...
  */

  dstlen = strlen(dst);

  if (dstsize <= (dstlen + 1))
    return (dstlen);		        /* No room, return immediately... */

  dstsize -= dstlen + 1;

 /*
  * Figure out how much room is needed...
  */

  srclen = strlen(src);

 /*
  * Copy the appropriate amount...
  */

  if (srclen > dstsize)
    srclen = dstsize;

  memmove(dst + dstlen, src, srclen);
  dst[dstlen + srclen] = '\0';

  return (dstlen + srclen);
}
#endif /* !HAVE_STRLCAT */


#ifndef HAVE_STRLCPY
/*
 * '_mxml_strlcpy()' - Safely copy a string.
 */

size_t					/* O - Number of bytes copied */
_mxml_strlcpy(char       *dst,		/* I - Destination buffer */
              const char *src,		/* I - Source string */
              size_t     dstsize)	/* I - Size of destination buffer */
{
  size_t        srclen;                 /* Length of source string */


 /*
  * Figure out how much room is needed...
  */

  dstsize --;

  srclen = strlen(src);

 /*
  * Copy the appropriate amount...
  */

  if (srclen > dstsize)
    srclen = dstsize;

  memmove(dst, src, srclen);
  dst[srclen] = '\0';

  return (srclen);
}
#endif /* !HAVE_STRLCPY */


#ifndef HAVE_VSNPRINTF
/*
 * '_mxml_vsnprintf()' - Format a string into a fixed size buffer.
 */

int					/* O - Number of bytes formatted */
_mxml_vsnprintf(char       *buffer,	/* O - Output buffer */
                size_t     bufsize,	/* O - Size of output buffer */
        const char *format,	/* I - Printf-style format string */
        va_list    ap)		/* I - Pointer to additional arguments */
{
  char		*bufptr,		/* Pointer to position in buffer */
        *bufend,		/* Pointer to end of buffer */
        sign,			/* Sign of format width */
        size,			/* Size character (h, l, L) */
        type;			/* Format type character */
  int		width,			/* Width of field */
        prec;			/* Number of characters of precision */
  char		tformat[100],		/* Temporary format string for sprintf() */
        *tptr,			/* Pointer into temporary format */
        temp[1024];		/* Buffer for formatted numbers */
  char		*s;			/* Pointer to string */
  int		slen;			/* Length of string */
  int		bytes;			/* Total number of bytes needed */


 /*
  * Loop through the format string, formatting as needed...
  */

  bufptr = buffer;
  bufend = buffer + bufsize - 1;
  bytes  = 0;

  while (*format)
  {
    if (*format == '%')
    {
      tptr = tformat;
      *tptr++ = *format++;

      if (*format == '%')
      {
        if (bufptr && bufptr < bufend)
          *bufptr++ = *format;
        bytes ++;
        format ++;
    continue;
      }
      else if (strchr(" -+#\'", *format))
      {
        *tptr++ = *format;
        sign = *format++;
      }
      else
        sign = 0;

      if (*format == '*')
      {
       /*
        * Get width from argument...
    */

    format ++;
    width = va_arg(ap, int);

    snprintf(tptr, sizeof(tformat) - (tptr - tformat), "%d", width);
    tptr += strlen(tptr);
      }
      else
      {
    width = 0;

    while (isdigit(*format & 255))
    {
      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      width = width * 10 + *format++ - '0';
    }
      }

      if (*format == '.')
      {
    if (tptr < (tformat + sizeof(tformat) - 1))
      *tptr++ = *format;

        format ++;

        if (*format == '*')
    {
         /*
      * Get precision from argument...
      */

      format ++;
      prec = va_arg(ap, int);

      snprintf(tptr, sizeof(tformat) - (tptr - tformat), "%d", prec);
      tptr += strlen(tptr);
    }
    else
    {
      prec = 0;

      while (isdigit(*format & 255))
      {
        if (tptr < (tformat + sizeof(tformat) - 1))
          *tptr++ = *format;

        prec = prec * 10 + *format++ - '0';
      }
    }
      }
      else
        prec = -1;

      if (*format == 'l' && format[1] == 'l')
      {
        size = 'L';

    if (tptr < (tformat + sizeof(tformat) - 2))
    {
      *tptr++ = 'l';
      *tptr++ = 'l';
    }

    format += 2;
      }
      else if (*format == 'h' || *format == 'l' || *format == 'L')
      {
    if (tptr < (tformat + sizeof(tformat) - 1))
      *tptr++ = *format;

        size = *format++;
      }

      if (!*format)
        break;

      if (tptr < (tformat + sizeof(tformat) - 1))
        *tptr++ = *format;

      type  = *format++;
      *tptr = '\0';

      switch (type)
      {
    case 'E' : /* Floating point formats */
    case 'G' :
    case 'e' :
    case 'f' :
    case 'g' :
        if ((width + 2) > sizeof(temp))
          break;

        sprintf(temp, tformat, va_arg(ap, double));

            bytes += strlen(temp);

            if (bufptr)
        {
          if ((bufptr + strlen(temp)) > bufend)
          {
        strncpy(bufptr, temp, (size_t)(bufend - bufptr));
        bufptr = bufend;
          }
          else
          {
        strcpy(bufptr, temp);
        bufptr += strlen(temp);
          }
        }
        break;

        case 'B' : /* Integer formats */
    case 'X' :
    case 'b' :
        case 'd' :
    case 'i' :
    case 'o' :
    case 'u' :
    case 'x' :
        if ((width + 2) > sizeof(temp))
          break;

#ifdef HAVE_LONG_LONG_INT
        if (size == 'L')
          sprintf(temp, tformat, va_arg(ap, long long));
        else
#endif /* HAVE_LONG_LONG_INT */
        sprintf(temp, tformat, va_arg(ap, int));

            bytes += strlen(temp);

        if (bufptr)
        {
          if ((bufptr + strlen(temp)) > bufend)
          {
        strncpy(bufptr, temp, (size_t)(bufend - bufptr));
        bufptr = bufend;
          }
          else
          {
        strcpy(bufptr, temp);
        bufptr += strlen(temp);
          }
        }
        break;

    case 'p' : /* Pointer value */
        if ((width + 2) > sizeof(temp))
          break;

        sprintf(temp, tformat, va_arg(ap, void *));

            bytes += strlen(temp);

        if (bufptr)
        {
          if ((bufptr + strlen(temp)) > bufend)
          {
        strncpy(bufptr, temp, (size_t)(bufend - bufptr));
        bufptr = bufend;
          }
          else
          {
        strcpy(bufptr, temp);
        bufptr += strlen(temp);
          }
        }
        break;

        case 'c' : /* Character or character array */
        bytes += width;

        if (bufptr)
        {
          if (width <= 1)
            *bufptr++ = va_arg(ap, int);
          else
          {
        if ((bufptr + width) > bufend)
          width = bufend - bufptr;

        memcpy(bufptr, va_arg(ap, char *), (size_t)width);
        bufptr += width;
          }
        }
        break;

    case 's' : /* String */
        if ((s = va_arg(ap, char *)) == NULL)
          s = "(null)";

        slen = strlen(s);
        if (slen > width && prec != width)
          width = slen;

            bytes += width;

        if (bufptr)
        {
          if ((bufptr + width) > bufend)
            width = bufend - bufptr;

              if (slen > width)
            slen = width;

          if (sign == '-')
          {
        strncpy(bufptr, s, (size_t)slen);
        memset(bufptr + slen, ' ', (size_t)(width - slen));
          }
          else
          {
        memset(bufptr, ' ', (size_t)(width - slen));
        strncpy(bufptr + width - slen, s, (size_t)slen);
          }

          bufptr += width;
        }
        break;

    case 'n' : /* Output number of chars so far */
        *(va_arg(ap, int *)) = bytes;
        break;
      }
    }
    else
    {
      bytes ++;

      if (bufptr && bufptr < bufend)
        *bufptr++ = *format;

      format ++;
    }
  }

 /*
  * Nul-terminate the string and return the number of characters needed.
  */

  *bufptr = '\0';

  return (bytes);
}
#endif /* !HAVE_VSNPRINTF */


/*
 * '_mxml_vstrdupf()' - Format and duplicate a string.
 */

char *					/* O - New string pointer */
_mxml_vstrdupf(const char *format,	/* I - Printf-style format string */
               va_list    ap)		/* I - Pointer to additional arguments */
{
#ifdef HAVE_VASPRINTF
  char		*s;			/* String */

  if (vasprintf(&s, format, ap) < 0)
    s = NULL;

  return (s);

#else
  int		bytes;			/* Number of bytes required */
  char		*buffer;		/* String buffer */
#  ifndef _WIN32
  char		temp[256];		/* Small buffer for first vsnprintf */
#  endif /* !_WIN32 */


 /*
  * First format with a tiny buffer; this will tell us how many bytes are
  * needed...
  */

#  ifdef _WIN32
  bytes = _vscprintf(format, ap);

#  else
  va_list	apcopy;			/* Copy of argument list */

  va_copy(apcopy, ap);
  if ((bytes = vsnprintf(temp, sizeof(temp), format, apcopy)) < sizeof(temp))
  {
   /*
    * Hey, the formatted string fits in the tiny buffer, so just dup that...
    */

    return (strdup(temp));
  }
#  endif /* _WIN32 */

 /*
  * Allocate memory for the whole thing and reformat to the new buffer...
  */

  if ((buffer = calloc(1, bytes + 1)) != NULL)
    vsnprintf(buffer, bytes + 1, format, ap);

 /*
  * Return the new string...
  */

  return (buffer);
#endif /* HAVE_VASPRINTF */
}
