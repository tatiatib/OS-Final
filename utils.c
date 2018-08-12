#include "utils.h"


static void put_path(struct msg * data, char ** res, int size){
	memcpy(*res, &size, sizeof(int));
	memcpy(*res + sizeof(int), &data->type, sizeof(int));
	size_t length = strlen(data->path);
	memcpy(*res + sizeof(int)*2, &length, sizeof(size_t));
	memcpy(*res + sizeof(int)*2 + sizeof(size_t), data->path, strlen(data->path) + 1);
}

static void put_fd(struct msg * data, char ** res, int size){
	memcpy(*res, &size, sizeof(int));
	memcpy(*res + sizeof(int), &data->type, sizeof(int));
	memcpy(*res + sizeof(int)*2, &data->fh, sizeof(int));
	memcpy(*res + sizeof(int)*3, &data->size, sizeof(int));
	memcpy(*res + sizeof(int)*4, &data->offset, sizeof(int));
}

int get_path_data(struct msg * data, char ** res){
	int size = sizeof(int) * 2 + sizeof(size_t) + strlen(data->path) + 1;
	*res = malloc(size);
	put_path(data, res, size - sizeof(int));
	return size;
}

int get_open_data(struct msg * data, char ** res){
	int size = sizeof(int)*2 + sizeof(size_t) + strlen(data->path) + 1 + sizeof(int);
	*res = malloc(size);
	put_path(data, res, size - sizeof(int));
	int pointer = sizeof(int) * 2 + sizeof(size_t) + strlen(data->path) + 1;
	memcpy(*res + pointer, &data->flags, sizeof(int) );
	return size;
}

int get_read_data(struct msg * data, char ** res){
	int size = sizeof(int) * 5;
	*res = malloc(size);
	put_fd(data, res, size - sizeof(int));
	return size;
}

int get_write_data(struct msg * data, char ** res){
	int size = sizeof(int) * 6 + data->size + 1 + strlen(data->path);
	*res = malloc(size);
	put_fd(data, res, size - sizeof(int));
	int length = strlen(data->path);
	memcpy(*res + sizeof(int) * 5, &length, sizeof(int));
	memcpy(*res + sizeof(int) * 6, data->path, strlen(data->path) + 1);
	memcpy(*res + sizeof(int) * 6 + strlen(data->path) + 1, data->buf, data->size);
	return size;
}

int get_fd_data(struct msg * data, char ** res){
	int size = sizeof(int) * 2;
	*res = malloc(size + sizeof(int));
	memcpy(*res, &size, sizeof(int));
	memcpy(*res + sizeof(int), &data->type, sizeof(int));
	memcpy(*res + sizeof(int)*2, &data->fh, sizeof(int));
	return size + sizeof(int);
}

int get_dir_data(struct msg * data, char ** res){
	int size = sizeof(int) + sizeof(uintptr_t);
	*res = malloc(size + sizeof(int));
	memcpy(*res, &size, sizeof(int));
	memcpy(*res + sizeof(int), &data->type, sizeof(int));
	memcpy(*res + sizeof(int) * 2, &data->dir, sizeof(uintptr_t));
	return size + sizeof(int);	
}

int get_rename_data(struct msg* data, char ** data_to_send){
	int size = sizeof(int) * 2 + 2 * sizeof(size_t) + strlen(data->path) + 1 + strlen(data->new_name) + 1;
	*data_to_send = malloc(size);
	put_path(data, data_to_send, size - sizeof(int));

	int pointer = sizeof(int) * 2 + sizeof(size_t) + strlen(data->path) + 1;
	size_t length = strlen(data->new_name);
	memcpy(*data_to_send + pointer, &length, sizeof(size_t));
	pointer += sizeof(size_t);
	memcpy(*data_to_send + pointer, data->new_name, strlen(data->new_name) + 1);
	return size;
}

int get_mode_data(struct msg *data, char ** res){
	int size = sizeof(int) * 2 + sizeof(size_t) + strlen(data->path) + 1 + sizeof(mode_t);
	*res = malloc(size);
	put_path(data, res, size - sizeof(int));	
	int pointer = sizeof(int)*2 + sizeof(size_t) + strlen(data->path) + 1;
	memcpy(*res + pointer, &data->mode, sizeof(mode_t));
	return size;
}

int get_create_data(struct msg * data, char ** res){
	int size = sizeof(int) * 2 + sizeof(size_t) + strlen(data->path) + 1 + 
		sizeof(int) + sizeof(mode_t);
	*res = malloc(size);
	put_path(data, res, size - sizeof(int));
	int pointer = sizeof(int) * 2 + sizeof(size_t) + strlen(data->path) + 1;
	memcpy(*res + pointer, &data->flags, sizeof(int));
	pointer += sizeof(int);
	memcpy(*res + pointer, &data->mode, sizeof(mode_t));
	return size;
}

int get_truncate_data(struct msg * data, char ** res){
	int size = sizeof(int) * 2 + sizeof(size_t) + strlen(data->path) + 1 + sizeof(int);
	*res = malloc(size);
	put_path(data, res, size - sizeof(int));
	int pointer = sizeof(int)*2+ sizeof(size_t) + strlen(data->path) + 1;
	memcpy(*res + pointer, &data->size, sizeof(int));
	return size;
}


int get_chunk_data(struct msg * data, char ** res){
	return get_truncate_data(data, res);
}

int get_chunk_buf_data(struct msg * data, char ** res){
	return get_write_data(data, res);
}

