#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "log.h"


void _log(int priority, const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "|-- ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	va_end(va);
}	

void _fatal(const char *fmt, ...)
{
	va_list va;

	va_start(va, fmt);
	fprintf(stderr, "|-- ");
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	fprintf(stderr, "|-- *** ABORT.\n");
	va_end(va);
	exit(1);
}	
