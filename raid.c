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


static int net_getattr(const char* path, struct stat* stbuf){
	return 1;
}

static int net_open(const char* path, struct fuse_file_info* fi){
	return 1;
}

static int net_read(const char* path, char *buf, size_t size, off_t offset, 
	struct fuse_file_info* fi){
	return 0;
}
static int net_release(const char* path, struct fuse_file_info *fi){
	return 0;
}
static int net_rename(const char* from, const char* to){
	return 1;
}

static int net_unlink(const char* path){
	return 1;
}

static int net_rmdir(const char* path){
	return 1;
}

static int net_mkdir(const char* path, mode_t mode){
	return 1;
}

static int net_opendir(const char* path, struct fuse_file_info* fi){
	return 1;

}
static int net_releasedir(const char* path, struct fuse_file_info *fi){
	return 1;
}

static int net_create(const char * path, mode_t modes, struct fuse_file_info * fi){
	return 1;
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
	.opendir	= net_opendir,
	.releasedir = net_releasedir,
	.create		= net_create
};


int main(int argc, char const *argv[])
{
	fuse_main(argc, argv, &net_oper, NULL);
	return 0;
}