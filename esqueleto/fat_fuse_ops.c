/*
 * fat_fuse_ops.c
 *
 * FAT32 filesystem operations for FUSE (Filesystem in Userspace)
 */

#include "fat_fuse_ops.h"

#include "fat_file.h"
#include "fat_filename_util.h"
#include "fat_fs_tree.h"
#include "fat_util.h"
#include "fat_volume.h"
#include <alloca.h>
#include <assert.h>
#include <errno.h>
#include <gmodule.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

/* Retrieve the currently mounted FAT volume from the FUSE context. */
static inline fat_volume get_fat_volume() {
    return fuse_get_context()->private_data;
}

#define LOG_MESSAGE_SIZE 100
#define DATE_MESSAGE_SIZE 30

static int fat_fuse_mknod(const char *path, mode_t mode, dev_t dev);

/* Create log file.
 *
 * Must not be executed if log already exists.
 */
static void fat_fuse_log_create(void) {
    fat_fuse_mknod("/fs.log", 0, 0); // 0, 0 are ignored
    /*fat_volume vol = get_fat_volume();
    fat_tree_node log_node = fat_tree_node_search(vol->file_tree, "/fs.log");
    fat_file log_file = fat_tree_get_file(log_node);
    log_file->dentry->base_name[0] = FAT_FILENAME_DELETED_CHAR;
    log_file->dentry->attribs = FILE_ATTRIBUTE_SYSTEM;  */
}

/* Writes @text to the log file.
 *
 * If the log file doesn't exists, it's created.
 *
 * PRE: text != NULL
 */
static void fat_fuse_log_write(char *text) {
    assert(text != NULL);

    fat_volume vol = get_fat_volume();
    fat_tree_node log_node = fat_tree_node_search(vol->file_tree, "/fs.log");
    fat_file log_file = fat_tree_get_file(log_node);
    fat_file parent = fat_tree_get_parent(log_node);
    fat_file_pwrite(log_file, text, strlen(text), log_file->dentry->file_size,
                    parent);
}

/* Creates a string with the current date and time.
 * The string is allocates, and must be freed by the caller.
 * In case of memory allocation error NULL is returned
 */
static char *now_to_str(void) {
    char *text = malloc(DATE_MESSAGE_SIZE);
    if (text == NULL) {
        return text;
    }

    time_t now = time(NULL);
    struct tm *timeinfo;
    timeinfo = localtime(&now);

    strftime(text, DATE_MESSAGE_SIZE, "%d-%m-%Y %H:%M", timeinfo);
    return (text);
}

/* Concatenates 2 strings re-allocating the first one to add the necessary
 * space. s1 has to be a dinamic string (declared with malloc), but s2 can
 * be static.
 *
 * s2 must not be NULL, but s1 can be NULL, in that case nothing is done
 * and NULL is returned. This allows us to use str_concat several times and
 * only check if there's an error at the end of everything, saving us a lot
 * of `if`.
 *
 * In case of memory allocation error NULL is returned and s1 is freed
 * (but not s2).
 *
 * USAGE:
 *     s1 = str_concat(s1, s2);
 *     s1 = str_concat(s1, "string");
 *
 * REQUIRES:
 *     s2 != NULL
 */
static char *str_concat(char *s1, const char *s2) {
    assert(s2 != NULL);

    if (s1 != NULL) {
        size_t s1_len = strlen(s1);
        size_t s2_len = strlen(s2);
        s1 = reallocarray(s1, s1_len + s2_len + 1, sizeof(char));
        // The + 1 is for the '\0'
        if (s1 != NULL) {
            // Case that reallocarray doesn't fail
            s1 = strcat(s1, s2);
        } else {
            // Case that reallocarray fails
            free(s1);
            s1 = NULL;
        }
    }
    return (s1);
}

/* Creates the string with the message that will be logged into fs.log
 * The returned pointer is owned by the caller, and must be freed.
 *
 * In case of memory allocation error NULL is returned.
 */
static char *fat_fuse_log_creat_string(char *log_text, fat_file target_file) {
    char *text = now_to_str();
    text = str_concat(text, "\t");
    text = str_concat(text, getlogin());
    text = str_concat(text, "\t");
    text = str_concat(text, target_file->filepath);
    text = str_concat(text, "\t");
    text = str_concat(text, log_text);
    text = str_concat(text, "\n");

    return (text);
}

/* Makes a call to fat_fuse_log_creat_string to receive
 * the message to be logged into fs.log and logs it using
 * fat_fuse_log_write.
 */
static void fat_fuse_log_activity(char *log_text, fat_file target_file) {
    char *text = fat_fuse_log_creat_string(log_text, target_file);
    if (text == NULL) {
        // In this case no message is logged
        return;
    }
    fat_fuse_log_write(text);
    free(text);
}

/* Get file attributes (file descriptor version) */
static int fat_fuse_fgetattr(const char *path, struct stat *stbuf,
                             struct fuse_file_info *fi) {
    fat_file file = (fat_file)fat_tree_get_file((fat_tree_node)fi->fh);
    fat_file_to_stbuf(file, stbuf);
    return 0;
}

/* Get file attributes (path version) */
static int fat_fuse_getattr(const char *path, struct stat *stbuf) {
    fat_volume vol;
    fat_file file;

    vol = get_fat_volume();
    file = fat_tree_search(vol->file_tree, path);
    if (file == NULL) {
        errno = ENOENT;
        return -errno;
    }
    fat_file_to_stbuf(file, stbuf);
    return 0;
}

/* Open a file */
static int fat_fuse_open(const char *path, struct fuse_file_info *fi) {
    fat_volume vol;
    fat_tree_node file_node;
    fat_file file;

    vol = get_fat_volume();
    file_node = fat_tree_node_search(vol->file_tree, path);
    if (!file_node)
        return -errno;
    file = fat_tree_get_file(file_node);
    if (fat_file_is_directory(file))
        return -EISDIR;
    fat_tree_inc_num_times_opened(file_node);
    fi->fh = (uintptr_t)file_node;
    return 0;
}

/* Open a directory */
static int fat_fuse_opendir(const char *path, struct fuse_file_info *fi) {
    fat_volume vol = NULL;
    fat_tree_node file_node = NULL;
    fat_file file = NULL;

    vol = get_fat_volume();
    file_node = fat_tree_node_search(vol->file_tree, path);
    if (file_node == NULL) {
        return -errno;
    }
    file = fat_tree_get_file(file_node);
    if (!fat_file_is_directory(file)) {
        return -ENOTDIR;
    }
    fat_tree_inc_num_times_opened(file_node);
    fi->fh = (uintptr_t)file_node;
    return 0;
}

/* Read directory children */
static void fat_fuse_read_children(fat_tree_node dir_node) {
    fat_volume vol = get_fat_volume();
    fat_file dir = fat_tree_get_file(dir_node);
    GList *children_list = fat_file_read_children(dir);
    // Add child to tree. TODO handle duplicates
    for (GList *l = children_list; l != NULL; l = l->next) {
        vol->file_tree =
            fat_tree_insert(vol->file_tree, dir_node, (fat_file)l->data);
    }
    fat_tree_node log_node = fat_tree_node_search(vol->file_tree, "/fs.log");
    if (log_node == NULL) {
        DEBUG("log doesn't exist");
        fat_fuse_log_create();
    } else {
        DEBUG("log exists");
    }
}

/* Add entries of a directory in @fi to @buf using @filler function. */
static int fat_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                            off_t offset, struct fuse_file_info *fi) {
    errno = 0;
    fat_tree_node dir_node = (fat_tree_node)fi->fh;
    fat_file dir = fat_tree_get_file(dir_node);
    fat_file *children = NULL, *child = NULL;
    int error = 0;

    // Insert first two filenames (. and ..)
    if ((*filler)(buf, ".", NULL, 0) || (*filler)(buf, "..", NULL, 0)) {
        return -errno;
    }
    if (!fat_file_is_directory(dir)) {
        errno = ENOTDIR;
        return -errno;
    }
    if (dir->children_read != 1) {
        fat_fuse_read_children(dir_node);
        if (errno < 0) {
            return -errno;
        }
    }

    children = fat_tree_flatten_h_children(dir_node);
    child = children;
    while (*child != NULL) {
        if(strcmp((*child)->filepath, "/fs.log") != 0){
            error = (*filler)(buf, (*child)->name, NULL, 0);
            if (error != 0) {
                return -errno;
            }
        }
        child++;
    }
    return 0;
}

/* Read data from a file */
static int fat_fuse_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi) {
    errno = 0;
    int bytes_read;
    fat_tree_node file_node = (fat_tree_node)fi->fh;
    fat_file file = fat_tree_get_file(file_node);
    fat_file parent = fat_tree_get_parent(file_node);

    bytes_read = fat_file_pread(file, buf, size, offset, parent);
    if (errno != 0) {
        return -errno;
    }

    fat_fuse_log_activity("read", file);

    return bytes_read;
}

/* Write data from a file */
static int fat_fuse_write(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi) {
    fat_tree_node file_node = (fat_tree_node)fi->fh;
    fat_file file = fat_tree_get_file(file_node);
    fat_file parent = fat_tree_get_parent(file_node);

    if (size == 0)
        return 0; // Nothing to write
    if (offset > file->dentry->file_size)
        return -EOVERFLOW;

    fat_fuse_log_activity("write", file);

    return fat_file_pwrite(file, buf, size, offset, parent);
}

/* Close a file */
static int fat_fuse_release(const char *path, struct fuse_file_info *fi) {
    fat_tree_node file = (fat_tree_node)fi->fh;
    fat_tree_dec_num_times_opened(file);
    return 0;
}

/* Close a directory */
static int fat_fuse_releasedir(const char *path, struct fuse_file_info *fi) {
    fat_tree_node file = (fat_tree_node)fi->fh;
    fat_tree_dec_num_times_opened(file);
    return 0;
}

static int fat_fuse_mkdir(const char *path, mode_t mode) {
    errno = 0;
    fat_volume vol = NULL;
    fat_file parent = NULL, new_file = NULL;
    fat_tree_node parent_node = NULL;

    // The system has already checked the path does not exist. We get the parent
    vol = get_fat_volume();
    parent_node = fat_tree_node_search(vol->file_tree, dirname(strdup(path)));
    if (parent_node == NULL) {
        errno = ENOENT;
        return -errno;
    }
    parent = fat_tree_get_file(parent_node);
    if (!fat_file_is_directory(parent)) {
        fat_error("Error! Parent is not directory\n");
        errno = ENOTDIR;
        return -errno;
    }

    // init child
    new_file = fat_file_init(vol->table, true, strdup(path));
    if (errno != 0) {
        return -errno;
    }
    // insert to directory tree representation
    vol->file_tree = fat_tree_insert(vol->file_tree, parent_node, new_file);
    // write file in parent's entry (disk)
    fat_file_dentry_add_child(parent, new_file);
    return -errno;
}

/* Creates a new file in @path. @mode and @dev are ignored. */
static int fat_fuse_mknod(const char *path, mode_t mode, dev_t dev) {
    errno = 0;
    fat_volume vol;
    fat_file parent, new_file;
    fat_tree_node parent_node;

    // The system has already checked the path does not exist. We get the parent
    vol = get_fat_volume();
    parent_node = fat_tree_node_search(vol->file_tree, dirname(strdup(path)));
    if (parent_node == NULL) {
        errno = ENOENT;
        return -errno;
    }
    parent = fat_tree_get_file(parent_node);
    if (!fat_file_is_directory(parent)) {
        fat_error("Error! Parent is not directory\n");
        errno = ENOTDIR;
        return -errno;
    }
    new_file = fat_file_init(vol->table, false, strdup(path));
    if (errno < 0) {
        return -errno;
    }
    // insert to directory tree representation
    vol->file_tree = fat_tree_insert(vol->file_tree, parent_node, new_file);
    // Write dentry in parent cluster
    fat_file_dentry_add_child(parent, new_file);
    return -errno;
}

static int fat_fuse_utime(const char *path, struct utimbuf *buf) {
    errno = 0;
    fat_file parent = NULL;
    fat_volume vol = get_fat_volume();
    fat_tree_node file_node = fat_tree_node_search(vol->file_tree, path);
    if (file_node == NULL || errno != 0) {
        errno = ENOENT;
        return -errno;
    }
    parent = fat_tree_get_parent(file_node);
    if (parent == NULL || errno != 0) {
        DEBUG("WARNING: Setting time for parent ignored");
        return 0; // We do nothing, no utime for parent
    }
    fat_utime(fat_tree_get_file(file_node), parent, buf);
    return -errno;
}

/* Shortens the file at the given offset.*/
int fat_fuse_truncate(const char *path, off_t offset) {
    errno = 0;
    fat_volume vol = get_fat_volume();
    fat_file file = NULL, parent = NULL;
    fat_tree_node file_node = fat_tree_node_search(vol->file_tree, path);
    if (file_node == NULL || errno != 0) {
        errno = ENOENT;
        return -errno;
    }
    file = fat_tree_get_file(file_node);
    if (fat_file_is_directory(file))
        return -EISDIR;

    parent = fat_tree_get_parent(file_node);
    fat_tree_inc_num_times_opened(file_node);
    fat_file_truncate(file, offset, parent);
    return -errno;
}

/* Filesystem operations for FUSE.  Only some of the possible operations are
 * implemented (the rest stay as NULL pointers and are interpreted as not
 * implemented by FUSE). */
struct fuse_operations fat_fuse_operations = {
    .fgetattr = fat_fuse_fgetattr,
    .getattr = fat_fuse_getattr,
    .open = fat_fuse_open,
    .opendir = fat_fuse_opendir,
    .mkdir = fat_fuse_mkdir,
    .mknod = fat_fuse_mknod,
    .read = fat_fuse_read,
    .readdir = fat_fuse_readdir,
    .release = fat_fuse_release,
    .releasedir = fat_fuse_releasedir,
    .utime = fat_fuse_utime,
    .truncate = fat_fuse_truncate,
    .write = fat_fuse_write,

/* We use `struct fat_file_s's as file handles, so we do not need to
 * require that the file path be passed to operations such as read() */
#if FUSE_MAJOR_VERSION > 2 || \
    (FUSE_MAJOR_VERSION == 2 && FUSE_MINOR_VERSION >= 8)
    .flag_nullpath_ok = 1,
#endif
#if FUSE_MAJOR_VERSION > 2 || \
    (FUSE_MAJOR_VERSION == 2 && FUSE_MINOR_VERSION >= 9)
    .flag_nopath = 1,
#endif
};
