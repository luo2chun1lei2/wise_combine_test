#define q_open raw_q_open
#define q_push1 raw_q_push1
#define q_push2 raw_q_push2
#define q_pop raw_q_pop
#define q_peek raw_q_peek
#define q_size raw_q_size
#define q_close raw_q_close
#include "queue_sut.h"
#undef q_open
#undef q_push1
#undef q_push2
#undef q_pop
#undef q_peek
#undef q_size
#undef q_close

int q_open(void) { return raw_q_open(); }
int q_reopen(int ignored) { (void)ignored; return raw_q_open(); }
int q_push1(int ignored) { (void)ignored; return raw_q_push1(); }
int q_push2(int ignored) { (void)ignored; return raw_q_push2(); }
int q_pop(int ignored) { (void)ignored; return raw_q_pop(); }
int q_peek(int ignored) { (void)ignored; return raw_q_peek(); }
int q_size(int ignored) { (void)ignored; return raw_q_size(); }
int q_close(int ignored) { (void)ignored; return raw_q_close(); }
