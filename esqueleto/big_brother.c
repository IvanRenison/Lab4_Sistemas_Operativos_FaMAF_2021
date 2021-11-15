#include "big_brother.h"

#include <gmodule.h>
#include <stdio.h>
#include <string.h>

char *censored_words[] = {
    "Oldspeak", "English", "revolution", "Emmanuel", "Goldstein", NULL,
};

int is_log_file_dentry(unsigned char *base_name, unsigned char *extension) {
    return strncmp(LOG_FILE_BASENAME, (char *)base_name, 3) == 0 &&
           strncmp(LOG_FILE_EXTENSION, (char *)extension, 3) == 0;
}

int is_log_filepath(char *filepath) {
    return strncmp(LOG_FILE, filepath, 8) == 0;
}

/* Checks if a needle is a  substring of the string haystack
 * Ignores capitalization
 */
static bool has_strcasestr(const char *haystack, const char *needle,
                           size_t haystack_length) {
    size_t needle_length = strlen(needle);
    for (size_t i = 0; i < haystack_length; i++) {
        if (i + needle_length > haystack_length) {
            return false;
        }
        if (strncasecmp(&haystack[i], needle, needle_length) == 0) {
            return true;
        }
    }
    return false;
}

GSList *censored_words_found(const char *buf, size_t size) {
    GSList *words_found = NULL;
    for (unsigned int i = 0; censored_words[i] != NULL; i++) {
        bool result = has_strcasestr(buf, censored_words[i], size);
        if (result) {
            words_found = g_slist_prepend(words_found, censored_words[i]);
        }
    }
    return words_found;
}
