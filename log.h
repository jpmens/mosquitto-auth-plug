
#define LOG_DEBUG (1)
#define LOG_NOTICE (2)

void _log(int priority, const char *fmt, ...);
void _fatal(const char *fmt, ...);
