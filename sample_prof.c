#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_sample_prof.h"

#include <signal.h>

#define SAMPLE_PROF_INITIAL_ALLOC 100000

ZEND_DECLARE_MODULE_GLOBALS(sample_prof)

static void sample_prof_handler(int signal) {
	zend_sample_prof_globals *g = SAMPLE_PROF_G;

	if (!EG(opline_ptr)) {
		return;
	}

	g->entries[g->entries_num].filename = EG(active_op_array)->filename;
	g->entries[g->entries_num].lineno = (*EG(opline_ptr))->lineno;

	if (++g->entries_num == g->entries_allocated) {
		g->entries_allocated *= 2;
		g->entries = erealloc(g->entries, g->entries_allocated);
	}
}

static void sample_prof_start(long msec) {
	struct sigaction sa;
	struct itimerval timer;
	zend_sample_prof_globals *g = SAMPLE_PROF_G;

	sa.sa_handler = &sample_prof_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;

	timer.it_interval.tv_sec = 0;
	timer.it_interval.tv_usec = msec;
	timer.it_value = timer.it_interval;

	/* NULL = ignore old handler/timer */
	sigaction(SIGPROF, &sa, NULL);
	setitimer(ITIMER_PROF, &timer, NULL);

	if (g->entries) {
		efree(g->entries);
	}
	g->entries_allocated = SAMPLE_PROF_INITIAL_ALLOC;
	g->entries_num = 0;
	g->entries = safe_emalloc(g->entries_allocated, sizeof(sample_prof_entry), 0);
}

static void sample_prof_end() {
	struct itimerval timer;
	timer.it_interval.tv_sec = timer.it_interval.tv_usec = 0;
	timer.it_value = timer.it_interval;
	setitimer(ITIMER_PROF, &timer, NULL);
}

PHP_FUNCTION(sample_prof_start) {
	long msec;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "l", &msec) == FAILURE) {
		return;
	}

	sample_prof_start(msec);
}

PHP_FUNCTION(sample_prof_end) {
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	sample_prof_end();
}

PHP_FUNCTION(sample_prof_get_data) {
	zend_sample_prof_globals *g = SAMPLE_PROF_G;
	size_t entry_num;

	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	array_init(return_value);

	for (entry_num = 0; entry_num < g->entries_num; ++entry_num) {
		sample_prof_entry *entry = &g->entries[entry_num];
		const char *filename = entry->filename;
		zend_uint lineno = entry->lineno;

		zval **lines;
		zval **num;

		if (zend_hash_find(Z_ARRVAL_P(return_value), filename, strlen(filename)+1, (void **) &lines) == FAILURE) {
			zval *lines_zv;
			MAKE_STD_ZVAL(lines_zv);
			array_init(lines_zv);

			zend_hash_update(Z_ARRVAL_P(return_value), filename, strlen(filename)+1, (void *) &lines_zv, sizeof(zval *), (void **) &lines);
		}

		if (zend_hash_index_find(Z_ARRVAL_PP(lines), lineno, (void **) &num) == FAILURE) {
			zval *num_zv;
			MAKE_STD_ZVAL(num_zv);
			ZVAL_LONG(num_zv, 0);

			zend_hash_index_update(Z_ARRVAL_PP(lines), lineno, (void *) &num_zv, sizeof(zval *), (void **) &num);
		}

		increment_function(*num);
	}
}

PHP_MINIT_FUNCTION(sample_prof)
{
	return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(sample_prof)
{
	return SUCCESS;
}

PHP_RINIT_FUNCTION(sample_prof)
{
	SAMPLE_PROF_G->entries = NULL;
	SAMPLE_PROF_G->entries_num = 0;
	SAMPLE_PROF_G->entries_allocated = 0;

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(sample_prof)
{
	if (SAMPLE_PROF_G->entries) {
		efree(SAMPLE_PROF_G->entries);
	}

	return SUCCESS;
}

PHP_MINFO_FUNCTION(sample_prof)
{
	php_info_print_table_start();
	php_info_print_table_header(2, "sample_prof support", "enabled");
	php_info_print_table_end();
}

const zend_function_entry sample_prof_functions[] = {
	PHP_FE(sample_prof_start, NULL)
	PHP_FE(sample_prof_end, NULL)
	PHP_FE(sample_prof_get_data, NULL)
	PHP_FE_END
};

zend_module_entry sample_prof_module_entry = {
	STANDARD_MODULE_HEADER,
	"sample_prof",
	sample_prof_functions,
	PHP_MINIT(sample_prof),
	PHP_MSHUTDOWN(sample_prof),
	PHP_RINIT(sample_prof),	
	PHP_RSHUTDOWN(sample_prof),
	PHP_MINFO(sample_prof),
	PHP_SAMPLE_PROF_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SAMPLE_PROF
ZEND_GET_MODULE(sample_prof)
#endif

