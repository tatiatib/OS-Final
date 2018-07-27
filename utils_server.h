#ifndef UTILS_SERVER_H
#define UTILS_SERVER_C

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>

struct msg{
	int fd;
	intptr_t dir;
	int flags;
	char * path;
	int size;
	int offset;
	mode_t mode;
	char * to;  // used for rename call
	char * buf;
};

struct msg * deserialize_path(char *, int);
struct msg * deserialize_read(char *);
struct msg * deserialize_write(char * );
struct msg * deserialize_close(char * );
struct msg * deserialize_rename(char* );
struct msg * deserialize_mode(char * );
struct msg * deserialize_dir(char * );
struct msg * deserialize_create(char * );
unsigned long hash_djb(unsigned char *);
void compute_hash(char *, unsigned long * );
#endif