#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>	
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>
#include <fuse.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <time.h>
#include <dirent.h>


#include "fuse_raid_1.h"

struct msg{
	int type;
	char * path;
	int size;
	int offset;
	mode_t  mode;
	int flags;
	int fh;
	uintptr_t dir;
	char * new_name;
	char * buf;
};

struct buf{
	struct stat * stbuf;
	char * read_buf;	
};

static void put_path(struct msg * data, char ** res, int size){
	memcpy(*res, &size, sizeof(int));
	memcpy(*res + sizeof(int), &data->type, sizeof(int));
	size_t length = strlen(data->path);
	memcpy(*res + sizeof(int)*2, &length, sizeof(size_t));
	memcpy(*res + sizeof(int)*2 + sizeof(size_t), data->path, strlen(data->path) + 1);
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

static void put_fd(struct msg * data, char ** res, int size){
	memcpy(*res, &size, sizeof(int));
	memcpy(*res + sizeof(int), &data->type, sizeof(int));
	memcpy(*res + sizeof(int)*2, &data->fh, sizeof(int));
	memcpy(*res + sizeof(int)*3, &data->size, sizeof(int));
	memcpy(*res + sizeof(int)*4, &data->offset, sizeof(int));
}

int get_read_data(struct msg * data, char ** res){
	int size = sizeof(int) * 5;
	*res = malloc(size);
	put_fd(data, res, size - sizeof(int));
	return size;
}

int get_write_data(struct msg * data, char ** res){
	int size = sizeof(int) * 5 + data->size;
	*res = malloc(size);
	put_fd(data, res, size - sizeof(int));
	memcpy(*res + sizeof(int) * 5, data->buf, data->size);
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
	int pointer = sizeof(int) + sizeof(size_t) + strlen(data->path) + 1;
	memcpy(*res + pointer, &data->size, sizeof(int));
	return size;
}

static int send_data_buf(int fd_index, char * data_to_send, int size, char * log_msg,
	 struct msg * msg, struct buf * received_buf){

	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd = data.fds[fd_index];

	send(fd, data_to_send, size, 0);
	time_t current_time = time(NULL);
    printf("[%s] %s %s:%d %s %s", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, log_msg, msg->path);
    if (msg->type == 4){
    	printf(" to %s", msg->new_name);
    }
    printf("\n");
  
    int status_code;
    int received = recv(fd, &status_code, sizeof(int), 0);
    if (received == 0){
    	printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
    }

    if (msg->type == 0){
    	if (status_code != 0)
    		status_code = -ENOENT;
    	received = recv(fd, received_buf->stbuf, sizeof(*received_buf->stbuf), 0);
    	if (received == 0){
    		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    			data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
    	}
    }	
    
    if (msg->type == 2){
    	int read_bytes = status_code;
		if (read_bytes != -1 && read_bytes != 0){
			received = recv(fd, received_buf->read_buf, read_bytes, 0);
		}
		printf("[%s] %s %s:%d %s %d %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
			data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, "received ", read_bytes,  " bytes to read");
    	if (received == 0){
    		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    			data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
    	}
    }

    if (msg->type == 8){
    	printf("[%s] %s %s:%d %d bytes wrote at %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
			data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, status_code, msg->path);
    }
    return status_code;
   
}


static void send_data_readdir(int fd_index, char * data_to_send, int size, char * log_msg, char* path,
		void ** buf, fuse_fill_dir_t filler){
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd = data.fds[fd_index];

	send(fd, data_to_send, size, 0);
	time_t current_time = time(NULL);
    printf("[%s] %s %s:%d %s %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, log_msg, path);

    int name_length;
	int received = recv(fd, &name_length, sizeof(int), 0);
	if (received == 0){
		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);

	}
	while(name_length != 0){
		char * name = malloc(name_length + 1);
		received = recv(fd, name, name_length, 0);
		if (received == 0){
			printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		}
		name[name_length] = '\0';
		filler(*buf, name, NULL, 0);
		received = recv(fd, &name_length, sizeof(int), 0);
		if (received == 0){
			printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		}
		free(name);
	}

}

static int net_getattr(const char* path, struct stat* stbuf){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 0,
		.path = (char*)path
	};

	int size = get_path_data(&msg, &data_to_send);
	struct buf cur = {
		.stbuf = stbuf
	};
	int status_code = send_data_buf(0, data_to_send, size, "getattr on path", &msg, &cur);
	
	free(data_to_send);
	return status_code;
}

static int net_open(const char* path, struct fuse_file_info* fi){
	if (fi->fh != 0){
		printf("%s\n", "weird");
		return 0;
	}
	int res = 0;
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 1,
		.path = (char*)path,
		.flags = fi->flags
	};
	int size = get_open_data(&msg, &data_to_send);
	res = send_data_buf(0, data_to_send, size, "open", &msg, NULL);
	// res = send_data_buf(1, data_to_send, size, "open", &msg, NULL);
	free(data_to_send);
	if (res < 0){
		return -ENOENT;
	}

	fi->fh = res;
	return 0;
}

static int net_read(const char* path, char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 2,
		.fh = fi->fh,
		.size = size,
		.offset = offset,
		.path = (char*)path
	};
	
	int buf_size = get_read_data(&msg, &data_to_send);
	struct buf cur = {
		.read_buf = buf
	};
	int bytes_to_read = send_data_buf(0, data_to_send, buf_size, "read", &msg, &cur);

	free(data_to_send);

	return bytes_to_read;
	
}



static int net_release(const char* path, struct fuse_file_info *fi){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 3,
		.fh = fi->fh,
		.path = (char*)path
	};
	int size = get_fd_data(&msg, &data_to_send);
	send_data_buf(0, data_to_send, size, "close", &msg, NULL);

    free(data_to_send);
	return 0;
}

static int net_rename(const char* from, const char* to){
	char *  data_to_send = NULL;
	struct msg msg = {
		.type = 4,
		.path = (char*)from,
		.new_name = (char*)to,
	};
	int size = get_rename_data(&msg, &data_to_send);
	
	send_data_buf(0, data_to_send, size, "rename", &msg, NULL);
	
    free(data_to_send);

	return 0;
}

static int net_unlink(const char* path){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 5,
		.path = (char*)path
	};

	int size = get_path_data(&msg, &data_to_send);
	send_data_buf(0, data_to_send, size, "unlink", &msg, NULL);
	
    free(data_to_send);
	return 0;
}

static int net_rmdir(const char* path){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 6,
		.path = (char*)path
	};

	int size = get_path_data(&msg, &data_to_send);
	send_data_buf(0, data_to_send, size, "remove dir ", &msg, NULL);
	
    free(data_to_send);
	return 0;
}

static int net_mkdir(const char* path, mode_t mode){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 7,
		.path = (char*)path,
		.mode = mode
	};
	int size = get_mode_data(&msg, &data_to_send);
	send_data_buf(0, data_to_send, size, "make dir ", &msg, NULL);

    free(data_to_send);
	return 0;
}

static int net_write(const char* path, const char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){
	if (fi->fh == 0){
		printf("%s is not opened\n", path);
		return -1;
	}
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 8,
		.fh = fi->fh,
		.size = size,
		.offset = offset,
		.buf = (char*)buf,
		.path = (char *)path
	};

	int buf_size = get_write_data(&msg, &data_to_send);
	int bytes_written_1 = send_data_buf(0, data_to_send, buf_size, "write", &msg, NULL);
	// int bytes_written_2 = send_data_buf(1, data_to_send, buf_size, "write", &msg, NULL);
	// if (bytes_written_1 == bytes_written_2){
	// 	printf("%s\n", "Done writing");
	// }

	free(data_to_send);
	return bytes_written_1;
}

static int net_opendir(const char* path, struct fuse_file_info* fi){
	if (fi->fh != 0 || strcmp(path, "/") == 0){
		return 0;
	}
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 9,
		.path = (char*)path
	};

	int size = get_path_data(&msg, &data_to_send);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd = data.fds[0];

	send(fd, data_to_send, size, 0);
    int bytes = recv(fd, &fi->fh, sizeof(intptr_t), 0);
    if (fi->fh == 0){
		return -ENOENT;
	}
	time_t current_time = time(NULL);
	printf("[%s] %s %s:%d %s %s\n", strtok(ctime(&current_time), "\n"), data.diskname, data.ip_ports[0].ip,
        data.ip_ports[0].port, "open dir  ", path);

    free(data_to_send);
    return 0;
}

static int net_releasedir(const char* path, struct fuse_file_info *fi){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 10,
		.dir = fi->fh
	};
	int size = get_dir_data(&msg, &data_to_send);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd = data.fds[0];

	send(fd, data_to_send, size, 0);
	int res;
    recv(fd, &res, sizeof(int), 0);
    if (res == 0){
    	fi->fh = 0;
		time_t current_time = time(NULL);
    	printf("[%s] %s %s:%d %s %s\n", strtok(ctime(&current_time), "\n"), data.diskname, data.ip_ports[0].ip,
            data.ip_ports[0].port, "release dir  ", path);
    }

    free(data_to_send);

	return 0;

}

static int net_create(const char * path, mode_t modes, struct fuse_file_info * fi){
	int res = 0;
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 11,
		.path = (char*)path,
		.flags = fi->flags,
		.mode = modes
	};
	int size = get_create_data(&msg, &data_to_send);
	res = send_data_buf(0, data_to_send, size, "create ", &msg, NULL);
	free(data_to_send);
	if (res < 0){
		return -ENOENT;
	}
	fi->fh = res;
	return 0;
}


static int net_truncate(const char* path, off_t size){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 12,
		.path = (char*)path,
		.size = size
	};
	int buf_size = get_truncate_data(&msg, &data_to_send);
	send_data_buf(0, data_to_send, buf_size, "truncate ", &msg, NULL);

	free(data_to_send);

	return 0;
}

static void net_destroy(void * private_data){
	struct auxdata * data = (struct auxdata *)private_data;
	int i;
	for(i = 0; i < data->fd_numb; i++){
		close(data->fds[i]);
	}
	free(data->ip_ports->ip);
	free(data->ip_ports);
  	free(data->swap_ip_port->ip);
  	free(data->swap_ip_port);
	free(data);
}

// NOT IMPLEMENTED
static int net_utime (const char *path, struct utimbuf *ubuf){
	return 0;
}

static int net_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 13,
		.path = (char*)path
	};
	int size = get_path_data(&msg, &data_to_send);
	send_data_readdir(0, data_to_send, size, "readdir", (char*)path, &buf, filler);

	return 0;
}

static struct fuse_operations net_oper = {
	.getattr	= net_getattr,	
	.open		= net_open,
	.read		= net_read,
	.release	= net_release,
	.rename		= net_rename,
	.unlink		= net_unlink,
	.rmdir		= net_rmdir, 
	.mkdir 		= net_mkdir,
	.write      = net_write,
	.opendir	= net_opendir,
	.releasedir = net_releasedir,
	.create		= net_create,
	.truncate   = net_truncate,
	.destroy	= net_destroy,
	.utime 		= net_utime,
	.readdir    = net_readdir
};



void init_filesys(char * mountpoint, struct auxdata * data){
	int argc = 3;
	char *argv[3];
	argv[0] = data->diskname;
	argv[1] = "-f";
	argv[2] = mountpoint;
	fuse_main(argc, argv, &net_oper, data);
}
