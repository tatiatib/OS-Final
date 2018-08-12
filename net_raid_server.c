#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/xattr.h>
#include <dirent.h>
#include <errno.h>
#include "utils_server.h"
#define BACKLOG 10
#define FUNC_NUMB 20
#define BLOCK 1024

void net_get_attr(int cfd, char * buf, int type, char * path);
void net_open(int cfd, char * buf, int type, char * path);
void net_read(int cfd, char * buf, int type, char * path);
void net_close(int cfd, char * buf, int type, char * path);
void net_rename(int cfd, char * buf, int type, char * path);
void net_unlink(int cfd, char * buf, int type, char * path);
void net_rmdir(int cfd, char * buf, int type, char * path);
void net_mkdir(int cfd, char * buf, int type, char * path);
void net_write(int cfd, char * buf, int type, char * path);
void net_opendir(int cfd, char * buf, int type, char * path);
void net_closedir(int cfd, char * buf, int type, char * path);
void net_create(int cfd, char * buf, int type, char * path);
void net_truncate(int cfd, char * buf, int type, char * path);
void net_readdir(int cfd, char * buf, int type, char * path);
void net_hostwap_storage(int cfd, char * buf, int type, char * path);
void net_hotswap_file_content(int cfd, char * buf, int type, char * path);
void net_restore_file(int cfd, char * buf, int type, char * path);

void net_send_chunk(int cfd, char * buf, int type, char * path);
void net_get_chunk(int cfd, char * buf, int type, char * path);
void net_get_xordfile(int cfd, char * buf, int type, char * path);

typedef void (*fun)(int cfd, char * buf, int type, char * path);

static fun functions[FUNC_NUMB] = {net_get_attr, net_open, net_read, net_close,
	net_rename, net_unlink, net_rmdir,  net_mkdir, net_write, net_opendir, 
	net_closedir, net_create, net_truncate, net_readdir, net_hostwap_storage,
	net_hotswap_file_content, net_restore_file, net_send_chunk, net_get_chunk,
	net_get_xordfile};

void net_open(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	int fd = open(temp, data->flags);
	unsigned long old_hash = -1;
	if (fgetxattr(fd, "user.hash", &old_hash, sizeof(unsigned long)) == -1){
		perror(strerror(errno));
	}else{
		unsigned long hash;
		compute_hash(temp, &hash);
		if (old_hash == hash){
			send(cfd, &fd, sizeof(int), 0);
			return;
		}
		else{
			int err = -2;
			send(cfd, &err, sizeof(int), 0);
			return;
		}

	}
	if (old_hash == -1){
		send(cfd, &fd, sizeof(int), 0);
	}else{
		int err = -1;
		send(cfd, &err, sizeof(int), 0);	
	}
	free(data);
	
}

void net_get_attr(int cfd, char * buf_data, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf_data, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	struct stat buf;

	int res = stat(temp, &buf);
	char * data_to_send = malloc(sizeof(int) + sizeof(struct stat));
	memcpy(data_to_send, &res, sizeof(int));
	memcpy(data_to_send + sizeof(int), &buf, sizeof(struct stat));
	send(cfd, data_to_send, sizeof(struct stat) + sizeof(int), 0);
	free(data_to_send);
	free(data);
}

void net_read(int cfd, char * buf_data, int type, char * mountpoint){
	struct msg * data = deserialize_read(buf_data);
	char * buf = malloc(data->size + sizeof(int));
	size_t bytes_read = pread(data->fd, (char*)buf + sizeof(int), data->size, data->offset);

	memcpy(buf, &bytes_read, sizeof(int));
    send(cfd, buf, sizeof(int) + bytes_read, 0);
    free(buf);
    free(data);
	
}

void net_close(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_close(buf);
	int res = close((int)data->fd);
	send(cfd, &res, sizeof(int), 0);
	free(data);
}

void net_rename(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_rename(buf);
	char old[strlen(mountpoint) + strlen(data->path)];
	char new[strlen(mountpoint) + strlen(data->to)];
	strcpy(old, mountpoint);
	strcat(old, data->path);
	strcpy(new, mountpoint);
	strcat(new, data->to);
	int ret = rename(old, new);

	send(cfd, &ret, sizeof(int), 0);
	free(data);
}

void net_unlink(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);

	int ret = unlink(temp);
	send(cfd, &ret, sizeof(int), 0);
	free(data);
}

void net_rmdir(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	int ret = rmdir(temp);
	send(cfd, &ret, sizeof(int), 0);	
	free(data);
}

void net_mkdir(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_mode(buf);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);

	int ret = mkdir(temp, data->mode);
	send(cfd, &ret, sizeof(int), 0);
	free(data);	
}

void net_opendir(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	DIR * dp = opendir(temp);
	intptr_t dp_pointer = (intptr_t)dp;
 	if (dp == NULL){
		intptr_t err = 0;
		send(cfd, &err, sizeof(intptr_t), 0);	
	}else{;
		send(cfd, &dp_pointer, sizeof(intptr_t), 0);	
	}
	free(data);
	
}

void net_closedir(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_dir(buf);
	int ret = closedir((DIR *)(uintptr_t)data->dir);
	send(cfd, &ret, sizeof(int), 0);
	free(data);
}

void net_create(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_create(buf);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	mknod(temp, data->mode, S_IFREG);
	int fd = open(temp, 34881);
	// int fd = open(temp, data->flags);
	send(cfd, &fd, sizeof(int), 0);
	free(data);
}

void net_truncate(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	int ret = truncate(temp, data->size);
	unsigned long hash;
	compute_hash(temp, &hash);
	if (setxattr(temp, "user.hash", &hash, sizeof(unsigned long), 0) == -1){
		perror(strerror(errno));
	}
	char * resp = malloc(sizeof(int) + sizeof(unsigned long));
	memcpy(resp, &ret, sizeof(int));
	memcpy(resp + sizeof(int), &hash, sizeof(unsigned long));
	send(cfd, resp, sizeof(int) + sizeof(unsigned long), 0);
	
	free(data);
}

void net_write(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_write(buf);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	size_t written = pwrite(data->fd, data->buf, data->size, data->offset);
	
	unsigned long hash;
	// compute_hash(temp, &hash);
	if (fgetxattr(data->fd, "user.hash", &hash, sizeof(unsigned long)) == -1){
		hash = 5381;
	}
	hash = hash_djb((unsigned char *)data->buf, hash);
	// printf("hash write %lu\n", hash);
	if (fsetxattr(data->fd, "user.hash", &hash, sizeof(unsigned long), 0) == -1){
		perror(strerror(errno));
	}

	char * resp = malloc(sizeof(int) + sizeof(unsigned long));
	memcpy(resp, &written, sizeof(int));
	memcpy(resp + sizeof(int), &hash, sizeof(unsigned long));
	send(cfd, resp, sizeof(int) + sizeof(unsigned long), 0);
	free(data);
	free(resp);
}

void net_readdir(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	DIR * dp = opendir(temp);
	int ret = 0;
	struct dirent *de = readdir(dp);
	if (de == 0){
		send(cfd, &ret, sizeof(int), 0);
	}
	do {
		if (de->d_name[0] != '.'){
			char * name = malloc(strlen(de->d_name) + sizeof(int));
			int size = strlen(de->d_name);
			memcpy(name, &size, sizeof(int));
			memcpy(name + sizeof(int), de->d_name, strlen(de->d_name) );
			send(cfd, name, strlen(de->d_name) + sizeof(int), 0);
			free(name);
		}

    } while ((de = readdir(dp)) != NULL);

    send(cfd, &ret, sizeof(int), 0);	
    free(data);
}

void net_hostwap_storage(int cfd, char * buf, int type, char * mountpoint){
	int length = *(int*)(buf + sizeof(int));
	char path[length + 1];
	strcpy(path, buf + sizeof(int) *2);
	int file = *(int*)(buf + sizeof(int) *2 + length + 1);
	char temp[strlen(mountpoint) + strlen(path)];
	strcpy(temp, mountpoint);
	strcat(temp, path);
	struct stat buffer;   
	if (stat(temp, &buffer) == 0){
		 unlink(temp);
	}
	if (file){
		mknod(temp, 33204, S_IFREG);
	}else{
		mkdir(temp, 509);
	}
}

void net_hotswap_file_content(int cfd, char * buf, int type, char * mountpoint){
	int size = *(int *)(buf + sizeof(int));
	char * path = malloc(size);
	memcpy(path, (buf + sizeof(int) * 2), size);
	path[size] = '\0';
	int data_size = *(int*)(buf + sizeof(int) * 2 + size);
	char temp[strlen(mountpoint) + strlen(path)];
	strcpy(temp, mountpoint);
	strcat(temp, path);
	
	int fd = open(temp, O_WRONLY);
	// printf("buf %s\n",  (buf + sizeof(int) * 3 + size));
	// printf("data size %d\n", data_size);
	write(fd, (buf + sizeof(int) * 3 + size), data_size);
	close(fd);
	unsigned long hash = 5381;
	// compute_hash(temp, &hash);
	hash = hash_djb((unsigned char *)(buf + sizeof(int) * 3 + size), hash);
	// printf("content hash %lu\n", hash);
	if (setxattr(temp, "user.hash", &hash, sizeof(unsigned long), 0) == -1){
		perror(strerror(errno));
	}
	free(path);
}

//0 for dir
//1 for file
static void send_file_packet(int fd, char * path, int type){
	int size = sizeof(int) * 3  + strlen(path) + 1;
	char * msg = malloc(size + sizeof(int) * 2);
	int packet_size = size + sizeof(int);
	memcpy(msg, &packet_size, sizeof(int));
	memcpy(msg + sizeof(int), &size, sizeof(int));
	int syscall_numb = 14;
	memcpy(msg + sizeof(int) * 2, &syscall_numb, sizeof(int));
	int length = strlen(path);
	memcpy(msg + sizeof(int) * 3, &length, sizeof(int));
	memcpy(msg + sizeof(int) * 4, path, length + 1);
	memcpy(msg + sizeof(int) * 4 + length + 1, &type, sizeof(int));

	send(fd, msg, size + sizeof(int) * 2, 0);
	free(msg);
	// printf("send file packet %s\n", path);
}	

static void send_file_content(int cfd, char * full_path, char * path){

	struct stat st;
    stat(full_path, &st);
    int file_length = st.st_size;
    if (file_length == 0)
    	return;
    char * buf = malloc(file_length);
   
    int fd = open(full_path, O_RDONLY);
    read(fd, buf, file_length);
    // printf("file_length %d\n", file_length);
    // printf("read %s\n", buf);
    buf[file_length] = '\0';
    close(fd);
    int size = sizeof(int) * 3 + file_length + strlen(path);
    void * packet = malloc(size + sizeof(int) * 2);
    int packet_size = size + sizeof(int);
    memcpy(packet, &packet_size, sizeof(int));
    memcpy(packet + sizeof(int), &size, sizeof(int));
    int type = 15;
    memcpy(packet + sizeof(int) * 2, &type, sizeof(int));
    int path_length = strlen(path);
    memcpy(packet + sizeof(int) * 3, &path_length, sizeof(int));
    strcpy(packet + sizeof(int) * 4, path);
    memcpy(packet + sizeof(int) * 4 + path_length, &file_length, sizeof(int));
    memcpy(packet + sizeof(int) * 5 + path_length, buf, file_length);
    send(cfd, packet, size + sizeof(int) * 2, 0);
    free(packet);
    free(buf);
}


static void dump_tree(int fd, char * path, char * relative_path){
	DIR *d;
	struct dirent *dir;
	struct stat path_stat;
	d = opendir(path);
	
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0){
				continue;
			}
		
			char temp[strlen(path) + strlen(dir->d_name) + 2];

			strcpy(temp, path);
			strcat(temp, "/");
			strcat(temp, dir->d_name);
			temp[strlen(path) + strlen(dir->d_name) + 1] = '\0';
			stat(temp, &path_stat);
			if (S_ISDIR(path_stat.st_mode)){
				char ret[strlen(relative_path) + strlen(dir->d_name) + 1];
				strcpy(ret, relative_path);
				strcat(ret, dir->d_name);
				strcat(ret, "/");
				send_file_packet(fd, ret, 0);
				dump_tree(fd, temp, ret);

			}else{
				char ret[strlen(relative_path) + strlen(dir->d_name)];
				strcpy(ret, relative_path);
				strcat(ret, dir->d_name);
				send_file_packet(fd, ret, 1);
				send_file_content(fd, temp, ret);
			}
		}
		closedir(d);
	}
}

void net_dump(int fd, char * path){
	dump_tree(fd, path, "/");	
	int length = 0;
	send(fd, &length, sizeof(int), 0);
}

void net_free_directory(int fd, char * path){
	DIR *d;
	struct dirent *dir;
	struct stat path_stat;
	d = opendir(path);
	if (d) {
		while ((dir = readdir(d)) != NULL) {
			if (strcmp(dir->d_name, ".") == 0 || strcmp(dir->d_name, "..") == 0){
				continue;
			}
		
			char temp[strlen(path) + strlen(dir->d_name) + 2];

			strcpy(temp, path);
			strcat(temp, "/");
			strcat(temp, dir->d_name);
			temp[strlen(path) + strlen(dir->d_name) + 1] = '\0';
			stat(temp, &path_stat);
			if (S_ISDIR(path_stat.st_mode)){
				net_free_directory(fd, temp);
			}else{
				unlink(temp);
			}
		}
		
		rmdir(path);
	}
	mkdir(path, 509);
	int done = 1;
	send(fd, &done, sizeof(int), 0);
}

void net_restore_file(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	send_file_packet(cfd, data->path, 1);
	send_file_content(cfd, temp, data->path);
	int length = 0;
	send(cfd, &length, sizeof(int), 0);
	free(data);
	
}

void send_chunk(int cfd, char * buf, int size){
	char * packet = malloc(size + sizeof(int));
	memcpy(packet, &size, sizeof(int));
	if (buf != NULL){
		memcpy(packet + sizeof(int), buf, size);
	}
	
	send(cfd, packet, size + sizeof(int), 0);
	free(packet);

}

void net_send_chunk(int cfd, char * buf, int type, char * mountpoint){
	
	struct msg * data = deserialize_path(buf, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	int n_block = data->size;
	struct stat st;
    stat(temp, &st);
    int file_size = st.st_size;

    int fd = open(temp, O_RDONLY);
    int start = n_block * BLOCK;
    int end = (n_block + 1) * BLOCK;

    if (start < file_size && end <= file_size){
    	char * buf = malloc(BLOCK);
    	pread(fd, buf, BLOCK, start);
    	send_chunk(cfd, buf, BLOCK);
    	free(buf);
    	close(fd);
    	return;
    }
    if (start < file_size){

    	char * buf  = malloc(file_size - start);
    	pread(fd, buf, file_size - start, start);
    	send_chunk(cfd, buf, file_size - start);
    	free(buf);
    	close(fd);
    	return;
    }	
    send_chunk (cfd, NULL, 0);
    close(fd);
}

void net_get_chunk(int cfd, char * buf, int type, char * mountpoint){
	struct msg * data = deserialize_write(buf);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	
	int fd = open(temp, O_WRONLY);
	int cur_offset = data->fd * BLOCK + data->offset;
	int bytes_written = pwrite(fd, data->buf, data->size, cur_offset);
	send(cfd, &bytes_written, sizeof(size_t), 0);

}

int check_zeros(char * buf){
	int i;
	for (i = 0; i < BLOCK; i++){
		if (buf[i] == 0) return i;
	}
	return BLOCK;
}

void net_get_xordfile(int cfd, char * buf, int type, char * mountpoint){
	int size = *(int *)(buf + sizeof(int));
	char * path = malloc(size);
	memcpy(path, (buf + sizeof(int) * 2), size);
	path[size] = '\0';
	int data_size = *(int*)(buf + sizeof(int) * 2 + size);
	char temp[strlen(mountpoint) + strlen(path)];
	strcpy(temp, mountpoint);
	strcat(temp, path);
	struct stat st;
	stat(temp, &st);
	char * new = buf + sizeof(int) * 3 + size;
	int stripe = 2 * BLOCK;
	if (st.st_size == 0){
		int fd = open(temp, O_WRONLY);
		if (data_size == BLOCK)
			write(fd, new, check_zeros(new));
		else
			write(fd, new, data_size);
		close(fd);
	}else{
		
		int fd = open(temp, O_RDWR);
		int chunk_file = st.st_size < BLOCK ? st.st_size : BLOCK;
		int chunk_data = data_size < BLOCK ? data_size : BLOCK;
		int cur_size = st.st_size;
		int i = 0;
		
		do{
			char * chunk = malloc(BLOCK);
			pread(fd, chunk, chunk_file, i * BLOCK);
			char * cur_data = new + i * BLOCK;	
			if (chunk_file != BLOCK){
				// printf("chunk_file %d\n", chunk_file);
				// 
				// printf("chunk %s\n", chunk);
				memset(chunk + chunk_file, 0, BLOCK - chunk_file);
				int j = 0;
				char * xor = malloc(BLOCK);

				for (j = 0; j < BLOCK; j++){
					xor[j] = chunk[j] ^ cur_data[j];
				}
				pwrite(fd, xor, BLOCK, i * BLOCK);
				// printf("xor %s\n", xor);
				free(xor);
			}

			if (chunk_file == chunk_data){
				int j = 0;
				char * xor = malloc(BLOCK);
				
				memset(xor, 0, BLOCK);
				for (j = 0; j < BLOCK; j++){
					xor[j] = chunk[j] ^ cur_data[j];
					if ((xor[j] == chunk[j] || xor[j] == cur_data[j] ||
						(cur_data[j] == chunk[j] && xor[j] == 0)) && st.st_size % stripe == 0) break;
				}
				// printf("xor %s\n", xor);
				// printf("j %d\n", j);
				if (j != 0)
					pwrite(fd, xor, BLOCK, i * BLOCK);
				else{
					truncate(temp, st.st_size - BLOCK);
				}
				free(xor);
			}

			if (chunk_data != BLOCK){
				// printf("%s\n", "data != BLOCK");
				int j;
				char * xor = malloc(BLOCK);
				for (j = 0; j < chunk_data; j++){
					xor[j] = chunk[j] ^ cur_data[j];
					// printf("xor %c chunk %c cur_data %c\n", xor[j], chunk[j], cur_data[j]);
					if ((xor[j] == chunk[j] || xor[j] == cur_data[j] || 
							(cur_data[j] == chunk[j] && xor[j] == 0)) && st.st_size % stripe == 0){
						break;
					}
				}

				while (j < BLOCK && j != 0){
					xor[j] = chunk[j] ^ 0;
					j ++;
				}
			
				if (j != 0){
					pwrite(fd, xor, BLOCK, i * BLOCK);
				}else{
					truncate(temp, st.st_size - BLOCK);
				}
				free(xor);
			}

			cur_size -= chunk_file;
			data_size -= chunk_data;
			chunk_file = cur_size < BLOCK ? cur_size : BLOCK;
			chunk_data = data_size < BLOCK ? data_size : BLOCK;
			i += 1;
		}while(cur_size > 0 && data_size > 0);


		close(fd);	
	}

}


void * serve_client(void * data){
	char * path = *(char **)data;
	int cfd = *(int*)((char*)data + sizeof(char*));
    int received_size;
   
	while(1){
		int data_size;
		received_size = recv(cfd, &data_size, sizeof(int), 0);
        if (received_size <= 0){
            free(data);
    		break;
        }
        if (data_size == 1){
        	send(cfd, &data_size, sizeof(int), 0 );
        	continue;

        }
        if (data_size == -1){
        	printf("%s\n", "net dump");
        	net_dump(cfd, path);
        	continue;
        }
        if (data_size == -2){
        	printf("%s\n", "delete all of it");
        	net_free_directory(cfd, path);
        	continue;
        }
        char * buf = malloc(data_size);
        received_size = recv(cfd, buf, data_size, 0);
        if (received_size <= 0){
            free(data);
    		break;
        }
        int type = *(int*)buf;
		functions[type](cfd, buf, type, path);
		free(buf);
	}
	return NULL;
}

int main(int argc, char const *argv[])
{
	if (argc != 4){
		printf("%s\n", "Not enough arguments");
	}
	char * ip = (char *)argv[1];
	uint port = atoi((char *)argv[2]);
	char * path = (char *)argv[3];

	int sfd;
	struct sockaddr_in addr;
	sfd = socket(AF_INET, SOCK_STREAM, 0);
	int optval = 1;
    setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(int));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_aton(ip, (struct in_addr *)&addr.sin_addr.s_addr);
    bind(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    listen(sfd, BACKLOG);
 
    while(1){
    	struct sockaddr_in peer_addr;
    	socklen_t peer_addr_size = sizeof(struct sockaddr_in);
        int cfd = accept(sfd, (struct sockaddr *) &peer_addr, &peer_addr_size);
        
        pthread_t tid;
        void * auxdata = malloc(sizeof(char*) + sizeof(int));
    	memcpy(auxdata, &path, sizeof(char*));
        memcpy((char*)auxdata + sizeof(char*), &cfd, sizeof(int));
        pthread_create(&tid, NULL, serve_client, auxdata);
        
    }

	return 0;
}