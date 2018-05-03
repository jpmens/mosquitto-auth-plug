/*
 * Copyright (c) 2013 Jan-Piet Mens <jp@mens.de>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of mosquitto nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>
#include <mosquitto.h>
#include <mosquitto_plugin.h>
#include "log.h"

int log_quiet=0;

void (*_log)(int priority, const char *fmt, ...);

void log_init(void)
{
#if MOSQ_AUTH_PLUGIN_VERSION >= 3
	_log = __log;
#elif (LIBMOSQUITTO_MAJOR > 1) || ((LIBMOSQUITTO_MAJOR == 1) && (LIBMOSQUITTO_MINOR >= 4))
	_log = mosquitto_log_printf;
#else
	_log = __log;
#endif
}

void __log(int priority, const char *fmt, ...)
{
	va_list va;
	time_t now;

	if (log_quiet && priority <= LOG_DEBUG)
		return;

	time(&now);

	va_start(va, fmt);
	fprintf(stderr, "%ld: |-- ", now);
	vfprintf(stderr, fmt, va);
	fprintf(stderr, "\n");
	fflush(stderr);
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
	fflush(stderr);
	va_end(va);
	exit(1);
}
