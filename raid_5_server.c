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
#define FUNC_NUMB 17

void raid_get_attr(int cfd, char * buf, int type, char * path);
void raid_open(int cfd, char * buf, int type, char * path);
void raid_read(int cfd, char * buf, int type, char * path);
void raid_close(int cfd, char * buf, int type, char * path);
void raid_rename(int cfd, char * buf, int type, char * path);
void raid_unlink(int cfd, char * buf, int type, char * path);
void raid_rmdir(int cfd, char * buf, int type, char * path);
void raid_mkdir(int cfd, char * buf, int type, char * path);
void raid_write(int cfd, char * buf, int type, char * path);
void raid_opendir(int cfd, char * buf, int type, char * path);
void raid_closedir(int cfd, char * buf, int type, char * path);
void raid_create(int cfd, char * buf, int type, char * path);
void raid_truncate(int cfd, char * buf, int type, char * path);
void raid_readdir(int cfd, char * buf, int type, char * path);
void raid_hostwap_storage(int cfd, char * buf, int type, char * path);
void raid_hotswap_file_content(int cfd, char * buf, int type, char * path);
void raid_restore_file(int cfd, char * buf, int type, char * path);

typedef void (*raid_syscall)(int cfd, char * buf, int type, char * path);

static raid_syscall syscalls[FUNC_NUMB] = {raid_get_attr, raid_open, raid_read, raid_close,
	raid_rename, raid_unlink, raid_rmdir,  raid_mkdir, raid_write, raid_opendir, 
	raid_closedir, raid_create, raid_truncate, raid_readdir, raid_hostwap_storage,
	raid_hotswap_file_content, raid_restore_file};


void net_get_attr(int cfd, char * buf_data, int type, char * mountpoint){
	struct msg * data = deserialize_path(buf_data, type);
	char temp[strlen(mountpoint) + strlen(data->path)];
	strcpy(temp, mountpoint);
	strcat(temp, data->path);
	struct stat buf;
	int res = stat(temp, &buf);
	printf("%s\n", temp);
	char * data_to_send = malloc(sizeof(int) + sizeof(struct stat));
	memcpy(data_to_send, &res, sizeof(int));
	memcpy(data_to_send + sizeof(int), &buf, sizeof(struct stat));
	send(cfd, data_to_send, sizeof(struct stat) + sizeof(int), 0);
	free(data_to_send);
	free(data);

}

void * get_syscall_request(void * data){
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

        char buf[data_size];
        received_size = recv(cfd, buf, data_size, 0);
        if (received_size <= 0){
            free(data);
    		break;
        }
        int type = *(int*)buf;
		syscalls[type](cfd, buf, type, path);
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
        pthread_create(&tid, NULL, get_syscall_request, auxdata);
        
    }


	return 0;
}
