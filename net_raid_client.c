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
#include <time.h>
#include <sys/stat.h>
#include "fuse_raid_1.h"

#define BYTES 1048576

typedef struct client{
	char * errorlog;
	int cache_size;
	char * cache_replacement;
	int timeout;
}client;

typedef struct mount_info{
	char * diskname;
	char * mountpoint;
	int raid;
	char * servers;
	char * hotswap;
}mount_info;

int parse_file(char * buffer, client * net_client, mount_info ** mount_infos){
	char * saveptr = buffer;
  	char * token = strtok_r(buffer, "\n", &saveptr);
  	mount_info * cur;
  	*mount_infos = malloc(sizeof(struct mount_info));
  	int num_servers = 0;  	
  	if (strcmp(strtok(token, "="), "errorlog ") == 0){
  		char * temp_log = strtok(NULL, "=");
  		net_client->errorlog = malloc(strlen(temp_log));
  		strcpy(net_client->errorlog, temp_log + 1);
  	}
  	for (token = strtok_r(NULL, "\n", &saveptr); token != NULL; token = strtok_r(NULL, "\n", &saveptr)){
  		char * key = strtok(token, "=");
  		if (strcmp(key, "cache_size ") == 0){
  			char * temp_log = strtok(NULL, "=") + 1;
  			net_client->cache_size = atoi(temp_log) * BYTES;
  		}
  		if (strcmp(key, "cache_replacment ") == 0){
  			char * temp_log = strtok(NULL, "=");
	  		net_client->cache_replacement = malloc(strlen(temp_log));
	  		strcpy(net_client->cache_replacement, temp_log + 1);
	  	}
	  	if (strcmp(key, "timeout ") == 0){
  			char * temp_log = strtok(NULL, "=") + 1;
  			net_client->timeout = atoi(temp_log);
  		}
  		if (strcmp(key, "diskname ") == 0){
  			cur = malloc(sizeof(struct mount_info));	
  			char * temp_log = strtok(NULL, "=") + 1;
  			cur->diskname = malloc(strlen(temp_log));
	  		strcpy(cur->diskname, temp_log);
  		}
  		if (strcmp(key, "mountpoint ") == 0){
  			char * temp_log = strtok(NULL, "=") + 1;
  			cur->mountpoint = malloc(strlen(temp_log));
	  		strcpy(cur->mountpoint, temp_log);
  		}
  		if (strcmp(key, "raid ") == 0){
  			char * temp_log = strtok(NULL, "=") + 1;
  			cur->raid = atoi(temp_log);
  		}
  		if (strcmp(key, "servers ") == 0){
  			char * temp_log = strtok(NULL, "=");
	  		cur->servers = malloc(strlen(temp_log));
	  		strcpy(cur->servers, temp_log + 1);
	  	}
	  	if (strcmp(key, "hotswap ") == 0){
  			char * temp_log = strtok(NULL, "=");
	  		cur->hotswap = malloc(strlen(temp_log));
	  		strcpy(cur->hotswap, temp_log + 1);
	  		memcpy((char*)(*mount_infos) + num_servers * sizeof(struct mount_info), 
  					cur, sizeof(struct mount_info));
	  		free(cur);
  			num_servers += 1;
  			*mount_infos = realloc(*mount_infos, (num_servers+1) * sizeof(struct mount_info));
	  	}


  	}
  	return num_servers;
}


int read_file(char * config_file, client * net_client, mount_info ** mount_infos){

	int fd = open(config_file, O_RDONLY);
	if (fd == -1){
		exit(1);
	}
    struct stat st;
    stat(config_file, &st);
    int size = st.st_size;
    char * buffer = malloc(size);
	if (read(fd, buffer, size) == -1){
		exit(1);
	}
	int mount_numb = parse_file(buffer, net_client, mount_infos);
	free(buffer);
	close(fd);
	return mount_numb;
}

static int ip_port_parser(char * servers, addresses ** ip_ports){
	*ip_ports = malloc(sizeof(struct addresses));
	char * saveptr = servers;
  	char * token = strtok_r(servers, ",", &saveptr);
  	char * ip = strtok(token, ":");
  	(*ip_ports)->ip = malloc(strlen(ip) + 1);
  	strcpy((*ip_ports)->ip, ip);
  	int port = atoi(strtok(NULL, ":"));
  	(*ip_ports)->port = port;
  	int n = 1;
  	for (token = strtok_r(NULL, "\n", &saveptr); token != NULL; token = strtok_r(NULL, "\n", &saveptr)){
  		*ip_ports = realloc(*ip_ports, (n + 1) * sizeof(struct addresses));
  		char * ip = strtok(token, ":")+1;
	  	(*ip_ports)[n].ip = malloc(strlen(ip) + 1);
	  	strcpy((*ip_ports)[n].ip, ip);
	  	int port = atoi(strtok(NULL, ":"));
	  	(*ip_ports)[n].port = port;
  		n += 1;
  	}

  	return n;
}

void raid_1(mount_info * info, client * client){
	addresses * ip_ports = NULL;
	addresses * swap_ip_port = NULL;
    time_t current_time;
	int numb_connections = ip_port_parser(info->servers, &ip_ports);
	int swap_connections = ip_port_parser(info->hotswap, &swap_ip_port);
	int sfd[numb_connections+swap_connections];
	struct sockaddr_in addr[numb_connections + swap_connections];

	int i;
    int con;
	for (i = 0; i < numb_connections; ++i){
		sfd[i] = socket(AF_INET, SOCK_STREAM, 0);

    	addr[i].sin_family = AF_INET;
	    addr[i].sin_port = htons(ip_ports[i].port);
	    inet_aton(ip_ports[i].ip, (struct in_addr *)&addr[i].sin_addr.s_addr);
	    con = connect(sfd[i], (struct sockaddr *) &addr[i], sizeof(struct sockaddr_in));
      if (con == 0){
        current_time = time(NULL);
        printf("[%s] %s %s:%d %s\n", strtok(ctime(&current_time), "\n"), info->diskname, ip_ports[i].ip,
            ip_ports[i].port, "open connection");
      }
	}
    
	//hot swap connection
	sfd[i] = socket(AF_INET, SOCK_STREAM, 0);
	addr[i].sin_family = AF_INET;
    addr[i].sin_port = htons(swap_ip_port->port);
    inet_aton(swap_ip_port->ip, (struct in_addr *)&addr[i].sin_addr.s_addr);
    con = connect(sfd[i], (struct sockaddr *) &addr[i], sizeof(struct sockaddr_in));
    if (con == 0){
        current_time = time(NULL);
        printf("[%s] %s %s:%d %s\n", strtok(ctime(&current_time), "\n"), info->diskname, swap_ip_port->ip,
                swap_ip_port->port, "hotswap connected");
    }

    int fd = open(client->errorlog, O_RDWR);
    if (fd == -1){
        printf("Can't open errorlog file %s\n", client->errorlog);
    }
    
    struct auxdata * data = malloc(sizeof(struct auxdata));
    data->fds = sfd;
    data->errorlog = fd;
    data->timeout = client->timeout;
    data->diskname = info->diskname;
    data->ip_ports = ip_ports;
    data->swap_ip_port = swap_ip_port;

    init_filesys(info->mountpoint, data);

}

void raid_5(mount_info * info, client * client){
	printf("%s\n", "raid_5");
}


int main(int argc, char const *argv[])
{
	if (argc != 2){
		printf("%s\n", "Config file not found");
		return 1;
	}
	//deal with servers
	client * net_client = malloc(sizeof (struct client));
	mount_info * mount_infos = NULL;
	char * config_file = (char*)argv[1];
	int mount_numb = read_file(config_file, net_client, &mount_infos);
	pid_t child_id, wpid;
    int status = 0;
    int i;
  	for(i = 0; i < mount_numb; i++){
  		switch(child_id = fork()) {
            case -1:
                exit(100);
            case 0:
            	if (mount_infos[i].raid == 1)
                	raid_1(&mount_infos[i], net_client);
                else
                	raid_5(&mount_infos[i], net_client);
                exit(0);
            default:
            	continue;          
        }
  	}
  	while ((wpid = wait(&status)) > 0);
    printf("%s\n", "Parent process exited");
  	free(net_client);
  	free(mount_infos);
	 return 0;
}