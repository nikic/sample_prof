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
	const char *filename;
	zend_uint lineno;
} sample_prof_entry;

ZEND_BEGIN_MODULE_GLOBALS(sample_prof)
	zend_bool enabled;
	timer_t timer_id;
	sample_prof_entry *entries;
	size_t entries_num;
	size_t entries_allocated;
ZEND_END_MODULE_GLOBALS(sample_prof)

#ifdef ZTS
#define SAMPLE_PROF_G ((zend_sample_prof_globals *) (*(void ***) tsrm_ls)[TSRM_UNSHUFFLE_RSRC_ID(id)])
#else
#define SAMPLE_PROF_G (&sample_prof_globals)
#endif

#endif

/*
 * Local variables:
 * tab-width: 4
 * c-basic-offset: 4
 * End:
 * vim600: noet sw=4 ts=4 fdm=marker
 * vim<600: noet sw=4 ts=4
 */

