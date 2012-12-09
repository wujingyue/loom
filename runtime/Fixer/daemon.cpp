#define _REENTRANT

#include <arpa/inet.h>
#include <cstring>
#include <cstdlib>
#include <netdb.h>
#include <errno.h>
#include <iostream>
#include <string>
#include <sstream>
#include <cassert>
#include <fstream>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/time.h>
#include <ctime>

#include "loom/daemon.h"
#include "loom/fixes.h"
#include "loom/loom.h"

using namespace std;

#define MAX_LEN 1024
#define MAX_N_TRIES 10

#define LOCALHOST "127.0.0.1"
#define CONTROLLER_IP LOCALHOST
#define CONTROLLER_PORT 1221

static int daemon_sock;

static int send_to_controller(const char *msg) {
	int len = htonl(strlen(msg) + 4);
	if (send(daemon_sock, &len, sizeof(int), 0) == -1) {
		perror("send");
		return -1;
	}
	if (send(daemon_sock, msg, strlen(msg), 0) == -1) {
		perror("send");
		return -1;
	}
	return 0;
}

static int recv_from_controller(string &res) {
	fprintf(stderr, "receiving from the controller %d\n", daemon_sock);
	char buffer[MAX_LEN];
	int len;
	if (recv(daemon_sock, &len, sizeof(int), 0) == -1) {
		perror("recv");
		return -1;
	}
	fprintf(stderr, "received len\n");
	len = ntohl(len) - 4;
	if (len >= MAX_LEN) {
		fprintf(stderr, "Message too long: length = %d\n", len);
		return -1;
	}
	if (recv(daemon_sock, buffer, len, 0) == -1) {
		perror("recv");
		return -1;
	}
	buffer[len] = '\0';
	res = string(buffer);
	fprintf(stderr, "received something: %s\n", buffer);
	return 0;
}

static int get_absolute_path(string &path) {
	char buf[MAX_LEN];
	memset(buf, 0, MAX_LEN);
	int count = readlink("/proc/self/exe", buf, MAX_LEN);
	if (count < 0 || count >= MAX_LEN)
		return -1;
	path = string(buf);
	return 0;
}

static int handle_get_name(char *err_msg) {
	string path;
	if (get_absolute_path(path) == -1)
		return -1;
	cerr << "path = " << path << "\n";
	snprintf(err_msg, MAX_LEN, "%s %d", path.c_str(), 0);
	return 0;
}

static int get_checks_and_funcs(const Fix &fix, vector<int> &checks, vector<int> &funcs, char *err_msg) {

	ofstream fout("/tmp/mark-region.in");
	if (!fout) {
		snprintf(err_msg, MAX_LEN, "cannot open mark-region.in");
		return -1;
	}
	for (size_t i = 0; i < fix.ops.size(); ++i) {
		if (fix.ops[i].type == START)
			fout << fix.ops[i].pos << ' ';
	}
	fout << endl;
	for (size_t i = 0; i < fix.ops.size(); ++i) {
		if (fix.ops[i].type == END)
			fout << fix.ops[i].pos << ' ';
	}
	fout << endl;
	fout.close();



#if 1
	int ret;

	struct timeval start, end;
	gettimeofday(&start, NULL);
	if (fork() == 0) {
		// Change
		int input_fd = open("/home/jingyue/Research/bugs/mysql-791/mysqld-inject.bc", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/mysql-169/mysqld.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/mysql-644/mysqld.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/apache-25520/httpd.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/apache-21287/httpd.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/splash2/splash2/codes/kernels/fft/fft.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/splash2/splash2/codes/kernels/lu/non_contiguous_blocks/lu.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/splash2/splash2/codes/apps/barnes/barnes.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/bugs/pbzip2/pbzip2.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/defens-new/loom-bit/tests/clone.bc2", O_RDONLY);
		// int input_fd = open("/home/jingyue/Research/defens-new/loom-bit/tests/race.bc2", O_RDONLY);
		if (input_fd == -1) {
			perror("open");
			exit(-1);
		}
		ret = dup2(input_fd, STDIN_FILENO);
		if (ret == -1) {
			perror("dup2");
			exit(-1);
		}
		ret = execlp("opt",
				"opt",
				"-load",
				"/home/jingyue/Research/llvm/llvm-obj/Release/lib/idm.so",
				"-load",
				"/home/jingyue/Research/llvm/llvm-obj/Release/lib/mark-region.so",
				"-mark-region",
				"-analyze",
				NULL);
		if (ret == -1) {
			perror("execlp");
			exit(-1);
		}
		exit(0);
	}

	int child_status;
	if (wait(&child_status) == -1) {
		perror("wait");
		snprintf(err_msg, MAX_LEN, "error on wait");
		return -1;
	}
	if (child_status < 0) {
		fprintf(stderr, "Mark region failed.\n");
		snprintf(err_msg, MAX_LEN, "mark region failed\n");
		return -1;
	}

	fprintf(stderr, "Mark region succeeded.\n");

	gettimeofday(&end, NULL);

	struct timeval diff;
	timersub(&end, &start, &diff);
	FILE *f_time = fopen("/tmp/mark-region-time", "a");
	fprintf(f_time, "%ld\n", diff.tv_sec * 1000000 + diff.tv_usec);
	fclose(f_time);

#endif


	checks.clear();
	ifstream fin("/tmp/mark-region.out");
	if (!fin) {
		snprintf(err_msg, MAX_LEN, "cannot open mark-region.out");
		return -1;
	}
	int check_id;
	while (fin >> check_id)
		checks.push_back(check_id);
	fin.close();

	funcs.clear();
	fin.open("/tmp/funcs-to-be-patched.out");
	if (!fin) {
		snprintf(err_msg, MAX_LEN, "cannot open funcs-to-be-patched.out");
		return -1;
	}
	int func_id;
	while (fin >> func_id)
		funcs.push_back(func_id);
	fin.close();

	return 0;
}

static int handle_add(int fix_id, const string &file_name, char *err_msg) {
	Fix fix;
	if (read_fix(file_name, fix) == -1) {
		snprintf(err_msg, MAX_LEN, "failed to read the fix");
		return -1;
	}
	if (fix.type != CRITICAL && fix.type != ATOMIC && fix.type != ORDER) {
		snprintf(err_msg, MAX_LEN, "not implemented yet");
		return -1;
	}

	vector<int> checks, funcs;
#if 1
	funcs.push_back(3671);
#endif
#if 0
	if (get_checks_and_funcs(fix, checks, funcs, err_msg) == -1)
		return -1;
	if (fix.type == ORDER)
		checks.clear();

	fprintf(stderr, "deactivating...\n");
	int n_deact = 0, n_tries;
	while (true) {
		n_deact++;
		if (deactivate(checks, n_tries) == 0)
			break;
		fprintf(stderr, "deactivation failed\n");
		activate();
		usleep(10 * 1000);
	}
	fprintf(stderr, "deactivated\n");
#endif

	if (add_fix(fix_id, fix) == -1) {
		snprintf(err_msg, MAX_LEN, "Unable to add fix");
#if 0
		activate();
#endif
		return -1;
	}
	for (size_t i = 0; i < funcs.size(); ++i)
		is_func_patched[funcs[i]] = 1;
#if 0
	activate();
	snprintf(err_msg, MAX_LEN, "OK %d %d", n_deact, n_tries);
#endif

	return 0;
}

int preload_fix(int fix_id, const string &file_name, char *err_msg) {
	Fix fix;
	if (read_fix(file_name, fix) == -1) {
		snprintf(err_msg, MAX_LEN, "failed to read the fix");
		return -1;
	}
	if (fix.type != CRITICAL && fix.type != ATOMIC && fix.type != ORDER) {
		snprintf(err_msg, MAX_LEN, "not implemented yet");
		return -1;
	}

	vector<int> checks, funcs;
	if (get_checks_and_funcs(fix, checks, funcs, err_msg) == -1)
		return -1;
	if (fix.type == ORDER)
		checks.clear();

	if (add_fix(fix_id, fix) == -1) {
		snprintf(err_msg, MAX_LEN, "Unable to add fix");
		activate();
		return -1;
	}
	for (size_t i = 0; i < funcs.size(); ++i)
		is_func_patched[funcs[i]] = 1;
	snprintf(err_msg, MAX_LEN, "OK");

	return 0;
}

static int handle_del(int fix_id, char *err_msg) {
	if (fixes[fix_id].type == UNKNOWN) {
		snprintf(err_msg, MAX_LEN, "unknown fix ID'");
		return -1;
	}
	Fix fix = fixes[fix_id];
	vector<int> checks, funcs;
	if (get_checks_and_funcs(fix, checks, funcs, err_msg) == -1)
		return -1;
	if (fix.type == ORDER)
		checks.clear();

	int n_deact = 0, n_tries;
	while (true) {
		n_deact++;
		if (deactivate(checks, n_tries) == 0)
			break;
		fprintf(stderr, "deactivation failed\n");
		activate();
	}
	if (del_fix(fix_id) == -1) {
		snprintf(err_msg, MAX_LEN, "Unable to del fix");
		activate();
		return -1;
	}
	activate();
	snprintf(err_msg, MAX_LEN, "OK %d %d", n_deact, n_tries);

	return 0;
}

static int create_daemon_socket() {
	int sock = socket(AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		perror("socket");
		return -1;
	}
	struct sockaddr_in svr_addr;
	bzero(&svr_addr, sizeof svr_addr);
	svr_addr.sin_family = AF_INET;
	svr_addr.sin_addr.s_addr = inet_addr(CONTROLLER_IP);
	svr_addr.sin_port = htons(CONTROLLER_PORT);
	if (connect(sock, (struct sockaddr *)&svr_addr, sizeof svr_addr) == -1) {
		perror("Failed to connect to the controller");
		return -1;
	}
	return sock;
}

int handle_client_requests(void *arg) {
#if 0
	char file_name[1024];
	sprintf(file_name, "/tmp/log.%d", getpid());
	FILE *flog = fopen(file_name, "w");
	fprintf(flog, "handle_client_requests pid = %d, parent = %d\n", getpid(), getppid());
	fclose(flog);
#endif
	int ret = prctl(PR_SET_NAME, "daemon", 0, 0, 0);
	assert(ret == 0);

	daemon_sock = create_daemon_socket();
	if (daemon_sock == -1)
		return -1;
	sleep(1);
	fprintf(stderr, "daemon_sock after clone = %d\n", daemon_sock);
	while (true) {
		string msg;
		if (recv_from_controller(msg) == -1)
			break;
		istringstream stin(msg);
		string str_cmd;
		if (!(stin >> str_cmd)) {
			cerr << "Error: No command\n";
			continue;
		}
		char err_msg[MAX_LEN] = {0};
		if (str_cmd == "get_name") {
			handle_get_name(err_msg);
		} else if (str_cmd == "add") {
			int fix_id;
			string file_name;
			if (!(stin >> fix_id >> file_name)) {
				snprintf(err_msg, MAX_LEN, "format error: add <fix_id> <file_name>");
			} else {
				handle_add(fix_id, file_name, err_msg);
			}
		} else if (str_cmd == "del") {
			int fix_id;
			if (!(stin >> fix_id)) {
				snprintf(err_msg, MAX_LEN, "format error: del <fix_id>");
			} else {
				handle_del(fix_id, err_msg);
			}
		} else {
			snprintf(err_msg, MAX_LEN, "Unknown command: %s", str_cmd.c_str());
			cerr << "Error: Unknown command: " << str_cmd << "\n";
		}
		if (send_to_controller(err_msg) == -1) {
			perror("send");
			break;
		}
		// fprintf(stderr, "send succeed\n");
	}
	while (true) {
		sleep(20);
	}
	// never reaches here
	assert(false);
	return 0;
}


