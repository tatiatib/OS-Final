#include "utils_server.h"

struct msg * deserialize_path(char * buf, int type){
	struct msg * data = malloc(sizeof(struct msg));
	size_t length  = *(size_t*)(buf + sizeof(int));
	data->path = malloc(length + 1);
	strcpy(data->path, buf + sizeof(int)+ sizeof(size_t));
	if (type == 1){
		int pointer = sizeof(int) + sizeof(size_t) + length + 1;
		data->flags = *(int*)(buf + pointer);		
	}
	if (type == 12 || type == 17){
		int pointer = sizeof(int) + sizeof(size_t) + length + 1;
		data->size = *(int*)(buf + pointer);	
	}

	return data;
}

struct msg * deserialize_read(char * buf){
	struct msg * data = malloc(sizeof(struct msg));
	data->fd = *(int*)(buf + sizeof(int));
	data->size = *(int*)(buf + sizeof(int)*2);
	data->offset = *(int*)(buf + sizeof(int)*3);
	return data;
}

struct msg * deserialize_write(char * buf){
	struct msg * data = deserialize_read(buf);
	int length = *(int *)(buf + sizeof(int)*4);
	data->path = malloc(length + 1);
	strcpy(data->path,	buf + sizeof(int)*5);
	data->buf = malloc(data->size);
	memcpy(data->buf, (buf + sizeof(int)*5 + length + 1), data->size);
	data->buf[data->size] = '\0';
	return data;
}

struct msg * deserialize_close(char * buf){
	struct msg * data = malloc(sizeof(struct msg));
	data->fd = *(int*)(buf + sizeof(int));
	return data;
}


struct msg * deserialize_rename(char* buf){
	struct msg * data = malloc(sizeof(struct msg));
	size_t length  = *(size_t*)(buf + sizeof(int));
	data->path = malloc(length + 1);
	strcpy(data->path, buf + sizeof(int)+ sizeof(size_t));
	int pointer = sizeof(int) + sizeof(size_t) + length + 1;
	length = *(size_t *)(buf + pointer);
	data->to = malloc(length + 1);
	strcpy(data->to, buf + pointer + sizeof(size_t));
	return data;
}

struct msg * deserialize_mode(char * buf){
	struct msg * data = deserialize_path(buf, 0);
	data->mode = *(mode_t *)(buf + sizeof(int) + sizeof(size_t) + strlen(data->path) + 1);
	return data;
}

struct msg * deserialize_dir(char * buf){
	struct msg * data = malloc(sizeof(struct msg));
	data->dir = *(intptr_t*)(buf + sizeof(int));
	return data;
}

struct msg * deserialize_create(char * buf){
	struct msg * data = deserialize_path(buf, 1);
	data->mode = *(mode_t *)(buf + sizeof(int) + sizeof(size_t) + strlen(data->path) 
		+ 1 + sizeof(int));
	return data;
}

unsigned long hash_djb(unsigned char *str){
    unsigned long hash = 5381;
    int c;

    while ((c = *str++)){
        hash = ((hash << 5) + hash) + c; 
    }

    return hash;
}

 void compute_hash(char * path, unsigned long * hash){	
 	int fd = open(path, O_RDWR);
	struct stat st;
	fstat(fd, &st);
	int size = st.st_size;
	if (size == 0){
		close(fd);
		return;
	}
	unsigned char buffer[size];
	// unsigned char * buffer = malloc(size);
	read(fd, buffer, size);
	buffer[size] = '\0';
	*hash = hash_djb(buffer);
	close(fd);
	
}