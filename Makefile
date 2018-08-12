CC = gcc
# compiler flags:
#  -g    adds debugging information to the executable file
#  -Wall turns on most, but not all, compiler warnings
CFLAGS  = -g -Wall 
DEPS = utils.c fuse_raid_1.c fuse_raid_5.c
DEPS_SERVER = utils_server.c

all: net_raid_client.c net_raid_server.c 
	$(CC) $(CFLAGS) -o net_raid_client net_raid_client.c  $(DEPS) `pkg-config fuse --cflags --libs` -lpthread

	$(CC) $(CFLAGS) -o net_raid_server net_raid_server.c   $(DEPS_SERVER) -lpthread


clean:
	$(RM) net_raid_server
	$(RM) net_raid_client
