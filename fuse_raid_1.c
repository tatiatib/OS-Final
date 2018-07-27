#define FUSE_USE_VERSION 26
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>
#include <fuse.h>

#include "utils.h"
#include "fuse_raid.h"
#define SERVER_NUMB 3

int get_timeout(int fd, int fd_index, struct auxdata data){
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

static void swap_servers(int fd_index){
	struct fuse_context *context = fuse_get_context();
	struct auxdata * data = (struct auxdata *)context->private_data;
	int fd_hotswap = data->fds[2];
	
	char * ip_hotswap = data->swap_ip_port[0].ip;
	int port_hotswap = data->swap_ip_port[0].port;

	data->fds[2] = data->fds[fd_index];
	data->swap_ip_port[0].ip = data->ip_ports[fd_index].ip;
	data->swap_ip_port[0].port = data->ip_ports[fd_index].port;

	data->fds[fd_index] = fd_hotswap;
	data->ip_ports[fd_index].ip = ip_hotswap;
	data->ip_ports[fd_index].port = port_hotswap;
}


static int check_connection(int fd){
	int keep_alive_msg = 1;
	send(fd, &keep_alive_msg, sizeof(int), 0);
	int received;
	if ((received = recv(fd, &keep_alive_msg, sizeof(int), 0)) == 0 || received == -1){
		return 0;
	}
	return 1;
}

static void rewrite(int from, int to){
	int length;
	recv(from, &length, sizeof(int), 0);
	while (length > 0){
		void * buf = malloc(length);
		recv(from, buf, length, 0);
		send(to, buf, length, 0);
		recv(from, &length, sizeof(int), 0);
		free(buf);
	} 
}


static void restore_file(int from, int to, char * path){
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd_from = data.fds[from];
	int fd_to = data.fds[to];

	char * data_to_send = NULL;
	struct msg msg = {
		.type = 16,
		.path = path
	};

	int size = get_path_data(&msg, &data_to_send);
	send(fd_from, data_to_send, size, 0);
	time_t current_time = time(NULL);
    printf("[%s] %s %s:%d restore file %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[from].ip, data.ip_ports[from].port, path);
    rewrite(fd_from, fd_to);
}

static int rewrite_to_hotswap(int fd_index){
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd = data.fds[fd_index];

	if (check_connection(fd)){
		time_t current_time = time(NULL);
		printf("[%s] %s %s:%d hotswap connected\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
	}else{
		time_t current_time = time(NULL);
		printf("[%s] %s %s:%d hotswap is not connected\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		return 1;
	}
	printf("%s\n", "Rewriting...............");
	int dump_msg = -1;
	send(data.fds[1-fd_index], &dump_msg, sizeof(int), 0);
	rewrite(data.fds[1-fd_index], data.fds[fd_index]);
	printf("%s\n", "Rewriting Done");
	return 0;
}

static int fill_buf(int fd, int fd_index, struct auxdata data, struct buf * received_buf, 
		int *status_code, int buf_type){

	if (*status_code != 0 && buf_type == 0){
    	*status_code = -ENOENT;
	}
	int received;
	if (*status_code != -1 && *status_code != 0 && buf_type == 1){
		received = recv(fd, received_buf->read_buf, *status_code, 0);
	}
	time_t current_time = time(NULL);
	if (buf_type == 0){
		received = recv(fd, received_buf->stbuf, sizeof(*received_buf->stbuf), 0);
	}
	if (buf_type == 2)
		received = recv(fd, &received_buf->hash, sizeof(unsigned long), 0);

	if (received == 0){
		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
			data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);

		if (get_timeout(fd, fd_index, data) == -1){
			swap_servers(fd_index);
			if (rewrite_to_hotswap(fd_index))  //hotswap cant connect
				return 0;
			else return 1;
		}
	}

	if (buf_type == 1){
		printf("[%s] %s %s:%d %s %d %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
			data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, "received ", 
				*status_code,  " bytes to read");
	}
	return 0;
}

static int receive_data_from_storage(int fd_index, char * data_to_send, int size, char * log_msg,
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
    int status_code = -1;
    int received = recv(fd, &status_code, sizeof(int), 0);
    if (received == 0){
    	printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
    	if (get_timeout(fd, fd_index, data) == -1){
    		swap_servers(fd_index);
    		if (rewrite_to_hotswap(fd_index)){
    			return -1;
    		}
    	}
    
		return receive_data_from_storage(fd_index, data_to_send, size, log_msg, msg, 
			received_buf);

    }

    if (msg->type == 0){
    	if (fill_buf(fd, fd_index, data, received_buf, &status_code, 0)){
    		return receive_data_from_storage(fd_index, data_to_send, size, log_msg, msg, 
    				received_buf);	
    	}
    }	

    if (msg->type == 1 && status_code == -2){
    	printf("[%s] %s %s:%d wrong hash, restoring file %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    		data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, msg->path);
    	restore_file(fd_index ^ 1, fd_index, msg->path);
    	return receive_data_from_storage(fd_index, data_to_send, size, log_msg, msg, 
    				received_buf);

    }

    if (msg->type == 2){
    	if (fill_buf(fd, fd_index, data, received_buf, &status_code, 1)){
    		return receive_data_from_storage(fd_index, data_to_send, size, log_msg, msg, 
    				received_buf);
    	}

    }

    if (msg->type == 8 || msg->type == 12){
    	if (fill_buf(fd, fd_index, data, received_buf, &status_code, 2)){
    		return receive_data_from_storage(fd_index, data_to_send, size, log_msg, msg, 
    				received_buf);
    	} 
    	if (msg->type == 8)
			printf("[%s] %s %s:%d %d bytes wrote at %s\n", strtok(ctime(&current_time), "\n"), data.diskname, 
				data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port, status_code, msg->path);
    }
    return status_code;
   
}


static int send_keep_alive(int fd_index){
	struct fuse_context *context = fuse_get_context();
	struct auxdata data = *(struct auxdata *)context->private_data;
	int fd = data.fds[fd_index];
	int keep_alive_msg = 1;
	send(fd, &keep_alive_msg, sizeof(int), 0);

	if (recv(fd, &keep_alive_msg, sizeof(int), 0) == 0){
		time_t current_time = time(NULL);
		printf("[%s] %s %s:%d connection lost\n", strtok(ctime(&current_time), "\n"), data.diskname, 
    			data.ip_ports[fd_index].ip, data.ip_ports[fd_index].port);
		if (get_timeout(fd, fd_index, data) == -1){
    		swap_servers(fd_index);
    		if (rewrite_to_hotswap(fd_index))
    			return -1;
    	}

	}
	return 0;

}

static int receive_readdir_from_storage(int fd_index, char * data_to_send, int size, char * log_msg, 
	char* path, void ** buf, fuse_fill_dir_t filler){
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
		if (get_timeout(fd, fd_index, data) == -1){
    		swap_servers(fd_index);
    		if (rewrite_to_hotswap(fd_index)){
    			return -1;
    		}else{
    			return receive_readdir_from_storage(fd_index, data_to_send, size, log_msg, path, buf, filler);
    		}

    	}
		

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

	return 0;

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
	int status_code = receive_data_from_storage(0, data_to_send, size, "getattr on path", &msg, &cur);

	struct stat * stat_buf_check = malloc(sizeof (struct stat));
	struct buf cur_backup = {
		.stbuf = stat_buf_check
	};
	int backup_status = receive_data_from_storage(1, data_to_send, size, "getattr on path", &msg, &cur_backup);
	if (status_code != backup_status)
		status_code == -2 ? restore_file(1, 0, (char*)path) : restore_file(0, 1, (char*)path);

	free(stat_buf_check);
	free(data_to_send);
	return status_code;
}

static int net_open(const char* path, struct fuse_file_info* fi){
	if (fi->fh != 0){
		printf("%s\n", "weird");
		return 0;
	}
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 1,
		.path = (char*)path,
		.flags = fi->flags
	};
	int size = get_open_data(&msg, &data_to_send);
	int fd_1 = receive_data_from_storage(0, data_to_send, size, "open", &msg, NULL);
	int fd_2 = receive_data_from_storage(1, data_to_send, size, "open", &msg, NULL);
	free(data_to_send);

	if (fd_1 < 0 || fd_2 < 0){
		return -ENOENT;
	}

	fi->keep_cache = 1;
	memcpy(&fi->fh, &fd_1, sizeof(int));
	memcpy((char*)(&fi->fh) + sizeof(int), &fd_2, sizeof(int));
	return 0;
}

static int net_read(const char* path, char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){

	char * data_to_send = NULL;
	struct msg msg = {
		.type = 2,
		.fh = *(int*)(&fi->fh),
		.size = size,
		.offset = offset,
		.path = (char*)path
	};

	int buf_size = get_read_data(&msg, &data_to_send);
	struct buf cur = {
		.read_buf = buf
	};
	int bytes_to_read = receive_data_from_storage(0, data_to_send, buf_size, "read", &msg, &cur);
	send_keep_alive(1);
	free(data_to_send);

	return bytes_to_read;
	
}



static int net_release(const char* path, struct fuse_file_info *fi){
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 3,
		.fh = *(int*)(&fi->fh),
		.path = (char*)path
	};

	int size = get_fd_data(&msg, &data_to_send);
	receive_data_from_storage(0, data_to_send, size, "close", &msg, NULL);

	msg.fh = *(int*)((char*)(&fi->fh) + sizeof(int));
	size = get_fd_data(&msg, &data_to_send);
	receive_data_from_storage(1, data_to_send, size, "close", &msg, NULL);

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
	
	receive_data_from_storage(0, data_to_send, size, "rename", &msg, NULL);
	receive_data_from_storage(1, data_to_send, size, "rename", &msg, NULL);

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
	receive_data_from_storage(0, data_to_send, size, "unlink", &msg, NULL);
	receive_data_from_storage(1, data_to_send, size, "unlink", &msg, NULL);

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
	receive_data_from_storage(0, data_to_send, size, "remove dir ", &msg, NULL);
	receive_data_from_storage(1, data_to_send, size, "remove dir ", &msg, NULL);

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
	receive_data_from_storage(0, data_to_send, size, "make dir ", &msg, NULL);
	receive_data_from_storage(1, data_to_send, size, "make dir ", &msg, NULL);

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
		.fh = *(int*)(&fi->fh),
		.size = size,
		.offset = offset,
		.buf = (char*)buf,
		.path = (char *)path
	};

	int buf_size = get_write_data(&msg, &data_to_send);
	struct buf server_1 = {
		.hash = 0
	};
	struct buf server_2 = {
		.hash = 0
	};
	int bytes_written = receive_data_from_storage(0, data_to_send, buf_size, "write", &msg, &server_1);

	msg.fh = *(int*)((char*)(&fi->fh) + sizeof(int));
	buf_size = get_write_data(&msg, &data_to_send);
	receive_data_from_storage(1, data_to_send, buf_size, "write", &msg, &server_2);
	if (server_1.hash != server_2.hash){
		printf("%s %s \n", "Error while writing file ", path);
	}
	free(data_to_send);
	return bytes_written;
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
    recv(fd, &fi->fh, sizeof(intptr_t), 0);
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
	if (strcmp(path, "/") == 0) return 0;
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
	char * data_to_send = NULL;
	struct msg msg = {
		.type = 11,
		.path = (char*)path,
		.flags = fi->flags,
		.mode = modes
	};
	int size = get_create_data(&msg, &data_to_send);
	int fd_1 = receive_data_from_storage(0, data_to_send, size, "create ", &msg, NULL);
	int fd_2 = receive_data_from_storage(1, data_to_send, size, "create ", &msg, NULL);
	free(data_to_send);
	
	if (fd_1 < 0 || fd_2 < 0){
		return -ENOENT;
	}
	memcpy(&fi->fh, &fd_1, sizeof(int));
	memcpy((char*)(&fi->fh) + sizeof(int), &fd_2, sizeof(int));
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
	struct buf server_1 = {
		.hash = 0
	};
	struct buf server_2 = {
		.hash = 0
	};
	receive_data_from_storage(0, data_to_send, buf_size, "truncate ", &msg, &server_1);
	receive_data_from_storage(1, data_to_send, buf_size, "truncate ", &msg, &server_2);
	free(data_to_send);

	return 0;
}

static void net_destroy(void * private_data){
	struct auxdata * data = (struct auxdata *)private_data;
	int i;
	for(i = 0; i < SERVER_NUMB; i++){
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
	receive_readdir_from_storage(0, data_to_send, size, "readdir", (char*)path, &buf, filler);
	free(data_to_send);
	send_keep_alive(1);
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



void init_raid_1(char * mountpoint, struct auxdata * data){
	int argc = 3;
	char *argv[3];
	argv[0] = data->diskname;
	argv[1] = "-f";
	argv[2] = mountpoint;
	fuse_main(argc, argv, &net_oper, data);
}
