#ifndef WISE_ORACLE_QUEUE_SUT_H
#define WISE_ORACLE_QUEUE_SUT_H
#ifndef MUTANT
#define MUTANT 0
#endif

typedef struct { int opened, count, values[8], pops; } QueueState;
static QueueState queue_state;

static int enqueue(int value) {
    if (!queue_state.opened) return -1;
    if (queue_state.count == 8) return -3;
    queue_state.values[queue_state.count++] = value;
    return 0;
}

#ifdef __cplusplus
extern "C" {
#endif
int q_open(void) {
    queue_state.opened = 1;
    if (MUTANT != 3) queue_state.count = 0;
    queue_state.pops = 0;
    return 0;
}
int q_push1(void) { return enqueue(11); }
int q_push2(void) { return enqueue(22); }
int q_pop(void) {
    int i, value;
    if (!queue_state.opened) return -1;
    if (!queue_state.count) return MUTANT == 1 ? 7 : -2;
    value = queue_state.values[MUTANT == 2 ? queue_state.count - 1 : 0];
    for (i = 1; i < queue_state.count; ++i) queue_state.values[i-1] = queue_state.values[i];
    ++queue_state.pops;
    if (!(MUTANT == 5 && queue_state.pops == 2)) --queue_state.count;
    return value;
}
int q_peek(void) {
    if (MUTANT == 4) return q_pop();
    if (!queue_state.opened) return -1;
    return queue_state.count ? queue_state.values[0] : -2;
}
int q_size(void) { return queue_state.count; }
int q_close(void) { if (MUTANT != 6) queue_state.opened = 0; return 0; }
#ifdef __cplusplus
}
#endif
#endif
