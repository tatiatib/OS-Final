#ifndef UTILS_H
#define UTILS_C

#include <sys/stat.h>
#include <fcntl.h>	
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <dirent.h>
#include <stdint.h>

struct msg{
	int type;
	char * path;
	int size;
	int offset;
	mode_t mode;
	int flags;
	int fh;
	uintptr_t dir;
	char * new_name;
	char * buf;
};

struct buf{
	struct stat * stbuf;
	char * read_buf;	
	unsigned long hash;
};

int get_path_data(struct msg *, char **);
int get_open_data(struct msg *, char **);
int get_read_data(struct msg *, char **);
int get_write_data(struct msg *, char **);
int get_fd_data(struct msg * , char ** );
int get_dir_data(struct msg * , char ** );
int get_rename_data(struct msg* , char ** );
int get_mode_data(struct msg *, char ** );
int get_create_data(struct msg * , char ** );
int get_truncate_data(struct msg * , char **);
int get_chunk_data(struct msg *, char **);
int get_chunk_buf_data(struct msg *, char **);

#endif