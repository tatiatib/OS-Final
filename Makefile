all: net_raid_client.c  net_raid_server.c fuse_raid_1.c sha.c
	gcc -g -Wall -o  net_raid_client net_raid_client.c fuse_raid_1.c `pkg-config fuse --cflags --libs` 
	gcc -g -Wall -o  net_raid_server net_raid_server.c -lpthread  
clean: 
	$(RM) net_raid_client
	$(RM) net_raid_server