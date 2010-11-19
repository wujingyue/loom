#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
#include <set>
#include <ext/hash_map>
#include <ext/hash_set>
#include <cassert>

#include <netdb.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include<dirent.h>
#include <sys/types.h>   
#include <sys/stat.h> 
#include <stdlib.h>
#include <netinet/tcp.h>
#include <string.h>
#include <sys/socket.h>

#include "../constants.h"
#include "controller.h"

using namespace std;
using namespace __gnu_cxx;

#define DEBUG
//#undef DEBUG
#define MAX_PENDING 20

typedef hash_map<const char *, App_Fix_Grp *> App_To_Fix_Map;
App_To_Fix_Map app_with_fixes;
set<string> app_name_set;
static int controller_sock;
bool need_run_accept = true;

// Even the other side has closed the socket, send() will succeed. We can use PEEK in recv() to
// test whether the socket is closed at the other side.
bool is_sock_open(int sock)
{
	struct tcp_info  optval;
	socklen_t len = sizeof optval;
	
	getsockopt(sock, IPPROTO_TCP,TCP_INFO, &optval, &len);
	if (optval.tcpi_state != TCP_ESTABLISHED) {
		close(sock);
		return false;
	}
	else
		return true;
}

bool is_daemon_conn(int sock)
{
	char buffer[MAX_LEN] = {0};
	int len;

	usleep(100000);
	if (recv(sock, &len, sizeof(int), MSG_DONTWAIT | MSG_PEEK) == -1) {
		dbg_print("is_daemon_conn1\n");
		return true;
	} else {
		dbg_print("NOT is_daemon_conn1 %s\n", buffer);
		return false;
	}
}

static int send_to(int sock, const char *msg)
{
	int len = htonl(strlen(msg) + 4);
	if (!is_sock_open(sock)) {
		fprintf(stderr, "error in is_sock_open\n");
		return -1;
	}
	if (send(sock, &len, sizeof(int), 0) == -1) {
		perror("send");
		return -1;
	}
	if (send(sock, msg, strlen(msg), 0) == -1) {
		perror("send");
		return -1;
	}
	return 0;
}

int accept_connection()
{
	struct sockaddr_in from_addr;
	socklen_t from_addr_len = sizeof from_addr;
	int sock;

		dbg_print("Waiting for connection........................................................................\n");	
	if ((sock = accept(controller_sock, (struct sockaddr *)&from_addr, &from_addr_len)) < 0)
		perror("accept");
	fprintf(stderr, "accept %d\n", sock);
	return sock;
}

static string recv_from(int sock)
{
	char buffer[MAX_LEN] = {0};
	int len;

	dbg_print("Waiting for msg at recv_from()........................................................................\n");
	if (recv(sock, &len, sizeof(int), 0) == -1) {
		perror("recv");
		return "";
	}
	fprintf(stderr, "received len\n");
	len = ntohl(len) - 4;
	if (len >= MAX_LEN) {
		fprintf(stderr, "Message too long: length = %d\n", len);
		return "";
	}
	if (recv(sock, buffer, len, 0) == -1) {
		perror("recv");
		return "";
	}
	
	dbg_print("Controller recvs msg: (%s) from sock %d\n", buffer, sock);
	buffer[len] = '\0';
	return string(buffer);
}

static int create_controller_socket()
{
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	int opt = 1;
	if (sock == -1) {
		perror("socket");
		return -1;
	}
	struct sockaddr_in svr_addr;
	bzero(&svr_addr, sizeof svr_addr);
	svr_addr.sin_family = AF_INET;
	svr_addr.sin_addr.s_addr = inet_addr(CONTROLLER_IP);
	svr_addr.sin_port = htons(CONTROLLER_PORT);
	setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt,sizeof(opt));
	if (bind(sock, (struct sockaddr *)&svr_addr, sizeof svr_addr) == -1) {
		perror("Failed to create controller socket");
		return -1;
	} else
		fprintf(stderr, "Succeed to create controller socket\n");

	if (listen(sock, MAX_PENDING) == -1)
		perror("Failed to set max pending");
	else
		fprintf(stderr, "Succeed to set max pending\n");

	return sock;
}

void print_app_with_fixes()
{
	App_To_Fix_Map::iterator atf_itr = app_with_fixes.begin();
	while (atf_itr != app_with_fixes.end()) {
		dbg_print("print_app_with_fixes(%s, %d)\n", atf_itr->first, atf_itr->second->GetNumOfChanges());
		atf_itr++;
	}	
}


void init_app_fix_grp(string app_path)
{
	set<string>::iterator app_path_itr = app_name_set.find(app_path);
	if (app_path_itr == app_name_set.end()) {
		app_name_set.insert(app_path);
		app_path_itr = app_name_set.find(app_path);
	}

	if (app_with_fixes.find((*app_path_itr).c_str()) == app_with_fixes.end()) {
		dbg_print("Create new fix group for app: %s.\n", (*app_path_itr).c_str());
		app_with_fixes[(*app_path_itr).c_str()] = new App_Fix_Grp();
	}
}

App_Fix_Grp *get_app_with_fixes(string &app_path)
{
	App_To_Fix_Map::iterator atf_itr = app_with_fixes.begin();
	while (atf_itr != app_with_fixes.end()) {
		if (app_path == atf_itr->first)
			return atf_itr->second;
		atf_itr++;
	}
	init_app_fix_grp(app_path);
	atf_itr = app_with_fixes.begin();
	while (atf_itr != app_with_fixes.end()) {
		if (app_path == atf_itr->first)
			return atf_itr->second;
		atf_itr++;
	}
	assert(false);
}




int handle_daemon_get_name(int daemon_sock, string &app_path)
{	
	App_Fix_Grp *afp;
	int num_of_changes;

	dbg_print("handle_daemon_get_name sends get_name cmd to daemon sock %d\n", daemon_sock);
	if (send_to(daemon_sock, "get_name") == -1) {
		return -1;
	}
	dbg_print("sends succeed\n");
	istringstream stin(recv_from(daemon_sock));
	if (!(stin >> app_path >> num_of_changes)) {
		fprintf(stderr, "Need <app_path> <num_of_changes>\n");
		return -1;
	}

	init_app_fix_grp(app_path);	
	afp = get_app_with_fixes(app_path);
	afp->GetDaemonSocks().insert(daemon_sock);
	
	return num_of_changes;
}

int handle_daemon_start(int daemon_sock)
{
	App_Fix_Grp *afp;
 	string resp_msg;
	string app_path;
	char err_msg[MAX_LEN] = {0};
	int num_of_changes;
	int fix_id;
	int ret = 0;
	int i = -1;

	fprintf(stderr, "handle_daemon_start\n");
	if ((num_of_changes = handle_daemon_get_name(daemon_sock, app_path)) == -1)
		return -1;
	afp = get_app_with_fixes(app_path);

	// Restore fixes to newly setup processes.
	Version_Cnt_To_Op_Map::iterator vct_itr = afp->GetVerToOp().begin();
	while (vct_itr != afp->GetVerToOp().end()) {
		if (++i < num_of_changes) {
			vct_itr++;
			continue;
		}
		fix_id = vct_itr->second->GetFixID();
		if (afp->GetMap().find(fix_id) == afp->GetMap().end()) {
			vct_itr++;
			continue;
		}
			
		memset(err_msg, 0, sizeof err_msg);
		snprintf(err_msg, MAX_LINE, "add  %d %s", fix_id, afp->GetMap()[fix_id]->GetFileName().c_str());
		dbg_print("Going to restore fix to daemon: %s\n", err_msg);
		if (send_to(daemon_sock, err_msg) == -1) {
			fprintf(stderr, "Fail to restore fix %d to daemon sock %d", fix_id, daemon_sock);
			vct_itr++;
			continue;
		}
		if ((resp_msg = recv_from(daemon_sock)).compare(0, 2, "OK") != 0) {
			fprintf(stderr, "Fail to add fix %d to newly setup daemon sock %d", fix_id, daemon_sock);
			vct_itr++;
			ret = -1;
			continue;
		}
		vct_itr++;
	}
	return ret;
}

int handle_client_add(int client_sock, string &app_path, string &file_path)
{
	App_Fix_Grp *afp = get_app_with_fixes(app_path);
	int fix_id = -1;
	char err_msg[MAX_LINE] = {0};
	string resp_msg;
	bool succeed = true;

	if (afp == NULL) {
		snprintf(err_msg, MAX_LINE, "The app_path %s is wrong\n", app_path.c_str());
		send_to(client_sock, err_msg);
		close(client_sock);
		return -1;
	}

	// Enter a while loop to send fixes to all application processes.
	fix_id = *(afp->GetIdleFixIds().begin());
	if (afp->GetMap().find(fix_id) == afp->GetMap().end()) {
		dbg_print("Recv add command, adding new fix_id %d\n", fix_id);
		afp->GetMap()[fix_id] = new Fix_Info(file_path);
		afp->GetMap()[fix_id]->SetClientSock(client_sock);
	}
	afp->GetIdleFixIds().erase(fix_id);
 	
	snprintf(err_msg, MAX_LINE, "add %d %s", fix_id, file_path.c_str());
	set<int>::iterator set_itr = afp->GetDaemonSocks().begin();
	set<int> deleted_socks;
	while (set_itr != afp->GetDaemonSocks().end()) {
		dbg_print("Recv add command, send \"%s\" with fix_id %d to daemon daemon sock %d\n", err_msg, fix_id, *set_itr);
		if (send_to(*set_itr, err_msg) == -1) {
			deleted_socks.insert(*set_itr);
			set_itr++;
			continue;
		}
		if ((resp_msg = recv_from(*set_itr)).compare(0, 2, "OK") != 0) {
			fprintf(stderr, "Fail to add fix %d to existing daemon sock %d", fix_id, *set_itr);
			set_itr++;
			succeed = false;
			continue;
		}
		set_itr++;
	}
	// After traversing, delete inactive daemon sockets.
	set_itr = deleted_socks.begin();
	while (set_itr != deleted_socks.end()) {
		afp->GetDaemonSocks().erase(*set_itr);
		set_itr++;
	}
	afp->GetVerToOp()[afp->IncNumOfChanges()] = new Version_Op("add", fix_id);

	// Send response back to client.
	memset(err_msg, 0, sizeof err_msg);
	if (succeed)
		snprintf(err_msg, MAX_LINE, "Succeed to add fix %d", fix_id);
	else
		snprintf(err_msg, MAX_LINE, "Fail to add fix %d", fix_id);
	if (send_to(client_sock, err_msg) == -1) {
		return -1;
	}
	close(client_sock);

	return 0;
}

int handle_client_del(int client_sock, string &app_path, int fix_id)
{
	App_Fix_Grp *afp = get_app_with_fixes(app_path);
	char err_msg[MAX_LINE] = {0};
	string resp_msg;
	bool succeed = true;

	if (afp == NULL) {
		snprintf(err_msg, MAX_LINE, "The app_path %s is wrong\n", app_path.c_str());
		send_to(client_sock, err_msg);
		close(client_sock);
		return -1;
	}

	if (afp->GetMap().find(fix_id) != afp->GetMap().end()) {
		// Send fix ID to daemons. 
		afp->GetMap()[fix_id]->SetClientSock(client_sock);
		snprintf(err_msg, MAX_LINE, "del %d", fix_id);
		set<int>::iterator set_itr = afp->GetDaemonSocks().begin();
		set<int> deleted_socks;
		while (set_itr != afp->GetDaemonSocks().end()) {
			if (send_to(*set_itr, err_msg) == -1) {
				deleted_socks.insert(*set_itr);
				set_itr++;
				continue;
			}
			if ((resp_msg = recv_from(*set_itr)).compare(0, 2, "OK") != 0) {
				fprintf(stderr, "Fail to add fix %d to existing daemon sock %d", fix_id, *set_itr);
				set_itr++;
				succeed = false;
				continue;
			}
			set_itr++;
		}
		// After traversing, delete inactive daemon sockets.
		set_itr = deleted_socks.begin();
		while (set_itr != deleted_socks.end()) {
			afp->GetDaemonSocks().erase(*set_itr);
			set_itr++;
		}
		afp->GetVerToOp()[afp->IncNumOfChanges()] = new Version_Op("del", fix_id);
		delete afp->GetMap()[fix_id];
		afp->GetMap().erase(fix_id);
	} else {
		// Send a note to client.
		snprintf(err_msg, MAX_LINE, "The fix %d does not exist\n", fix_id);
		if (send_to(client_sock, err_msg) == -1)
			return -1;
	}
	
	// Send response back to client.
	memset(err_msg, 0, sizeof err_msg);
	if (succeed)
		snprintf(err_msg, MAX_LINE, "Succeed to delete fix %d", fix_id);
	else
		snprintf(err_msg, MAX_LINE, "Fail to delete fix %d", fix_id);
	if (send_to(client_sock, err_msg) == -1) {
		return -1;
	}
	close(client_sock);

	return 0;
}

void handle_exit()
{
	// Close all daemon sockets that connect to controller.
	App_To_Fix_Map::iterator atf_itr = app_with_fixes.begin();
	while (atf_itr != app_with_fixes.end()) {
		set<int>::iterator set_itr = atf_itr->second->GetDaemonSocks().begin();
		while (set_itr != atf_itr->second->GetDaemonSocks().end()) {
			close(*set_itr);
			set_itr++;
		}
		delete atf_itr->second;
		atf_itr++;
	}	
	app_with_fixes.clear();
	app_name_set.clear();
}

void start_controller_for_daemons()
{
	set<string> app_name_set;

	if ((controller_sock = create_controller_socket()) == -1)
		return;

	while (true) {
		print_app_with_fixes();
		int daemon_sock = accept_connection();
		if (is_daemon_conn(daemon_sock)) {
			handle_daemon_start(daemon_sock);
			continue;
		}
		string msg = recv_from(daemon_sock);
		istringstream stin(msg);
		string app_name;
		string cmd;
		set<string>::iterator app_itr;
		int ret;

		if (!(stin >> cmd)) {
			fprintf(stderr, "Error: No command\n");
			continue;
		}
		
		// exit, add and del all come from clients.
		if (cmd == "exit") {
			handle_exit();
			return;
		}
		if (cmd == "add") {
			// Format: add <application path> <fix file path>
			string app_path;
			string fix_file_path;

			if (!(stin >> app_path >> fix_file_path)) {
				fprintf(stderr, "Need app_path and fix_file_path\n");
				continue;
			}			
			ret = handle_client_add(daemon_sock, app_path, fix_file_path);
		} else if (cmd == "del") {
			// Format: del <application path> <fix id>
			string app_path;
			int fix_id;

			if (!(stin >> app_path >> fix_id)) {
				fprintf(stderr, "Need app_path and fix_file_path\n");
				continue;
			}
			ret = handle_client_del(daemon_sock, app_path, fix_id);	// For both add and del.
		} else {
			fprintf(stderr, "Invalid cmd %s\n", cmd.c_str());
		}
	}
}

int main(int argc, char *argv[])
{
	start_controller_for_daemons();
	return 0;
}



