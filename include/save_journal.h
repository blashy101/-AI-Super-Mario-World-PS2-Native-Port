#ifndef SMW_SAVE_JOURNAL_H
#define SMW_SAVE_JOURNAL_H
#include <stdbool.h>
#include <stddef.h>
/* Alternating complete records avoid relying on memory-card rename support. */
bool smw_journal_load(const char *base,void *data,size_t size);
bool smw_journal_save(const char *base,const void *data,size_t size);
#endif
