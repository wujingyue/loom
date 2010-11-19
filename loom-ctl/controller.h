#include <vector>
#include <set>
#include <ext/hash_map>
#include <ext/hash_set>
#include <fstream>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include "../constants.h"

using namespace std;
using namespace __gnu_cxx;

#define DEBUG_CONTROLLER
#ifdef DEBUG_CONTROLLER
#        define dbg_print(args...) fprintf(stderr, args)
#else
#        define dbg_print(args...)
#endif

class Fix_Info;
class Version_Op;

typedef hash_map<int, Fix_Info *> Fix_Id_To_Active_Socks_Map;
typedef hash_map<int, Version_Op *> Version_Cnt_To_Op_Map;

// When the controller recvs an add or del request from client, append this struct to App_Fix_Grp.
class Version_Op {
public:
	Version_Op(const char *operation, int id) {
		op = &(operation[0]);
		fix_id = id;
	}
	string& GetOp() {
		return op;
	}
	int GetFixID() {
		return fix_id;
	}
private:
	string op;
	int fix_id;
};

// The per-fix info.
class Fix_Info {
public:
	Fix_Info (string f_name) {
		file_name = f_name;
		client_sock = -1;
	}
	string GetFileName() { return file_name; }
	void SetFileName(string f_name) { file_name = f_name; }
	int GetClientSock() {
		return client_sock;
	}
	void SetClientSock(int sock) {
		client_sock = sock;
	}
	virtual ~Fix_Info() {
	}
private:
	string file_name;
	int client_sock;
};

// The per-application fix group.
class App_Fix_Grp {
public:
	App_Fix_Grp() {
		for (int i = 0; i < MAX_N_FIXES; i++)
			idle_fix_ids.insert(i);
		num_of_changes = 0;
	}
	int IncNumOfChanges() {
		num_of_changes++;
		return (num_of_changes  - 1);
	}
	int GetNumOfChanges() {
		return num_of_changes;
	}
	Version_Cnt_To_Op_Map& GetVerToOp() {
		return ver_to_op;
	}
	set<int>& GetIdleFixIds() {
		return idle_fix_ids;
	}
	set<int>& GetDaemonSocks() {
		return active_daemon_socks;
	}
	Fix_Id_To_Active_Socks_Map& GetMap() {
		return fix_id_with_active_socks;
	}
	virtual ~App_Fix_Grp() {
		Version_Cnt_To_Op_Map::iterator vct_itr = ver_to_op.begin();
		while (vct_itr != ver_to_op.end()) {
			delete vct_itr->second;
			vct_itr++;
		}
 		ver_to_op.clear();
		
 		idle_fix_ids.clear();
 		active_daemon_socks.clear();

		Fix_Id_To_Active_Socks_Map::iterator fit_itr = fix_id_with_active_socks.begin();
		while (fit_itr != fix_id_with_active_socks.end()) {
			delete fit_itr->second;
			fit_itr++;
		}
 		fix_id_with_active_socks.clear();
	}
private:
	int num_of_changes;
	Version_Cnt_To_Op_Map ver_to_op;		// store all daemon socks and version cnt for each daemon.
	set<int> idle_fix_ids;
	set<int> active_daemon_socks;
	Fix_Id_To_Active_Socks_Map fix_id_with_active_socks;	// store all fixes for this app grp.
};

