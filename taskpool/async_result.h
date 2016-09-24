#pragma once

#include    "./stdc_common.h"
#include    "./linux_common.h"
#include    "../common/util_list.h"

/*
 * It seems some well-behaved thread pool should return some kind of future as result for the client program.
 * However, I choose to use a simple structure for asynchronized results for there's no need for 
 * asynchronized task argument passing or any form of concurrency system implementation/wrapping.
 * 
 * Tasks are being executed. Promises were made. That's enough for now.
 */

struct clwater_async_result {
    union {
        int int_native;
        uint32_t u32;
        uint64_t u64;
        void *ptr;
        double numeric;
    } value;
    int status;
    uint8_t from;
    struct {
        sem_t semaphore;
    } sync;
    struct link_index recollectable;
};

#define     ASYNC_RESULT_PENDING        0
#define     ASYNC_RESULT_DONE           1

#define     clwater_async_result_write_defn(type, member) \
    static inline \
    void clwater_async_result_write_##member(struct clwater_async_result *result, type member) { \
        result->value.member = member; \
    }
#define     clwater_async_result_write_branch(type, member) \
    type: clwater_async_result_write_##member

clwater_async_result_write_defn(int, int_native)
clwater_async_result_write_defn(uint32_t, u32)
clwater_async_result_write_defn(uint64_t, u64)
clwater_async_result_write_defn(void *, ptr)
clwater_async_result_write_defn(double, numeric)

#define     clwater_async_result_write(result_, value_) \
        _Generic( \
                (value_), \
                clwater_async_result_write_branch(int, int_native), \
                clwater_async_result_write_branch(uint32_t, u32), \
                clwater_async_result_write_branch(uint64_t, u64), \
                clwater_async_result_write_branch(void *, ptr), \
                clwater_async_result_write_branch(double, numeric) \
        )((result_), (value_))

static inline
void clwater_async_result_commit(struct clwater_async_result *result, int status) {
    __atomic_store_n(&(result->status), status, __ATOMIC_RELEASE);
}

static inline
int clwater_async_result_poll(struct clwater_async_result *result) {
    return __atomic_load_n(&(result->status), __ATOMIC_ACQUIRE);
}

static inline
void clwater_async_result_wait(struct clwater_async_result *result) {
    // XXX Assume that a result is used only once.
    if (clwater_async_result_poll(result) == ASYNC_RESULT_DONE)
        return;
    sem_wait(&(result->sync.semaphore));
}

static inline
void clwater_async_result_signal(struct clwater_async_result *result) {
    clwater_async_result_commit(result, ASYNC_RESULT_DONE);
    sem_post(&(result->sync.semaphore));
}

static inline
struct clwater_async_result *clwater_async_result_init(struct clwater_async_result *result, uint8_t from) {
    __atomic_store_n(&(result->status), ASYNC_RESULT_PENDING, __ATOMIC_RELEASE);
    if (sem_init(&(result->sync.semaphore), 0, 0) < 0)
        return NULL;
    result->from = from;
    return result;
}

static inline
struct clwater_asunc_result *clwater_async_result_ruin(struct clwater_async_result *result) {
    sem_destroy(&(result->sync.semaphore));
    __atomic_store_n(&(result->status), ASYNC_RESULT_PENDING, __ATOMIC_RELEASE);
}

static inline
struct clwater_async_result *clwater_async_result_alloc(void) {
    struct clwater_async_result *result = malloc(sizeof(struct clwater_async_result));
    return result;
}

static inline
void clwater_async_result_free(struct clwater_async_result *result) {
    free(result);
}
