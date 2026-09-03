#include <cassert>

#include "ultramodern/ultramodern.hpp"

static PTR(OSThread) running_queue_impl = NULLPTR;

static PTR(OSThread)* queue_to_ptr(RDRAM_ARG PTR(PTR(OSThread)) queue) {
    if (queue == ultramodern::running_queue) {
        return &running_queue_impl;
    }
    return TO_PTR(PTR(OSThread), queue);
}

void ultramodern::thread_queue_insert(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) toadd_) {
    PTR(OSThread)* cur = queue_to_ptr(PASS_RDRAM queue_);
    OSThread* toadd = TO_PTR(OSThread, toadd_);
    debug_printf("[Thread Queue] Inserting thread %d into queue 0x%08X\n", toadd->id, (uintptr_t)queue_);
    // Real libultra's enqueueThread walks past entries with priority >= the
    // new thread's (not just strictly greater), so a new arrival is inserted
    // AFTER any existing same-priority threads -- FIFO within a priority
    // band. With strictly-greater here, a new same-priority arrival instead
    // cut in front of everything already waiting at that priority, so a
    // thread that repeatedly blocks and re-joins the queue (e.g. a
    // request/response loop) could perpetually starve another same-priority
    // thread that arrived earlier and is just waiting for its turn.
    while (*cur && TO_PTR(OSThread, *cur)->priority >= toadd->priority) {
        cur = &TO_PTR(OSThread, *cur)->next;
    }
    toadd->next = (*cur);
    toadd->queue = queue_;
    *cur = toadd_;

    debug_printf("  Contains:");
    cur = queue_to_ptr(PASS_RDRAM queue_);
    while (*cur) {
        debug_printf("%d (%d) ", TO_PTR(OSThread, *cur)->id, TO_PTR(OSThread, *cur)->priority);
        cur = &TO_PTR(OSThread, *cur)->next;
    }
    debug_printf("\n");
}

PTR(OSThread) ultramodern::thread_queue_pop(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    PTR(OSThread) ret = *queue;
    *queue = TO_PTR(OSThread, ret)->next;
    TO_PTR(OSThread, ret)->queue = NULLPTR;
    debug_printf("[Thread Queue] Popped thread %d from queue 0x%08X\n", TO_PTR(OSThread, ret)->id, (uintptr_t)queue_);
    return ret;
}

bool ultramodern::thread_queue_remove(RDRAM_ARG PTR(PTR(OSThread)) queue_, PTR(OSThread) t_) {
    debug_printf("[Thread Queue] Removing thread %d from queue 0x%08X\n", TO_PTR(OSThread, t_)->id, (uintptr_t)queue_);

    PTR(PTR(OSThread)) cur = queue_;
    while (cur != NULLPTR) {
        PTR(OSThread)* cur_ptr = queue_to_ptr(PASS_RDRAM queue_);
        if (*cur_ptr == t_) {
            *cur_ptr = TO_PTR(OSThread, *cur_ptr)->next;
            return true;
        }
        cur = TO_PTR(OSThread, *cur_ptr)->next;
    }

    return false;
}

bool ultramodern::thread_queue_empty(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    return *queue == NULLPTR;
}

PTR(OSThread) ultramodern::thread_queue_peek(RDRAM_ARG PTR(PTR(OSThread)) queue_) {
    PTR(OSThread)* queue = queue_to_ptr(PASS_RDRAM queue_);
    return *queue;
}

// Debug helper: the running queue is a priority-sorted (highest first)
// singly linked list, so this walks it front-to-back and reports it in
// scheduling order -- whatever's printed first is what run_next_thread
// would pick next. Used to diagnose whether a low-priority thread is being
// starved (always something higher-priority ready whenever a thread
// blocks) versus genuinely stuck for some other reason.
extern "C" void ultramodern_debug_dump_running_queue(uint8_t* rdram) {
    PTR(OSThread) cur = running_queue_impl;
    if (cur == NULLPTR) {
        printf("[QUEUE] running queue is empty\n");
        return;
    }
    printf("[QUEUE] running queue (front = next to run):");
    while (cur != NULLPTR) {
        OSThread* t = TO_PTR(OSThread, cur);
        printf(" %d(pri=%d)", t->id, t->priority);
        cur = t->next;
    }
    printf("\n");
}
