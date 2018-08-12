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

#define BLOCK 1024

int get_server_timeout(int fd, int fd_index, struct auxdata data){
	int ret;
	time_t start = time(NULL);
	printf("[%s] %s %s:%d Trying to connect for %d seconds\n", strtok(ctime(&start), "\n"), data.diskname, 
				data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, data.timeout);
	close(fd);
	fd = socket(AF_INET, SOCK_STREAM, 0);
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(data.ip_ports[fd_index].port);
	inet_aton(data.ip_ports[fd_index].ip, (struct in_addr *)&addr.sin_addr.s_addr);

	time_t end = time(NULL);
	while(difftime(end, start) < (double)data.timeout){
		ret = connect(fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
		if (ret == 0){
			printf("[%s] %s %s:%d connection established\n", strtok(ctime(&end), "\n"), data.diskname, 
				data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);	
			return 0;
		}
		end = time(NULL);
	}

	
	return -1;
}


static void swap_fd_servers(int fd_index){
	struct fuse_context *context = fuse_get_context();
	struct auxdata * data = (struct auxdata *)context->private_data;
	int fd_hotswap = data->fds[data->fd_numb];

	char * ip_hotswap = data->swap_ip_port[0].ip;
	int port_hotswap = data->swap_ip_port[0].port;

	data->fds[data->fd_numb] = data->fds[fd_index];
	data->swap_ip_port[0].ip = data->ip_ports[fd_index].ip;
	data->swap_ip_port[0].port = data->ip_ports[fd_index].port;

	data->fds[fd_index] = fd_hotswap;
	data->ip_ports[fd_index].ip = ip_hotswap;
	data->ip_ports[fd_index].port = port_hotswap;
}

int check_hotswap_connection(struct auxdata data, int fd_index){
	int fd = data.fds[fd_index];

	int keep_alive_msg = 1;
	send(fd, &keep_alive_msg, sizeof(int), 0);
	int received;
	if ((received = recv(fd, &keep_alive_msg, sizeof(int), 0)) == 0 || received == -1){
		if (get_server_timeout(fd, fd_index, data) == -1)
			return 0;
	}
	return 1;
}

void get_files(int from, int to, int tree){
	int length;
	recv(from, &length, sizeof(int), 0);
	// printf("tree %d\n", tree);
	while (length > 0){
		void * buf = malloc(length);
		recv(from, buf, length, 0);
		int type = *(int*)(buf + sizeof(int));
		// printf("type %d\n",type );
		if (type == 14){
			if (tree) send(to, buf, length, 0);
		}else{
			type = 19;
			memcpy(buf + sizeof(int), &type, sizeof(int));
			send(to, buf, length, 0);
		}
		recv(from, &length, sizeof(int), 0);
		free(buf);
	}
}


int dump_to_hotswap(int fd_index){
	printf("%s\n","dump_to_hotswap" );
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;

	if (check_hotswap_connection(data, fd_index)){
		time_t current_time = time(NULL);
		printf("[%s] %s %s:%d hotswap connected\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
	}else{
		time_t current_time = time(NULL);
		printf("[%s] %s %s:%d hotswap is not connected\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		return 1;
	}
	int i;
	int dump_msg = -1;
	int tree = 1;
	int clear_msg = -2;
	send(data.fds[fd_index], &clear_msg, sizeof(int), 0);
	int done;
	recv(data.fds[fd_index], &done, sizeof(int), 0);
	if (done){
		for (i = 0; i < data.fd_numb; i ++ ){
			if (i != fd_index){
				send(data.fds[i], &dump_msg, sizeof(int), 0);
				get_files(data.fds[i], data.fds[fd_index], tree);
				tree = 0;
			}
		}
	}
	

	return 0;
}

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
    	printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
    	if (get_server_timeout(fd, fd_index, data) == -1){
			swap_fd_servers(fd_index);
			if (dump_to_hotswap(fd_index))
    			return -1;
    	}

    	return connect_server(fd_index, data, data_to_send, size, log_msg, msg, buf);
    }

    if (msg->type == 0){
    	received = recv(fd, buf->stbuf, sizeof(*buf->stbuf), 0);
    	if(received == 0){
    		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
    		if (get_server_timeout(fd, fd_index, data) == -1){
				swap_fd_servers(fd_index);
	    		if (dump_to_hotswap(fd_index))
	    			return -1;
	    	}
	    	return connect_server(fd_index, data, data_to_send, size, log_msg, msg, buf);
    	}
    }

    return status_code;
}

static int get_chunk(int fd, int fd_index, const char * path, int n_block, char ** buf, struct auxdata data){

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
	time_t current_time = time(NULL);
	if (received == 0){
		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		if (get_server_timeout(fd, fd_index, data) == -1){
			swap_fd_servers(fd_index);
    		if (dump_to_hotswap(fd_index))
	    		return -1;
	    }
		return get_chunk(fd, fd_index, path, n_block, buf, data);
	}

	if (size != 0){
		received = recv(fd, *buf, size, 0);
		if (received  == 0) {
			printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
    		if (get_server_timeout(fd, fd_index, data) == -1){
				swap_fd_servers(fd_index);
	    		if (dump_to_hotswap(fd_index))
	    			return -1;
	    	}
	    	return get_chunk(fd, fd_index, path, n_block, buf, data);
		}
	}
	
	memset(*buf + size, 0, BLOCK - size);
	return size;
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
	int file_lenght = 0;
	int status = connect_server(0, data, data_to_send, size, "getattr on path", &msg, &cur);
	// printf("status %d\n", status);
	if (status != 0){
		free(data_to_send);
		return -ENOENT;
	}
	file_lenght += stbuf->st_size;
	
	if (S_ISDIR(stbuf->st_mode)){
		free(data_to_send);
		return status;
	}
	int i;
	struct stat * stat_buf_servers = malloc(sizeof (struct stat));
	struct buf cur_backup = {
		.stbuf = stat_buf_servers
	};
	for (i = 1; i < data.fd_numb;  i++){
		status = connect_server(i, data, data_to_send, size, "getattr on path", &msg, &cur_backup);
		file_lenght += stat_buf_servers->st_size;

	}

	int stripe = BLOCK * data.fd_numb;
	file_lenght = file_lenght % stripe == 0 ? file_lenght - (file_lenght/stripe) * BLOCK : 
		file_lenght - (file_lenght/stripe + 1) * BLOCK ;

	stbuf->st_size = file_lenght;
	free(data_to_send);
	printf("%d\n", file_lenght);
	return status;
}


static int raid_open(const char* path, struct fuse_file_info* fi){
	return 0;
}

static int raid_read(const char* path, char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){

	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int cur_server = (offset / BLOCK) % data.fd_numb;
	int stripe = BLOCK * (data.fd_numb - 1);
	int n_block = offset / stripe;	
	int pointer = 0;

	int received_size;

	do{
		char * buf_pointer = buf + pointer;
		received_size = get_chunk(data.fds[cur_server], cur_server, path, n_block, &buf_pointer, data);
		time_t current_time = time(NULL);
		printf("[%s] %s %s:%d %s %d %s %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[cur_server].ip, data.ip_ports[cur_server].port, 
    		"read ", received_size, "bytes from " , path);
		size -= received_size;
		pointer += received_size;
		n_block = (offset + pointer) / stripe;
		cur_server = ((offset + pointer) / BLOCK) % data.fd_numb;
	}while(received_size == BLOCK && size > 0);

	
	// printf("final buf %s\n",buf);
	// printf("strleen %d\n", strlen(buf));
	// printf("pointer %d\n", pointer);

	return pointer;
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



static void send_chunk(int fd, int fd_index, const char * path, const char * buf,
	 int offset, int size, int n_block, struct auxdata data){
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
	time_t current_time = time(NULL);
	if (received == 0){
		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		if (get_server_timeout(fd, fd_index, data) == -1){
			swap_fd_servers(fd_index);
			if (dump_to_hotswap(fd_index))
    			return;
	    }
	    send_chunk(fd, fd_index, path, buf, offset, size, n_block, data);
	    return;
	}

	if ((int)res != size){
		printf("wrote %zu bytes instead of %d \n", res, size);
	}

}

static void send_xor(int fd, int fd_index, const char * path, char * buf, int size, 
		int n_block, struct auxdata data){
	int i;
	char * xor = malloc(BLOCK);
	memset(xor, 0, BLOCK);

	for (i = 0; i < size; i ++){
		int j;
		for (j = 0; j < BLOCK; j++){
			xor[j] ^= (buf + i*BLOCK)[j];
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

	time_t current_time = time(NULL);
	if (received == 0){
		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		if (get_server_timeout(fd, fd_index, data) == -1){
			swap_fd_servers(fd_index);
			if (dump_to_hotswap(fd_index))
    			return;
	    }
	    send_xor(fd, fd_index, path, buf, size, n_block, data);
	    return;
	}

	if ((int)res != BLOCK){
		printf("wrote %zu bytes instead of %d  while sending xor\n", res, size);
	}
}

static int raid_write(const char* path, const char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){
	// printf("size %d\n", size);
	// printf("offset %d\n", offset);
	// printf("%s\n", buf);
	// printf("%s\n","============================");
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;

	int start_server = (offset / BLOCK) % data.fd_numb;
	char  * xor_data = malloc(BLOCK * (data.fd_numb - 1));
	// printf("start_server %d\n", start_server);
	int j = 0;
	// printf("%d\n", start_server);
	int cur_size = size;
	
	int chunk = BLOCK - (offset % BLOCK);
	if ((offset == 0 && size < BLOCK) || chunk > size) chunk = size;

	int pointer = 0;
	// printf("chunk %d \n",  chunk);

	int stripe = BLOCK * (data.fd_numb - 1);
	int cur_server = (start_server - 1) % data.fd_numb;
	cur_server = cur_server < 0 ? data.fd_numb - 1 : cur_server;
	int cur_rem = offset / stripe;
	int cur_offset = offset - BLOCK;

	int n_block;
	//get previous blocks;
	while (cur_offset > 0){
	
		if (cur_offset / stripe == cur_rem){
			char * xor_pointer = xor_data + j * BLOCK;
			get_chunk(data.fds[cur_server], cur_server, path, cur_rem, &xor_pointer, data);
		
		}else break;
		
		cur_offset -= BLOCK;
		j += 1;
		cur_server = (cur_server - 1) % data.fd_numb;
	}

	//current blocks

	while(cur_size > 0){
		int fd = start_server % data.fd_numb;
		n_block = (offset + pointer) / stripe;
		if (j == data.fd_numb - 1){
			// printf("send xor block_n %d\n",n_block - 1);
			send_xor(data.fds[fd], fd, path, xor_data, data.fd_numb - 1, n_block - 1, data);
			j = 0;
		}
		// printf("fd %d pointer %d\n", fd, pointer);
		// printf("n_block %d chunk %d\n", n_block, chunk);
		if (pointer == 0)
			send_chunk(data.fds[fd], fd, path, buf + pointer, offset % BLOCK, chunk, n_block, data);
		else send_chunk(data.fds[fd], fd, path,  buf + pointer, 0, chunk,  n_block, data);
		

		if (chunk == BLOCK){
			memcpy(xor_data + BLOCK * j, buf + pointer, BLOCK);
			// printf("int chunk = BLock j %d pointer %d \n", j, pointer);
		}else{
			// printf("j is %d\n", j);
			char * xor_pointer = xor_data + j * BLOCK;
			get_chunk(data.fds[fd], fd, path, n_block, &xor_pointer, data);
			// printf("get chunk , block_n %d\n", n_block);
		}
		
		j += 1;
		start_server += 1;
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
	// printf("%s\n", "-----------------------");
	int written = offset + size;
	// printf("written %d\n",written );
	n_block = written / stripe;
	cur_server = start_server % data.fd_numb;
	// printf("cur_server %d\n", cur_server);
	if (j == data.fd_numb - 1){
		n_block = written % stripe == 0 ? n_block - 1 : n_block;
		send_xor(data.fds[cur_server], cur_server, path, xor_data, data.fd_numb - 1, n_block, data);
		free(xor_data);
		return size;
	}

	while ((int)((written + BLOCK) / stripe) == n_block || (written + BLOCK) % stripe == 0){

		char * xor_pointer = xor_data + j * BLOCK;
		get_chunk(data.fds[cur_server], cur_server, path, n_block, &xor_pointer, data);
		j += 1;
		cur_server = (start_server+1) % data.fd_numb;
		if (j == data.fd_numb - 1){
			send_xor(data.fds[cur_server], cur_server, path, xor_data, data.fd_numb - 1, n_block, data);
			break;
		}
		written += BLOCK;
	}

	free(xor_data);
	return size;
}

static int raid_opendir(const char* path, struct fuse_file_info* fi){
	return 0;
}

static int raid_releasedir(const char* path, struct fuse_file_info *fi){
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
		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[0].ip, data.ip_ports[0].port);
		if (get_server_timeout(fd, 0, data) == -1){
			swap_fd_servers(0);
			if (dump_to_hotswap(0))
				return -1;
	   	}

		return 0;
	}
	while (name_length != 0){
		char * name = malloc(name_length + 1);
		received = recv(fd, name, name_length, 0);
		if (received == 0){
			printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[0].ip, data.ip_ports[0].port);
    		if (get_server_timeout(fd, 0, data) == -1){
				swap_fd_servers(0);
				if (dump_to_hotswap(0))
				return -1;
	    	}
	    	return 0;
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


static int raid_chmod(const char *path, mode_t mode){
	return 0;
}
static int raid_chown(const char *path, uid_t uid, gid_t gid){
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
	.readdir    = raid_readdir,
	.chmod 		= raid_chmod,
	.chown		= raid_chown
};

void init_raid_5(char * mountpoint, struct auxdata * data){
	int argc = 3;
	char *argv[3];
	argv[0] = data->diskname;
	argv[1] = "-f";
	argv[2] = mountpoint;

	fuse_main(argc, argv, &raid_oper, data);	
}