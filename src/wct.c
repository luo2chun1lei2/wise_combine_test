#include "wct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <poll.h>
#include <time.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#define WCT_MAX_RESULT (1024u * 1024u)

/* Execute one callback in a child process and transfer its result safely. */
static long long now_ms(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}
static int read_exact_deadline(int fd, void *buf, size_t n, long long deadline) {
    size_t off = 0; char *p = (char *)buf;
    while (off < n) {
        int wait_ms = -1;
        if (deadline >= 0) {
            long long rem = deadline - now_ms();
            if (rem <= 0) return -2;
            wait_ms = rem > INT_MAX ? INT_MAX : (int)rem;
        }
        struct pollfd f = {.fd = fd, .events = POLLIN};
        int pr = poll(&f, 1, wait_ms);
        if (pr == 0) return -2;
        if (pr < 0) { if (errno == EINTR) continue; return -1; }
        ssize_t got = read(fd, p + off, n - off);
        if (got == 0) return -1;
        if (got < 0) { if (errno == EINTR) continue; return -1; }
        off += (size_t)got;
    }
    return 0;
}
static int write_all(int fd, const void *buf, size_t n) {
    size_t off = 0;
    const char *p = (const char *)buf;
    while (off < n) {
        ssize_t sent = write(fd, p + off, n - off);
        if (sent < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        off += (size_t)sent;
    }
    return 0;
}
static int reap_deadline(pid_t pid, int *status, long long deadline) {
    for (;;) {
        pid_t r = waitpid(pid, status, WNOHANG);
        if (r == pid) return 0;
        if (r < 0) { if (errno == EINTR) continue; return -1; }
        if (deadline >= 0 && now_ms() >= deadline) return 1;
        struct timespec ts = {.tv_sec = 0, .tv_nsec = 1000000}; nanosleep(&ts, NULL);
    }
}
static void kill_reap(pid_t pid) { (void)kill(pid, SIGKILL); while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {} }

static int isolate_state_cb(wct_transition_fn fn, const char *input, char **actual,
                            void *ctx, unsigned timeout_ms, const char **why,
                            int *exit_code, int *signal_no, int *timed_out) {
    if (timeout_ms > (unsigned)INT_MAX) { *why = "timeout exceeds poll limit"; return -1; }
    int p[2]; if (pipe(p) < 0) { *why = "isolation pipe failed"; return -1; }
    pid_t pid = fork();
    if (pid < 0) { close(p[0]); close(p[1]); *why = "isolation fork failed"; return -1; }
    if (pid == 0) {
        close(p[0]); char *out = NULL; int rc = fn ? fn(input, &out, ctx) : 0;
        int present = out != NULL; uint64_t len = out ? strlen(out) : 0;
        if (write_all(p[1], &rc, sizeof rc) ||
            write_all(p[1], &present, sizeof present) ||
            write_all(p[1], &len, sizeof len) ||
            (len && write_all(p[1], out, (size_t)len))) _exit(111);
        free(out); close(p[1]); _exit(0);
    }
    close(p[1]); long long deadline = timeout_ms ? now_ms() + timeout_ms : -1;
    int rc = -1, present = 0; uint64_t len = 0;
    int rr = read_exact_deadline(p[0], &rc, sizeof rc, deadline);
    if (!rr) rr = read_exact_deadline(p[0], &present, sizeof present, deadline);
    if (!rr) rr = read_exact_deadline(p[0], &len, sizeof len, deadline);
    if (!rr && len > WCT_MAX_RESULT) rr = -3;
    if (!rr && present) { *actual = malloc((size_t)len + 1); if (!*actual) rr = -1; else if (read_exact_deadline(p[0], *actual, (size_t)len, deadline)) { free(*actual); *actual = NULL; rr = -1; } else (*actual)[len] = '\0'; }
    close(p[0]); int status = 0;
    if (rr == -2) { kill_reap(pid); *timed_out = 1; *signal_no = SIGKILL; *why = "callback timeout"; return -1; }
    if (rr) { kill_reap(pid); *why = rr == -3 ? "callback result too large" : "isolation read failed"; return -1; }
    int wr = reap_deadline(pid, &status, deadline);
    if (wr == 1) { kill_reap(pid); *timed_out = 1; *signal_no = SIGKILL; *why = "callback timeout"; return -1; }
    if (wr < 0) { kill_reap(pid); *why = "isolation wait failed"; return -1; }
    if (WIFEXITED(status)) *exit_code = WEXITSTATUS(status); else if (WIFSIGNALED(status)) *signal_no = WTERMSIG(status);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { *why = "callback terminated"; return -1; }
    return rc;
}

static int isolate_relation_cb(wct_call_fn fn, const char *id, const char *const *args,
                               size_t argc, char **result, void *ctx, unsigned timeout_ms,
                               const char **why, int *exit_code, int *signal_no, int *timed_out) {
    if (timeout_ms > (unsigned)INT_MAX) { *why = "timeout exceeds poll limit"; return -1; }
    int p[2]; if (pipe(p) < 0) { *why = "isolation pipe failed"; return -1; }
    pid_t pid = fork();
    if (pid < 0) { close(p[0]); close(p[1]); *why = "isolation fork failed"; return -1; }
    if (pid == 0) {
        close(p[0]); char *out = NULL; int rc = fn ? fn(id, args, argc, &out, ctx) : 0;
        int present = out != NULL; uint64_t len = out ? strlen(out) : 0;
        if (write_all(p[1], &rc, sizeof rc) ||
            write_all(p[1], &present, sizeof present) ||
            write_all(p[1], &len, sizeof len) ||
            (len && write_all(p[1], out, (size_t)len))) _exit(111);
        free(out); close(p[1]); _exit(0);
    }
    close(p[1]); long long deadline = timeout_ms ? now_ms() + timeout_ms : -1;
    int rc = -1, present = 0; uint64_t len = 0;
    int rr = read_exact_deadline(p[0], &rc, sizeof rc, deadline);
    if (!rr) rr = read_exact_deadline(p[0], &present, sizeof present, deadline);
    if (!rr) rr = read_exact_deadline(p[0], &len, sizeof len, deadline);
    if (!rr && len > WCT_MAX_RESULT) rr = -3;
    if (!rr && present) { *result = malloc((size_t)len + 1); if (!*result) rr = -1; else if (read_exact_deadline(p[0], *result, (size_t)len, deadline)) { free(*result); *result = NULL; rr = -1; } else (*result)[len] = '\0'; }
    close(p[0]); int status = 0;
    if (rr == -2) { kill_reap(pid); *timed_out = 1; *signal_no = SIGKILL; *why = "callback timeout"; return -1; }
    if (rr) { kill_reap(pid); *why = rr == -3 ? "callback result too large" : "isolation read failed"; return -1; }
    int wr = reap_deadline(pid, &status, deadline);
    if (wr == 1) { kill_reap(pid); *timed_out = 1; *signal_no = SIGKILL; *why = "callback timeout"; return -1; }
    if (wr < 0) { kill_reap(pid); *why = "isolation wait failed"; return -1; }
    if (WIFEXITED(status)) *exit_code = WEXITSTATUS(status); else if (WIFSIGNALED(status)) *signal_no = WTERMSIG(status);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) { *why = "callback terminated"; return -1; }
    return rc;
}

static char *dupstr(const char *s) {
    if (!s) return NULL;
    size_t n = strlen(s) + 1;
    char *p = malloc(n);
    if (p) memcpy(p, s, n);
    return p;
}
static void seterr(char *e, size_t n, const char *s) { if (n) { snprintf(e, n, "%s", s); } }
static int addstr(char ***a, size_t *n, const char *s) {
    char *copy = dupstr(s);
    if (!copy) return -1;
    char **p = realloc(*a, (*n + 1) * sizeof **a);
    if (!p) { free(copy); return -1; }
    *a = p;
    p[*n] = copy;
    (*n)++;
    return 0;
}
static int find_state(const wct_state_graph *g, const char *id) { for (size_t i=0;i<g->state_count;i++) if (!strcmp(g->states[i], id)) return (int)i; return -1; }
static int find_call(const wct_relation_graph *g, const char *id) { for (size_t i=0;i<g->call_count;i++) if (!strcmp(g->calls[i].id,id)) return (int)i; return -1; }
static unsigned next_rand(unsigned *state) {
    unsigned x = *state ? *state : 1u;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5; *state = x; return x;
}

static int call_depends_on(const wct_relation_graph *g, size_t dependent,
                           size_t prerequisite) {
    for (size_t i = 0; i < g->relation_count; ++i)
        if (find_call(g, g->relations[i].from) == (int)prerequisite &&
            find_call(g, g->relations[i].to) == (int)dependent)
            return 1;
    for (size_t i = 0; i < g->calls[dependent].argc; ++i) {
        const char *arg = g->calls[dependent].args[i];
        if (arg && arg[0] == '$' &&
            find_call(g, arg + 1) == (int)prerequisite)
            return 1;
    }
    return 0;
}

void wct_state_graph_free(wct_state_graph *g) { if (!g) return; free(g->id); free(g->initial); for(size_t i=0;i<g->state_count;i++) free(g->states[i]); free(g->states); for(size_t i=0;i<g->transition_count;i++){free(g->transitions[i].id);free(g->transitions[i].from);free(g->transitions[i].to);free(g->transitions[i].input);free(g->transitions[i].expect);} free(g->transitions); memset(g,0,sizeof *g); }
void wct_relation_graph_free(wct_relation_graph *g) { if (!g) return; free(g->id); for(size_t i=0;i<g->call_count;i++){free(g->calls[i].id);for(size_t j=0;j<g->calls[i].argc;j++)free(g->calls[i].args[j]);free(g->calls[i].args);free(g->calls[i].arg_types);} free(g->calls); for(size_t i=0;i<g->relation_count;i++){free(g->relations[i].from);free(g->relations[i].to);} free(g->relations); memset(g,0,sizeof *g); }
void wct_report_free(wct_report *r) {
    if (r) {
        free(r->error);
        free(r->scenario);
        free(r->expected);
        free(r->actual);
        memset(r,0,sizeof *r);
    }
}

int wct_parse_file(const char *path, wct_state_graph *s, wct_relation_graph *r, char *err, size_t errlen) {
    if (!s || !r) { seterr(err, errlen, "null graph output"); return -1; }
    memset(s,0,sizeof *s); memset(r,0,sizeof *r);
    FILE *f = fopen(path,"r"); char line[2048]; unsigned schema=0; size_t ln=0, schema_line=0;
    if (!f) { seterr(err,errlen,"cannot open model"); return -1; }
    while (fgets(line,sizeof line,f)) {
        ln++;
        size_t col = strspn(line, " \t") + 1;
        if (!strchr(line, '\n') && !feof(f)) {
            char msg[128]; snprintf(msg, sizeof msg, "line %zu column %zu: line too long", ln, col);
            seterr(err, errlen, msg); goto fail;
        }
        char *save=NULL, *tok=strtok_r(line," \t\r\n",&save); if(!tok || tok[0]=='#') continue;
        if(!strcmp(tok,"schema")){ char *v=strtok_r(NULL," \t\r\n",&save); schema_line=ln; if(!v || sscanf(v,"%u",&schema)!=1 || schema!=WCT_SCHEMA_VERSION){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: unsupported schema",ln, col); seterr(err,errlen,msg);goto fail;} }
        else if(!strcmp(tok,"state_graph")){ char *id=strtok_r(NULL," \t\r\n",&save), *init=strtok_r(NULL," \t\r\n",&save); if(!id||!init){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: missing state_graph fields",ln, col); seterr(err,errlen,msg);goto fail;} free(s->id); free(s->initial); s->id=dupstr(id);s->initial=dupstr(init); if(!s->id||!s->initial)goto oom; }
        else if(!strcmp(tok,"state")){ char *id=strtok_r(NULL," \t\r\n",&save); if(!id){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: missing state id",ln, col); seterr(err,errlen,msg);goto fail;} if(addstr(&s->states,&s->state_count,id)){goto oom;} }
        else if(!strcmp(tok,"transition")){ char *a[5]; for(int i=0;i<5;i++)a[i]=strtok_r(NULL," \t\r\n",&save); if(!a[4]){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: missing transition fields",ln, col); seterr(err,errlen,msg);goto fail;} wct_transition *t=realloc(s->transitions,(s->transition_count+1)*sizeof *t); if(!t)goto oom; s->transitions=t; t=&t[s->transition_count]; memset(t,0,sizeof *t); t->id=dupstr(a[0]);t->from=dupstr(a[1]);t->to=dupstr(a[2]);t->input=dupstr(a[3]);t->expect=dupstr(a[4]); if(!t->id||!t->from||!t->to||!t->input||!t->expect)goto oom; s->transition_count++; }
        else if(!strcmp(tok,"relation_graph")){ char *id=strtok_r(NULL," \t\r\n",&save); if(!id){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: missing relation_graph id",ln, col); seterr(err,errlen,msg);goto fail;} free(r->id); r->id=dupstr(id); if(!r->id)goto oom; }
        else if(!strcmp(tok,"call")){ char *id=strtok_r(NULL," \t\r\n",&save); if(!id){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: missing call id",ln, col); seterr(err,errlen,msg);goto fail;} wct_call *c=realloc(r->calls,(r->call_count+1)*sizeof *c); if(!c)goto oom; r->calls=c; c=&c[r->call_count]; memset(c,0,sizeof *c); c->id=dupstr(id); if(!c->id)goto oom; char *arg; while((arg=strtok_r(NULL," \t\r\n",&save))){ if(addstr(&c->args,&c->argc,arg))goto oom; } r->call_count++; }
        else if(!strcmp(tok,"contract")){ char *id=strtok_r(NULL," \t\r\n",&save), *argc_s=strtok_r(NULL," \t\r\n",&save); int ci=id ? find_call(r,id) : -1; size_t argc=0; if(!id || !argc_s || ci < 0 || sscanf(argc_s,"%zu",&argc) != 1){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: invalid call contract",ln, col); seterr(err,errlen,msg);goto fail;} wct_call *c=&r->calls[(size_t)ci]; c->expected_argc=argc; c->contract_set=1; char *typ; size_t count=0; while((typ=strtok_r(NULL," \t\r\n",&save))){ wct_value_type t=WCT_ANY; if(!strcmp(typ,"int")) t=WCT_INT; else if(!strcmp(typ,"bool")) t=WCT_BOOL; else if(!strcmp(typ,"string")) t=WCT_STRING; else if(!strcmp(typ,"bytes")) t=WCT_BYTES; else if(!strcmp(typ,"ref")) t=WCT_REF; else if(!strcmp(typ,"any")) t=WCT_ANY; else {char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: unknown argument type",ln, col); seterr(err,errlen,msg);goto fail;} wct_value_type *types=realloc(c->arg_types,(count+1)*sizeof *types); if(!types)goto oom; c->arg_types=types; c->arg_types[count++]=t; } c->arg_type_count=count; }
        else if(!strcmp(tok,"relation")){ char *a=strtok_r(NULL," \t\r\n",&save), *b=strtok_r(NULL," \t\r\n",&save); if(!a||!b){char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: missing relation fields",ln, col); seterr(err,errlen,msg);goto fail;} wct_relation *x=realloc(r->relations,(r->relation_count+1)*sizeof *x);if(!x)goto oom;r->relations=x;x=&x[r->relation_count];x->from=dupstr(a);x->to=dupstr(b);if(!x->from||!x->to)goto oom;r->relation_count++; }
        else { char msg[128]; snprintf(msg,sizeof msg,"line %zu column %zu: unknown directive",ln, col);seterr(err,errlen,msg);goto fail; }
    }
    fclose(f);
    if (schema != WCT_SCHEMA_VERSION) {
        char msg[128]; snprintf(msg,sizeof msg,"line %zu column 1: missing schema", schema_line ? schema_line : 1u); seterr(err, errlen, msg);
        wct_state_graph_free(s);
        wct_relation_graph_free(r);
        return -1;
    }
    return 0;
oom: seterr(err,errlen,"out of memory");
fail: fclose(f); wct_state_graph_free(s); wct_relation_graph_free(r); return -1;
}

int wct_validate_state(const wct_state_graph *g, char *err, size_t n) {
    if (!g || (g->state_count && !g->states) ||
        (g->transition_count && !g->transitions) || !g->initial ||
        find_state(g, g->initial) < 0) {
        seterr(err, n, "unknown initial state");
        return -1;
    }
    for (size_t i = 0; i < g->state_count; i++) {
        if (!g->states[i] || !*g->states[i]) {
            seterr(err, n, "empty state id");
            return -1;
        }
        for (size_t j = i + 1; j < g->state_count; j++) {
            if (!strcmp(g->states[i], g->states[j])) {
                seterr(err, n, "duplicate state");
                return -1;
            }
        }
    }
    for (size_t i = 0; i < g->transition_count; i++) {
        const wct_transition *t = &g->transitions[i];
        if (!t->id || !t->from || !t->to || !t->input || !t->expect) {
            seterr(err, n, "incomplete transition");
            return -1;
        }
        if (find_state(g, t->from) < 0 || find_state(g, t->to) < 0) {
            seterr(err, n, "transition references unknown state");
            return -1;
        }
        for (size_t j = 0; j < i; j++) {
            if (!strcmp(t->id, g->transitions[j].id)) {
                seterr(err, n, "duplicate transition");
                return -1;
            }
        }
    }
    return 0;
}

static wct_value_type infer_arg_type(const char *arg) {
    if (!arg) return WCT_ANY;
    if (arg[0] == '$' && arg[1] != '\0') return WCT_REF;
    if (!strcmp(arg, "true") || !strcmp(arg, "false")) return WCT_BOOL;
    char *end = NULL;
    (void)strtoll(arg, &end, 10);
    if (end && *end == '\0' && end != arg) return WCT_INT;
    if (!strncmp(arg, "0x", 2) && arg[2] != '\0') return WCT_BYTES;
    return WCT_STRING;
}

int wct_validate_relation(const wct_relation_graph *g, char *err, size_t n) {
    if (!g || (g->call_count && !g->calls) ||
        (g->relation_count && !g->relations)) {
        seterr(err, n, "null relation graph"); return -1;
    }
    for (size_t i = 0; i < g->call_count; i++) {
        if (!g->calls[i].id || !*g->calls[i].id) {
            seterr(err, n, "empty call id");
            return -1;
        }
        if (g->calls[i].argc && !g->calls[i].args) {
            seterr(err, n, "call arguments missing");
            return -1;
        }
        if ((g->calls[i].contract_set || g->calls[i].expected_argc != 0) &&
            g->calls[i].argc != g->calls[i].expected_argc) {
            char msg[128];
            snprintf(msg, sizeof msg, "call %s arity mismatch: expected %zu, got %zu",
                     g->calls[i].id, g->calls[i].expected_argc, g->calls[i].argc);
            seterr(err, n, msg);
            return -1;
        }
        if (g->calls[i].arg_type_count) {
            if (!g->calls[i].arg_types || g->calls[i].arg_type_count != g->calls[i].argc) {
                seterr(err, n, "call argument type contract has wrong arity");
                return -1;
            }
            for (size_t j = 0; j < g->calls[i].argc; ++j) {
                wct_value_type want = g->calls[i].arg_types[j];
                if (want != WCT_ANY && want != infer_arg_type(g->calls[i].args[j])) {
                    char msg[128];
                    snprintf(msg, sizeof msg, "call %s argument %zu type mismatch",
                             g->calls[i].id, j + 1);
                    seterr(err, n, msg);
                    return -1;
                }
            }
        }
        for (size_t j = 0; j < g->calls[i].argc; ++j) {
            const char *arg = g->calls[i].args[j];
            if (!arg) {
                seterr(err, n, "null call argument");
                return -1;
            }
            if (arg && arg[0] == '$' && (arg[1] == '\0' ||
                find_call(g, arg + 1) < 0)) {
                seterr(err, n, "call argument references unknown call");
                return -1;
            }
        }
        for (size_t j = i + 1; j < g->call_count; j++) {
            if (!strcmp(g->calls[i].id, g->calls[j].id)) {
                seterr(err, n, "duplicate call");
                return -1;
            }
        }
    }
    for (size_t i = 0; i < g->relation_count; i++) {
        if (!g->relations[i].from || !g->relations[i].to ||
            find_call(g, g->relations[i].from) < 0 ||
            find_call(g, g->relations[i].to) < 0) {
            seterr(err, n, "relation references unknown call");
            return -1;
        }
        if (!strcmp(g->relations[i].from, g->relations[i].to)) {
            seterr(err, n, "relation cycle");
            return -1;
        }
        for (size_t j = 0; j < i; ++j) {
            if (!strcmp(g->relations[i].from, g->relations[j].from) &&
                !strcmp(g->relations[i].to, g->relations[j].to)) {
                seterr(err, n, "duplicate relation");
                return -1;
            }
        }
    }
    size_t *ind = calloc(g->call_count ? g->call_count : 1, sizeof *ind);
    if (!ind) { seterr(err, n, "out of memory"); return -1; }
    for (size_t dependent = 0; dependent < g->call_count; ++dependent)
        for (size_t prerequisite = 0; prerequisite < g->call_count; ++prerequisite)
            if (call_depends_on(g, dependent, prerequisite)) ind[dependent]++;
    size_t done = 0;
    while (1) {
        int found = -1;
        for (size_t i = 0; i < g->call_count; i++)
            if (ind[i] == 0) { found = (int)i; break; }
        if (found < 0) break;
        ind[(size_t)found] = (size_t)-1;
        done++;
        for (size_t dependent = 0; dependent < g->call_count; ++dependent)
            if (ind[dependent] != (size_t)-1 &&
                call_depends_on(g, dependent, (size_t)found)) ind[dependent]--;
    }
    free(ind);
    if (done != g->call_count) { seterr(err, n, "relation cycle"); return -1; }
    return 0;
}

int wct_run_state(const wct_state_graph *g, wct_transition_fn fn, void *ctx,
                  wct_limits lim, wct_report *r) {
    if (!r) return -1;
    memset(r, 0, sizeof *r);
    r->seed = lim.seed;
    unsigned rng = lim.seed;
    char err[128];
    if (wct_validate_state(g, err, sizeof err)) {
        r->error = dupstr(err);
        return -1;
    }
    size_t max = lim.max_steps;
    if (!max) {
        /* A branch may require replaying a path from the initial state before
         * each uncovered edge.  Bound the default by a simple finite graph
         * upper bound rather than silently truncating reachable coverage. */
        size_t factor = g->state_count + 1;
        max = g->transition_count && factor > SIZE_MAX / g->transition_count
            ? SIZE_MAX : g->transition_count * factor;
    }
    r->declared_edges = g->transition_count;
    unsigned char *seen = calloc(g->transition_count ? g->transition_count : 1, 1);
    unsigned char *reachable = calloc(g->state_count ? g->state_count : 1, 1);
    size_t *queue = calloc(g->state_count ? g->state_count : 1, sizeof *queue);
    size_t *plan = calloc(g->transition_count ? g->transition_count + g->state_count + 1 : 1, sizeof *plan);
    if (!seen || !reachable || !queue || !plan) {
        free(seen); free(reachable); free(queue); free(plan);
        r->error = dupstr("out of memory"); return -1;
    }
    int initial = find_state(g, g->initial);
    int current = initial;
    size_t qhead = 0, qtail = 0;
    reachable[(size_t)initial] = 1;
    queue[qtail++] = (size_t)initial;
    while (qhead < qtail) {
        size_t state = queue[qhead++];
        for (size_t i = 0; i < g->transition_count; ++i) {
            const wct_transition *t = &g->transitions[i];
            if (find_state(g, t->from) == (int)state) {
                int target = find_state(g, t->to);
                if (target >= 0 && !reachable[(size_t)target]) {
                    reachable[(size_t)target] = 1;
                    if (qtail < g->state_count) queue[qtail++] = (size_t)target;
                }
            }
        }
    }
    size_t plan_len = 0, plan_pos = 0;
    if (fn && lim.state_reset && lim.state_reset(ctx)) {
        free(queue); free(plan); free(reachable); free(seen);
        r->error = dupstr("state reset callback failed");
        r->failures = 1;
        return -1;
    }
    for (size_t step = 0; step < max; step++) {
        int picked = -1;
        size_t ready_count = 0;
        if (plan_pos < plan_len) {
            picked = (int)plan[plan_pos++];
        }
        for (size_t i = 0; i < g->transition_count; i++) {
            int source = find_state(g, g->transitions[i].from);
            if (!seen[i] && source == current) ready_count++;
        }
        /* A graph may branch or terminate. Start a new scenario at the
         * initial state and replay a valid prefix to an uncovered edge;
         * never invoke an edge whose source is not the current state. */
        if (picked < 0 && !ready_count) {
            int target = -1;
            for (size_t i = 0; i < g->transition_count; ++i) {
                int source = find_state(g, g->transitions[i].from);
                if (!seen[i] && source >= 0 && reachable[(size_t)source]) { target = (int)i; break; }
            }
            if (target >= 0) {
                if (lim.isolate) {
                    r->error = dupstr("isolated branch replay is unsupported; use state_reset without isolation");
                    r->failures = 1;
                    break;
                }
                if (!lim.state_reset) {
                    r->error = dupstr("state reset callback required for branch replay");
                    r->failures = 1;
                    break;
                }
                size_t *pred_state = calloc(g->state_count, sizeof *pred_state);
                size_t *pred_edge = calloc(g->state_count, sizeof *pred_edge);
                unsigned char *vis = calloc(g->state_count, 1);
                size_t *bq = calloc(g->state_count, sizeof *bq), bh = 0, bt = 0;
                if (!pred_state || !pred_edge || !vis || !bq) { free(pred_state); free(pred_edge); free(vis); free(bq); r->error = dupstr("out of memory"); break; }
                vis[(size_t)initial] = 1; bq[bt++] = (size_t)initial;
                int goal = find_state(g, g->transitions[(size_t)target].from);
                while (bh < bt && !vis[(size_t)goal]) {
                    size_t st = bq[bh++];
                    for (size_t i = 0; i < g->transition_count; ++i) {
                        if (find_state(g, g->transitions[i].from) != (int)st) continue;
                        int ns = find_state(g, g->transitions[i].to);
                        if (ns >= 0 && !vis[(size_t)ns]) { vis[(size_t)ns] = 1; pred_state[(size_t)ns] = st; pred_edge[(size_t)ns] = i; bq[bt++] = (size_t)ns; }
                    }
                }
                size_t rev_len = 0, st = (size_t)goal;
                while ((int)st != initial) { rev_len++; st = pred_state[st]; }
                plan_len = rev_len + 1; plan_pos = 0; st = (size_t)goal;
                for (size_t k = rev_len; k > 0; --k) { plan[k - 1] = pred_edge[st]; st = pred_state[st]; }
                plan[rev_len] = (size_t)target;
                if (fn && lim.state_reset(ctx)) {
                    free(pred_state); free(pred_edge); free(vis); free(bq);
                    r->error = dupstr("state reset callback failed");
                    r->failures = 1;
                    break;
                }
                current = initial;
                free(pred_state); free(pred_edge); free(vis); free(bq);
                picked = (int)plan[plan_pos++];
            }
        }
        if (picked < 0 && ready_count) {
            size_t choice = lim.seed ? (next_rand(&rng) % ready_count) : 0;
            for (size_t i = 0; i < g->transition_count; i++) {
                int source = find_state(g, g->transitions[i].from);
                if (!seen[i] && source == current) {
                    if (choice == 0) { picked = (int)i; break; }
                    choice--;
                }
            }
        }
        if (picked < 0) break;
        const wct_transition *t = &g->transitions[(size_t)picked];
        char *actual = NULL;
        const char *isolation_error = NULL;
        int rc = lim.isolate ? isolate_state_cb(fn, t->input, &actual, ctx, lim.timeout_ms, &isolation_error,
                                                &r->process_exit, &r->process_signal, &r->timed_out)
                             : (fn ? fn(t->input, &actual, ctx) : 0);
        if (rc || !actual || strcmp(actual, t->expect)) {
            r->failures++;
            r->failed_step = step + 1;
            r->scenario = dupstr(t->id);
            r->expected = dupstr(t->expect);
            r->actual = actual ? dupstr(actual) : NULL;
            r->error = dupstr(isolation_error ? isolation_error : (rc ? "transition callback failed" : "transition expectation failed"));
            free(actual);
            break;
        }
        free(actual);
        if (!seen[(size_t)picked]) {
            seen[(size_t)picked] = 1;
            r->covered++;
        }
        current = find_state(g, t->to);
        r->steps++;
    }
    for (size_t i = 0; i < g->transition_count; i++) {
        int source = find_state(g, g->transitions[i].from);
        if (source < 0 || !reachable[(size_t)source] || !seen[i]) r->uncovered++;
    }
    free(queue); free(plan); free(reachable); free(seen);
    r->covered_edges = r->covered;
    r->uncovered_edges = r->uncovered;
    if (!r->failures && r->uncovered) { if (!r->error) r->error = dupstr("uncovered transition"); return -1; }
    return r->failures ? -1 : 0;
}

int wct_run_relation(const wct_relation_graph *g, wct_call_fn fn, void *ctx,
                    wct_limits lim, wct_report *r) {
    if (!r) return -1;
    memset(r, 0, sizeof *r);
    r->seed = lim.seed;
    unsigned rng = lim.seed;
    char err[128];
    if (wct_validate_relation(g, err, sizeof err)) {
        r->error = dupstr(err);
        return -1;
    }
    size_t n = g->call_count;
    size_t max = lim.max_flows ? lim.max_flows : n;
    r->declared_edges = g->relation_count;
    unsigned char *done = calloc(n ? n : 1, 1);
    char **res = calloc(n ? n : 1, sizeof *res);
    if (!done || !res) {
        free(done); free(res); r->error = dupstr("out of memory"); return -1;
    }
    for (size_t step = 0; step < max; step++) {
        int pick = -1;
        size_t ready_count = 0;
        for (size_t i = 0; i < n; i++) {
            if (done[i]) continue;
            int ready = 1;
            for (size_t j = 0; j < g->relation_count; j++) {
                int target = find_call(g, g->relations[j].to);
                if (target == (int)i) {
                    int source = find_call(g, g->relations[j].from);
                    if (source < 0 || !done[(size_t)source]) ready = 0;
                }
            }
            for (size_t j = 0; j < g->calls[i].argc; j++) {
                const char *arg = g->calls[i].args[j];
                if (arg && arg[0] == '$') {
                    int source = find_call(g, arg + 1);
                    if (source < 0 || !done[(size_t)source]) ready = 0;
                }
            }
            if (ready) ready_count++;
        }
        if (ready_count) {
            size_t choice = lim.seed ? (next_rand(&rng) % ready_count) : 0;
            for (size_t i = 0; i < n; i++) {
                if (done[i]) continue;
                int ready = 1;
                for (size_t j = 0; j < g->relation_count; j++) if (find_call(g, g->relations[j].to) == (int)i) { int source = find_call(g, g->relations[j].from); if (source < 0 || !done[(size_t)source]) ready = 0; }
                for (size_t j = 0; j < g->calls[i].argc; j++) { const char *arg = g->calls[i].args[j]; if (arg && arg[0] == '$') { int source = find_call(g, arg + 1); if (source < 0 || !done[(size_t)source]) ready = 0; } }
                if (ready) {
                    if (!lim.seed) {
                        if (pick < 0 || strcmp(g->calls[i].id, g->calls[(size_t)pick].id) < 0)
                            pick = (int)i;
                    } else if (choice == 0) { pick = (int)i; break; }
                    else choice--;
                }
            }
        }
        if (pick < 0) break;
        const wct_call *c = &g->calls[(size_t)pick];
        const char **args = calloc(c->argc ? c->argc : 1, sizeof *args);
        if (!args) { r->failures++; r->error = dupstr("out of memory"); break; }
        for (size_t i = 0; i < c->argc; ++i) {
            const char *arg = c->args[i];
            if (arg && arg[0] == '$') {
                const char *name = arg + 1;
                int source = find_call(g, name);
                if (source >= 0 && done[(size_t)source]) arg = res[(size_t)source];
            }
            args[i] = arg;
        }
        char *out = NULL;
        const char *isolation_error = NULL;
        int rc = lim.isolate ? isolate_relation_cb(fn, c->id, args, c->argc, &out, ctx, lim.timeout_ms, &isolation_error,
                                                   &r->process_exit, &r->process_signal, &r->timed_out)
                             : (fn ? fn(c->id, args, c->argc, &out, ctx) : 0);
        free(args);
        if (rc || !out) {
            r->failures++;
            r->failed_step = step + 1;
            r->scenario = dupstr(c->id);
            r->error = dupstr(isolation_error ? isolation_error : (rc ? "call callback failed" : "call callback returned no result"));
            free(out);
            break;
        }
        res[(size_t)pick] = out;
        done[(size_t)pick] = 1;
        r->steps++;
        r->covered++;
    }
    for (size_t i = 0; i < n; i++) if (!done[i]) r->uncovered++;
    r->covered_edges = 0;
    for (size_t i = 0; i < g->relation_count; i++) { int a=find_call(g,g->relations[i].from), b=find_call(g,g->relations[i].to); if (a>=0 && b>=0 && done[(size_t)a] && done[(size_t)b]) r->covered_edges++; }
    r->uncovered_edges = r->declared_edges - r->covered_edges;
    for (size_t i = 0; i < n; i++) free(res[i]);
    free(res);
    free(done);
    if (!r->failures && r->uncovered) {
        r->error = dupstr("uncovered call");
        return -1;
    }
    return r->failures ? -1 : 0;
}
