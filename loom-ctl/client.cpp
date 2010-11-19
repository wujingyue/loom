#include <iostream>
#include <sstream>
#include <fstream>
#include <ext/hash_map>
#include <ext/hash_set>
#include <vector>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "constants.h"

#define SOCK_TIMEOUT 10

using namespace std;
using namespace __gnu_cxx;
int controller_sock;

static int send_to_controller(const char *msg)
{
	int len = htonl(strlen(msg) + 4);
	if (send(controller_sock, &len, sizeof(int), 0) == -1)
		return -1;
	if (send(controller_sock, msg, strlen(msg), 0) == -1)
		return -1;
	return 0;
}

static string recv_from_controller()
{
	char buffer[MAX_LEN];
	int len;
	if (recv(controller_sock, &len, sizeof(int), 0) == -1) {
		perror("recv");
		return "";
	}
	len = ntohl(len) - 4;
	if (len >= MAX_LEN) {
		fprintf(stderr, "Message too long: length = %d\n", len);
		return "";
	}
	if (recv(controller_sock, buffer, len, 0) == -1) {
		perror("recv");
		return "";
	}
	buffer[len] = '\0';
	return string(buffer);
}

void print_usage()
{
	fprintf(stderr, "Usage: \nclient <absolute_path_of_app> del <fix_od>\n");
	fprintf(stderr, "Or:\nclient <absolute_path_of_app> add <file_name>\n");
	fprintf(stderr, "Or:\nclient exit (stop the controller)\n");
}

int main(int argc, char * argv[])
{
	struct sockaddr_in s_addr;
	int addr_len;
	socklen_t addr_len2;
	int len;
	char s_buf[1024];
	int timeout = SOCK_TIMEOUT;

	if (argc == 2) {
		if (strcmp(argv[1], "exit")) {
			print_usage();
			exit(0);
		}
	} else if (argc != 4) {
		print_usage();
		exit(0);
	}

	// Create socket
	if ((controller_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		fprintf(stderr, "client creates socket error\n");
		exit(1);
	}
	setsockopt(controller_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(int));

	// Controller process works at local machine, port is 1221.
	s_addr.sin_family = AF_INET;
	s_addr.sin_port = htons(1221);	
	s_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	
	addr_len = sizeof(s_addr);
	addr_len2 = sizeof(s_addr);
	memset(s_buf, 0, sizeof s_buf);

	// add/del operation.
	if (argc == 4) {
		if (strcmp(argv[2], "del") == 0) {
			sprintf(s_buf, "del %s %s", argv[1], argv[3]);
		} else if (strcmp(argv[2], "add") == 0) {
			sprintf(s_buf, "add %s %s", argv[1], argv[3]);
		} else {
			print_usage();
			exit(0);
		}
	}
	
	if (connect(controller_sock, (struct sockaddr *)&s_addr, sizeof s_addr) == -1) {
		perror("Failed to connect to the controller");
		return -1;
	}

	// exit operation. stop the controller.
	if (argc == 2 && strcmp(argv[1], "exit") == 0) {
		fprintf(stderr, "client asks controller to exit\n");
		if (send_to_controller("exit") == -1)
			return -1;
		else
			return 0;
	}
	
	if (send_to_controller(s_buf) == -1)
		return -1;
	fprintf(stderr, "client send request success, len %d, waiting for response from controller process...\n", len);
	fprintf(stderr, "client receives a reply: %s\n", recv_from_controller().c_str());
	close(controller_sock);
	return 0;
}

