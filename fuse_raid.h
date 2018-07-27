#ifndef FUSE_RAID_H
#define FUSE_RAID_H

typedef struct addresses{
	char * ip;
	int port;
}addresses;

struct auxdata{
	int * fds;
	int fd_numb;
	int timeout;
	char * diskname;
	int errorlog;
	addresses * ip_ports;
	addresses * swap_ip_port;
};

void init_raid_1(char *, struct auxdata *);
void init_raid_5(char *, struct auxdata *);
#endif /* FUSE_RAID_1_H */