#ifndef __SYNC_H
#define __SYNC_H

#include <pthread.h>

typedef volatile long spin_rwlock_t;
// typedef pthread_rwlock_t spin_rwlock_t;
typedef volatile long atomic_t;

#define USE_ATOMIC

#ifdef USE_ATOMIC

inline int atomic_dec(atomic_t *v) {
	return __sync_sub_and_fetch(v, 1);
}

inline int atomic_inc(atomic_t *v) {
	return __sync_add_and_fetch(v, 1);
}

inline int atomic_add(int i, atomic_t *v) {
	return __sync_add_and_fetch(v, i);
}

inline int atomic_sub_and_test(int i, atomic_t *v) {
	return __sync_sub_and_fetch(v, i) == 0;
}

#else // USE_ATOMIC

inline int atomic_dec(atomic_t *v) {
	atomic_t old = *v;
	--(*v);
	return old;
}

inline int atomic_inc(atomic_t *v) {
	atomic_t old = *v;
	++(*v);
	return old;
}

inline int atomic_add(int i, atomic_t *v) {
	atomic_t old = *v;
	*v += i;
	return old;
}

inline int atomic_sub_and_test(int i, atomic_t *v) {
	*v--;
	return *v == 0;
}

#endif // USE_ATOMIC

inline void spin_rwlock_init(spin_rwlock_t *v) {
	// pthread_rwlock_init(v, NULL);
	*v = 0x01000000;
}

inline int spin_read_trylock(spin_rwlock_t *v) {
	atomic_dec(v);
	if (*v >= 0)
		return 1;
	atomic_inc(v);
	return 0;
}

inline int spin_write_trylock(spin_rwlock_t *v) {
	if (atomic_sub_and_test(0x01000000, v))
		return 1;
	atomic_add(0x01000000, v);
	return 0;
}

inline void spin_read_lock(spin_rwlock_t *v) {
	// pthread_rwlock_rdlock(v);
	while (!spin_read_trylock(v));
}

inline void spin_write_lock(spin_rwlock_t *v) {
	// pthread_rwlock_wrlock(v);
	while (!spin_write_trylock(v));
}

inline void spin_read_unlock(spin_rwlock_t *v) {
	// pthread_rwlock_unlock(v);
	atomic_inc(v);
}

inline void spin_write_unlock(spin_rwlock_t *v) {
	// pthread_rwlock_unlock(v);
	atomic_add(0x01000000, v);
}

#endif // USE_ATOMIC
