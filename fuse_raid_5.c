#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>
#include "stdio.h"
#include "utils.h"
#include "fuse_raid.h"
#include "fuse.h"

#define BLOCK 10

static int connect_server(int fd_index, struct auxdata data, char * data_to_send, int size, char * log_msg,
	struct msg * msg, struct buf * buf){

	int fd = data.fds[fd_index];
	send(fd, data_to_send, size, 0);
	time_t current_time = time(NULL);
    printf("[%s] %s %s:%d %s %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, log_msg, msg->path);
    int status_code = -1;
    int received = recv(fd, &status_code, sizeof(int), 0);

    if (received == 0){
    	printf("%s\n", "received nothing");
    	return -1;
    }

    if (msg->type == 0){
    	received = recv(fd, buf->stbuf, sizeof(*buf->stbuf), 0);
    }

    return status_code;
}

static int raid_getattr(const char* path, struct stat* stbuf){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 0,
		.path = (char*)path
	};
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;

	int size = get_path_data(&msg, &data_to_send);
	struct buf cur = {
		.stbuf = stbuf
	};
	int status = connect_server(0, data, data_to_send, size, "getattr on path", &msg, &cur);
	
	free(data_to_send);
	if (status != 0){
		printf("%s\n", "ENOENT");
		return -ENOENT;
	}
	
	return status;
}


static int raid_open(const char* path, struct fuse_file_info* fi){
	return 0;
}

static int raid_read(const char* path, char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){
	



	return 0;
}
static int raid_release(const char* path, struct fuse_file_info *fi){
	return 0;
}

static int raid_rename(const char* from, const char* to){
	char *  data_to_send = NULL;
	struct msg msg = {
		.type = 4,
		.path = (char*)from,
		.new_name = (char*)to,
	};
	int size = get_rename_data(&msg, &data_to_send);

	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int i;
	for (i = 0; i < data.fd_numb; i++ ){
		connect_server(i, data, data_to_send, size, "rename", &msg, NULL);
	}

	free(data_to_send);
	return 0;
}

static int raid_unlink(const char* path){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 5,
		.path = (char*)path
	};

	int size = get_path_data(&msg, &data_to_send);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int i;
	for (i = 0; i < data.fd_numb; i++ ){
		connect_server(i, data, data_to_send, size, "unlink", &msg, NULL);
	}

	free(data_to_send);

	return 0;
}

static int raid_rmdir(const char* path){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 6,
		.path = (char*)path
	};

	int size = get_path_data(&msg, &data_to_send);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int i;
	for (i = 0; i < data.fd_numb; i++ ){
		connect_server(i, data, data_to_send, size, "rmdir", &msg, NULL);
	}

	free(data_to_send);
	return  0;
}

static int raid_mkdir(const char* path, mode_t mode){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 7,
		.path = (char*)path,
		.mode = mode
	};

	int size = get_mode_data(&msg, &data_to_send);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int i;
	for (i = 0; i < data.fd_numb; i++ ){
		connect_server(i, data, data_to_send, size, "mkdir", &msg, NULL);
	}

	free(data_to_send);

	return 0;
}

static void get_chunk(int fd, const char * path, int n_block, char ** buf){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 17,
		.path = (char*)path,
		.size = n_block
	};

	int data_size = get_chunk_data(&msg, &data_to_send);
	send(fd, data_to_send, data_size, 0);
	free(data_to_send);

	int size;
	int received = recv(fd, &size, sizeof(int), 0);
	printf("%d\n", size);
	if (size != 0){
		received = recv(fd, *buf, size, 0);
	}
	memset(*buf + size, 0, BLOCK - size);
	printf("get chunk %s\n", *buf);
}

static void send_chunk(int fd, const char * path, const char * buf, int offset, int size, int n_block){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 18,
		.fh = n_block,
		.size = size,
		.offset = offset,
		.buf = (char*)buf,
		.path = (char *)path
	};

	int data_size = get_chunk_buf_data(&msg, &data_to_send);
	send(fd, data_to_send, data_size, 0);
	free(data_to_send);

	size_t res;
	int received = recv(fd, &res, sizeof(size_t), 0);
	if (received == 0){
		printf("%s\n", "connection lost");
	}

	if ((int)res == size){
		printf("%s\n", "correct write");
	}
}

static void send_xor(int fd, const char * path, char * data, int size, int n_block){
	int i;
	char * xor = malloc(BLOCK);
	memset(xor, 0, BLOCK);

	for (i = 0; i < size; i ++){
		int j;
		for (j = 0; j < BLOCK; j++){
			xor[j] ^= (data + i*BLOCK)[j];
			// printf("cur %c xor %c \n", (data + i*BLOCK)[j], xor[j]);	
		}
	}
	
	
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 18,
		.fh = n_block,
		.size = BLOCK,
		.offset = 0,
		.buf = xor,
		.path = (char *)path
	};
	int data_size = get_chunk_buf_data(&msg, &data_to_send);
	send(fd, data_to_send, data_size, 0);
	free(data_to_send);
	free(xor);

	size_t res;
	int received = recv(fd, &res, sizeof(size_t), 0);
	if (received == 0){
		printf("%s\n", "connection lost");
	}

	if ((int)res == BLOCK){
		printf("%s\n", "correct write");
	}
}

static int raid_write(const char* path, const char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){
	printf("size %d\n", size);
	printf("offset %d\n", offset);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;

	int start_server = (offset / BLOCK) % data.fd_numb;
	char  * xor_data = malloc(BLOCK * (data.fd_numb - 1));
	printf("start_server %d\n", start_server);
	int j = 0;
	int i = 0;
	printf("%d\n", start_server);
	int cur_size = size;
	int server_numb = size % BLOCK == 0 ? size / BLOCK : size / BLOCK + 1;
	int chunk = size > BLOCK ? BLOCK - (offset % BLOCK): size;
	int pointer = 0;
	printf("server_numb %d  chunk %d \n", server_numb, chunk);

	int stripe = BLOCK * (data.fd_numb - 1);
	int cur_server = (start_server - 1) % data.fd_numb;
	int cur_rem = offset / stripe;
	int cur_offset = offset - BLOCK;

	int n_block;
	//get previous blocks;
	while (cur_offset > 0){
		if (cur_offset / stripe == cur_rem){
			char * xor_pointer = xor_data + j * BLOCK;
			get_chunk(data.fds[cur_server], path, cur_rem, &xor_pointer);
		}else break;

		cur_offset -= BLOCK;
		j += 1;
		cur_server = (cur_server - 1) % server_numb;
	}
	
	//current blocks

	for (i = start_server; i < server_numb; i++){
		int fd  = i % data.fd_numb;
		n_block = offset + pointer / stripe;
		if (j == data.fd_numb - 1){
			printf("send xor block_n %d\n",n_block - 1);
			send_xor(data.fds[fd], path, xor_data, data.fd_numb - 1, n_block - 1);
			j = 0;
		}
		printf("fd %d pointer %d\n", fd, pointer);
		printf("n_block %d\n", n_block);
		if (pointer == 0)
			send_chunk(data.fds[fd], path, buf + pointer, offset % BLOCK, chunk, n_block);
		else send_chunk(data.fds[fd], path,  buf + pointer, 0, chunk,  n_block);
		

		if (chunk == BLOCK){
			memcpy(xor_data + BLOCK * j, buf + pointer, BLOCK);
			printf("int chunk = BLock j %d pointer %d \n", j, pointer);
		}else{
			printf("j is %d\n", j);
			char * xor_pointer = xor_data + j * BLOCK;
			get_chunk(data.fds[fd], path, n_block, &xor_pointer);
			printf("get chunk , block_n %d\n", n_block);
		}

		j += 1;
		pointer += chunk;
		cur_size -= chunk;
		if (cur_size > BLOCK){
			chunk = BLOCK;
		}else{
			chunk = cur_size;
		}
	}	
	//TODO 
	//next blocks
	int written = offset + size;
	printf("written %d\n",written );
	n_block = written / stripe;
	cur_server = i % data.fd_numb;
	printf("cur_server %d\n", cur_server);
	if (j == data.fd_numb - 1){
		send_xor(data.fds[cur_server], path, xor_data, data.fd_numb - 1, n_block);
		free(xor_data);
		return size;
	}

	while ((written + BLOCK) / stripe == n_block){
		char * xor_pointer = xor_data + j * BLOCK;
		get_chunk(data.fds[cur_server], path, n_block, &xor_pointer);
		printf("get chunk , block_n %d\n", cur_rem);
		j += 1;
		cur_server = (i+1) % data.fd_numb;
		if (j == data.fd_numb - 1){
			send_xor(data.fds[cur_server], path, xor_data, data.fd_numb - 1, n_block);
			printf("send_xor fd %d\n", cur_server);
			break;
		}
		written += BLOCK;
	}

	free(xor_data);
	return size;
}

static int raid_opendir(const char* path, struct fuse_file_info* fi){
	printf("opendir %s\n", path);
	return 0;
}

static int raid_releasedir(const char* path, struct fuse_file_info *fi){
	printf("releasedir %s\n", path);
	return 0;
}

static int raid_create(const char * path, mode_t modes, struct fuse_file_info * fi){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 11,
		.path = (char*)path,
		.flags = fi->flags,
		.mode = modes
	};
	int size = get_create_data(&msg, &data_to_send);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int i;
	for (i = 0; i < data.fd_numb; i++ ){
		connect_server(i, data, data_to_send, size, "create", &msg, NULL);
	}
	
	free(data_to_send);
	
	return 0;
}

static int raid_truncate(const char* path, off_t size){
	printf("truncate %s\n", path);
	return 0;
}

static void raid_destroy(void * private_data){
	struct auxdata * data = (struct auxdata *)private_data;
	int i;
	for(i = 0; i < data->fd_numb + 1; i++){
		close(data->fds[i]);
	}
	close(data->errorlog);
	free(data->ip_ports->ip);
	free(data->ip_ports);
  	free(data->swap_ip_port->ip);
  	free(data->swap_ip_port);
  	free(data->fds);
	free(data);

}

static int raid_utime (const char *path, struct utimbuf *ubuf){
	printf("%s\n","utime" );
	return 0;
}

static int raid_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
			 off_t offset, struct fuse_file_info *fi){

	char * data_to_send = NULL;
	struct msg msg = {
		.type = 13,
		.path = (char*)path
	};
	int size = get_path_data(&msg, &data_to_send);
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd = data.fds[0];

	send(fd, data_to_send, size, 0);
	time_t current_time = time(NULL);
    printf("[%s] %s %s:%d %s %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[0].ip, data.ip_ports[0].port, "readdir ", path);
    int name_length;
	int received = recv(fd, &name_length, sizeof(int), 0);
	if (received == 0){
		printf("%s\n", "received nothing");
		return 0;
	}
	while (name_length != 0){
		char * name = malloc(name_length + 1);
		received = recv(fd, name, name_length, 0);
		if (received == 0){
			printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[0].ip, data.ip_ports[0].port);
		}
		name[name_length] = '\0';
		filler(buf, name, NULL, 0);
		received = recv(fd, &name_length, sizeof(int), 0);
		if (received == 0){
			printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[0].ip, data.ip_ports[0].port);
		}
		free(name);
	}

	free(data_to_send);

	return 0;
}


static struct fuse_operations raid_oper = {
	.getattr	= raid_getattr,	
	.open		= raid_open,
	.read		= raid_read,
	.release	= raid_release,
	.rename		= raid_rename,
	.unlink		= raid_unlink,
	.rmdir		= raid_rmdir, 
	.mkdir 		= raid_mkdir,
	.write      = raid_write,
	.opendir	= raid_opendir,
	.releasedir = raid_releasedir,
	.create		= raid_create,
	.truncate   = raid_truncate,
	.destroy	= raid_destroy,
	.utime 		= raid_utime,
	.readdir    = raid_readdir
};

void init_raid_5(char * mountpoint, struct auxdata * data){
	int argc = 3;
	char *argv[3];
	argv[0] = data->diskname;
	argv[1] = "-f";
	argv[2] = mountpoint;

	fuse_main(argc, argv, &raid_oper, data);	
}