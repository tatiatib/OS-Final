#ifndef FUSE_RAID_1_H
#define FUSE_RAID_1_H

typedef struct addresses{
	char * ip;
	int port;
}addresses;

struct auxdata{
	int * fds;
	int fd_numb;
	char * diskname;
	int errorlog;
	addresses * ip_ports;
	addresses * swap_ip_port;
};

void init_filesys(char *, struct auxdata *);
#endif /* FUSE_RAID_1_H */