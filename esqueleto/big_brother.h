#ifndef _BIG_BROTHER_H
#define _BIG_BROTHER_H

#include "fat_filename_util.h"
#include <gmodule.h>

#define LOG_FILEPATH       PATH_SEPARATOR LOG_FILE
#define LOG_FILE           LOG_FILE_BASENAME "." LOG_FILE_EXTENSION
#define LOG_FILE_BASENAME  "fs"
#define LOG_FILE_EXTENSION "log"

extern char *censored_words[];

/* Returns a list with the censured words funded in @buf
 * The returned list is owned by the caller but not the references.
 */
GSList *censored_words_found(const char *buf, size_t size);

int is_log_file_dentry(unsigned char *base_name, unsigned char *extension);

int is_log_filepath(char *filepath);

#endif