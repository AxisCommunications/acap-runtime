/* Copyright 2020 Axis Communications AB. All Rights Reserved.
==============================================================================*/
#include <stdio.h>
#include <unistd.h>
#include <sys/resource.h>

/*
 * Measures the current resident and virtual memories
 * usage of your linux C process, in kB
 */
int memory_use()
{
   long rss = 0L;
    FILE* fp = NULL;
    if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
        return (size_t)0L;      /* Can't open? */
    if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
    {
        fclose( fp );
        return (size_t)0L;      /* Can't read? */
    }
    fclose( fp );
    return (size_t)rss * (size_t)sysconf( _SC_PAGESIZE) / 1024;
}