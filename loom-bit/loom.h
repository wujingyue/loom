#ifndef __LOOM_H
#define __LOOM_H

#include <vector>
#include "sync.h"
#include "def.h"

// Define array bounds
#define MAX_N_FUNCS (8192)
#define MAX_N_CHECKS (65536)
/*
#define MAX_N_FUNCS (64)
#define MAX_N_CHECKS (512)
*/
#define MAX_N_INSTS (2000000)

// Define the snapshot
#define N_REGISTERS (4)
#define POS_EAX 0
#define POS_EBX 1
#define POS_ECX 2
#define POS_EBP 3

#define likely(x) __builtin_expect((x), 1)
#define unlikely(x) __builtin_expect((x), 0)

/*
 * The names of all hook functions. 
 * Notice that we disable the name mangling because 
 * 1) No two hook functions share the same name
 * 2) The argument types may be different on 32-bit and 64-bit machines
 */
#define LOOM_INIT "loom_init"
#define LOOM_SETUP "loom_setup"
#define LOOM_DISPATCHER "loom_dispatcher"
#define LOOM_ENTER_THREAD_FUNC "loom_enter_thread_func"
#define LOOM_EXIT_THREAD_FUNC "loom_exit_thread_func"
#define LOOM_BEFORE_CALL "loom_before_call"
#define LOOM_AFTER_CALL "loom_after_call"
#define LOOM_IS_FUNC_PATCHED "loom_is_func_patched"
#define LOOM_CHECK "loom_check"
#define LOOM_ENTER_FUNC "loom_enter_func"

extern spin_rwlock_t updating_nthreads;
extern volatile int in_atomic_region;
extern atomic_t executing_basic_func[MAX_N_CHECKS];
extern atomic_t in_check[MAX_N_CHECKS];
extern int daemon_pid;

typedef volatile void *argument_t;
typedef int (*inject_func_t)(argument_t);

extern volatile int is_func_patched[MAX_N_FUNCS];
extern volatile int enabled[MAX_N_CHECKS];
// TODO: multiple callback functions
extern volatile inject_func_t callback[MAX_N_INSTS];
extern argument_t arguments[MAX_N_INSTS];

int start_daemon();
void stop_daemon();
int deactivate(const std::vector<int> &checks, int &n_tries);
int activate();
int __enter_atomic_region();
int __exit_atomic_region();

#endif

