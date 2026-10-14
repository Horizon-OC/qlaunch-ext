/* qlaunch-ext miniINI glue (C) 2026 Souldbminer */
/* Licensed under the GPLv2                */
/* Maps miniINI file I/O onto libc stdio. miniINI itself
 * (minIni.h/minIni.c) is upstream CompuPhase code, vendored
 * unmodified; see LICENSE/NOTICE beside it. */

#pragma once
#include <stdio.h>

#define INI_FILETYPE                  FILE*
#define ini_openread(filename,file)   ((*(file) = fopen((filename), "r")) != NULL)
#define ini_openwrite(filename,file)  ((*(file) = fopen((filename), "w")) != NULL)
#define ini_close(file)               (fclose(*(file)) == 0)
#define ini_read(buffer,size,file)    (fgets((buffer),(size),*(file)) != NULL)
#define ini_write(buffer,file)        (fputs((buffer),*(file)) >= 0)
#define ini_rename(source,dest)       (rename((source), (dest)) == 0)
#define ini_remove(filename)          (remove(filename) == 0)

#define INI_FILEPOS                   fpos_t
#define ini_tell(file,pos)            (fgetpos(*(file), (pos)) == 0)
#define ini_seek(file,pos)            (fsetpos(*(file), (pos)) == 0)

/* Longest INI line we accept; must cover the largest value
   (512-char NRO paths) plus key and overhead. */
#define INI_BUFFERSIZE                1024
#define INI_LINETERM                  "\n"
