#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h> /* superset of previous */
#include <unistd.h>
#include <arpa/inet.h>

int main(int argc, char* argv[])
{
    int sfd;
    struct sockaddr_in addr;
    int ip;
    char buf[1024];
    sfd = socket(AF_INET, SOCK_STREAM, 0);
    inet_pton(AF_INET, "127.0.0.1", &ip);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(5000);
    addr.sin_addr.s_addr = ip;

    connect(sfd, (struct sockaddr *) &addr, sizeof(struct sockaddr_in));
    write(sfd, "qwe", 3);
    read(sfd, &buf, 3);
    sleep(600);
    close(sfd);
}