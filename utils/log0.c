/*
 * Local logging helpers used by temporary instrumentation.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdbool.h>

#include "utils/log0.h"

// Single process-wide log file handle used by instrumentation helpers.
static FILE *log_file = NULL;
static bool log_enabled = false;

static void init_log_file(void)
{
	if (log_file != NULL) {
		return;
	}

#ifdef nsmui
	log_file = fopen("log-mui.txt", "a");
#else
#warning compiling sdl version, using log-art.txt
	log_file = fopen("log-art.txt", "a");
#endif

	if (log_file == NULL) {
		log_file = stderr;
	}
}

void close_log_file(void)
{
	if (log_file != NULL && log_file != stderr) {
		fclose(log_file);
	}
	log_file = NULL;
}

void write_to_log(const char *format, ...)
{
	va_list args;

	if (!log_enabled) {
		return;
	}

	init_log_file();

	va_start(args, format);
	vfprintf(log_file, format, args);
	va_end(args);

	fflush(log_file);
}

static void __attribute__((destructor)) cleanup_log(void)
{
	close_log_file();
}

void set_log0_enabled(bool enabled)
{
	log_enabled = enabled;
	if (!log_enabled) {
		close_log_file();
	}
}
