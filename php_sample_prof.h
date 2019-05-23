#ifndef PHP_SAMPLE_PROF_H
#define PHP_SAMPLE_PROF_H

extern zend_module_entry sample_prof_module_entry;
#define phpext_sample_prof_ptr &sample_prof_module_entry

#define PHP_SAMPLE_PROF_VERSION "0.1.0"

#ifdef PHP_WIN32
#	define PHP_SAMPLE_PROF_API __declspec(dllexport)
#elif defined(__GNUC__) && __GNUC__ >= 4
#	define PHP_SAMPLE_PROF_API __attribute__ ((visibility("default")))
#else
#	define PHP_SAMPLE_PROF_API
#endif

#ifdef ZTS
#include "TSRM.h"
#endif

typedef struct _sample_prof_entry {
	zend_string *filename;
	uint32_t lineno;
} sample_prof_entry;

ZEND_BEGIN_MODULE_GLOBALS(sample_prof)
	zend_bool enabled;
	long interval_usec;
	pthread_t thread_id;
	sample_prof_entry *entries;
	size_t entries_num;
	size_t entries_allocated;
ZEND_END_MODULE_GLOBALS(sample_prof)

#define SAMPLE_PROF_G ZEND_MODULE_GLOBALS_BULK(sample_prof)

#endif

