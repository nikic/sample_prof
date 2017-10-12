#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_ini.h"
#include "ext/standard/info.h"
#include "php_sample_prof.h"
#include "zend_exceptions.h"

#include <signal.h>

/* We don't use SIGPROF because it interferes with set_time_limit(). */
#define SAMPLE_PROF_DEFAULT_SIGNUM SIGRTMIN

#define SAMPLE_PROF_DEFAULT_INTERVAL 100

/* On 64-bit this will give a 16 * 1MB allocation */
#define SAMPLE_PROF_DEFAULT_ALLOC (1 << 20)

ZEND_DECLARE_MODULE_GLOBALS(sample_prof)

static inline void sample_prof_remove_signal_handler(int signum) {
	struct sigaction sa;
	sa.sa_handler = SIG_DFL;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(signum, &sa, NULL);
}

static inline zend_bool sample_prof_end() {
	if (!SAMPLE_PROF_G->enabled) {
		return 0;
	}

	timer_delete(SAMPLE_PROF_G->timer_id);
	sample_prof_remove_signal_handler(SAMPLE_PROF_G->signum);

	SAMPLE_PROF_G->enabled = 0;
	return 1;
}

static void sample_prof_handler(int signum) {
	zend_sample_prof_globals *g = SAMPLE_PROF_G;
#ifdef ZEND_ENGINE_3
	zend_execute_data *ex = EG(current_execute_data);
	while (ex && !ZEND_USER_CODE(ex->func->type)) {
		ex = ex->prev_execute_data;
	}
	if (!ex) {
		return;
	}

	g->entries[g->entries_num].filename = ex->func->op_array.filename;
	g->entries[g->entries_num].lineno = ex->opline->lineno;
#else
	if (!EG(opline_ptr)) {
		return;
	}

	g->entries[g->entries_num].filename = EG(active_op_array)->filename;
	g->entries[g->entries_num].lineno = (*EG(opline_ptr))->lineno;
#endif

	if (++g->entries_num == g->entries_allocated) {
		/* Doing a realloc within a signal handler is unsafe, end profiling */
		sample_prof_end();
	}
}

static void sample_prof_start(long interval_usec, size_t num_entries_alloc, int signum) {
	struct sigaction sa;
	struct sigevent sev;
	struct itimerspec its;

	zend_sample_prof_globals *g = SAMPLE_PROF_G;

	/* Initialize data structures for entries */
	if (g->entries) {
		efree(g->entries);
	}

	g->signum = signum;
	g->entries_allocated = num_entries_alloc;
	g->entries_num = 0;
	g->entries = safe_emalloc(g->entries_allocated, sizeof(sample_prof_entry), 0);

	/* Register signal handler */
	sa.sa_handler = &sample_prof_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (-1 == sigaction(signum, &sa, NULL /* ignore old handler */)) {
		zend_throw_exception(NULL, "Could not register signal handler", 0 TSRMLS_CC);
		return;
	}

	/* Create timer */
	memset(&sev, 0, sizeof(struct sigevent));
	sev.sigev_notify = SIGEV_SIGNAL;
	sev.sigev_signo = signum;
	if (-1 == timer_create(CLOCK_REALTIME, &sev, &g->timer_id)) {
		sample_prof_remove_signal_handler(signum);
		zend_throw_exception(NULL, "Could not create timer", 0 TSRMLS_CC);
		return;
	}

	/* Arm timer */
	its.it_value.tv_sec = interval_usec / 1000000;
	its.it_value.tv_nsec = interval_usec * 1000;
	its.it_interval = its.it_value;
	if (-1 == timer_settime(g->timer_id, 0, &its, NULL /* ignore old timerspec */)) {
		timer_delete(g->timer_id);
		sample_prof_remove_signal_handler(signum);
		zend_throw_exception(NULL, "Could not arm timer", 0 TSRMLS_CC);
		return;
	}

	g->enabled = 1;
}

PHP_FUNCTION(sample_prof_start) {
	long interval_usec = 0;
	long num_entries_alloc = 0;
	long signum = 0;

	if (zend_parse_parameters(ZEND_NUM_ARGS() TSRMLS_CC, "|lll", &interval_usec, &num_entries_alloc, &signum) == FAILURE) {
		return;
	}

	if (interval_usec < 0) {
		zend_throw_exception(NULL, "Number of microseconds can't be negative", 0 TSRMLS_CC);
		return;
	} else if (interval_usec == 0) {
		interval_usec = SAMPLE_PROF_DEFAULT_INTERVAL;
	}

	if (num_entries_alloc < 0) {
		zend_throw_exception(NULL, "Number of profiling can't be negative", 0 TSRMLS_CC);
		return;
	} else if (num_entries_alloc == 0) {
		num_entries_alloc = SAMPLE_PROF_DEFAULT_ALLOC;
	}

	if (signum < 0) {
		zend_throw_exception(NULL, "Signal number can't be negative", 0 TSRMLS_CC);
		return;
	} else if (signum == 0) {
		signum = SAMPLE_PROF_DEFAULT_SIGNUM;
	}

	sample_prof_start(interval_usec, num_entries_alloc, signum);
}

PHP_FUNCTION(sample_prof_end) {
	if (zend_parse_parameters_none() == FAILURE) {
		return;
	}

	RETURN_BOOL(sample_prof_end());
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
#ifdef ZEND_ENGINE_3
		zend_string *filename = entry->filename;
		uint32_t lineno = entry->lineno;
		zval *lines, *num;

		lines = zend_hash_find(Z_ARR_P(return_value), filename);
		if (lines == NULL) {
			zval lines_zv;
			array_init(&lines_zv);
			lines = zend_hash_update(Z_ARR_P(return_value), filename, &lines_zv);
		}

		num = zend_hash_index_find(Z_ARR_P(lines), lineno);
		if (num == NULL) {
			zval num_zv;
			ZVAL_LONG(&num_zv, 0);
			num = zend_hash_index_update(Z_ARR_P(lines), lineno, &num_zv);
		}

		increment_function(num);
#else
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
#endif
	}
}

PHP_RINIT_FUNCTION(sample_prof)
{
	SAMPLE_PROF_G->enabled = 0;
	SAMPLE_PROF_G->entries = NULL;
	SAMPLE_PROF_G->entries_num = 0;

	return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(sample_prof)
{
	sample_prof_end();
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
	NULL,
	NULL,
	PHP_RINIT(sample_prof),	
	PHP_RSHUTDOWN(sample_prof),
	PHP_MINFO(sample_prof),
	PHP_SAMPLE_PROF_VERSION,
	STANDARD_MODULE_PROPERTIES
};

#ifdef COMPILE_DL_SAMPLE_PROF
ZEND_GET_MODULE(sample_prof)
#endif

