#include "wct.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

void wct_state_graph_free(wct_state_graph *g) { if (!g) return; free(g->id); free(g->initial); for(size_t i=0;i<g->state_count;i++) free(g->states[i]); free(g->states); for(size_t i=0;i<g->transition_count;i++){free(g->transitions[i].id);free(g->transitions[i].from);free(g->transitions[i].to);free(g->transitions[i].input);free(g->transitions[i].expect);} free(g->transitions); memset(g,0,sizeof *g); }
void wct_relation_graph_free(wct_relation_graph *g) { if (!g) return; free(g->id); for(size_t i=0;i<g->call_count;i++){free(g->calls[i].id);for(size_t j=0;j<g->calls[i].argc;j++)free(g->calls[i].args[j]);free(g->calls[i].args);} free(g->calls); for(size_t i=0;i<g->relation_count;i++){free(g->relations[i].from);free(g->relations[i].to);} free(g->relations); memset(g,0,sizeof *g); }
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
    memset(s,0,sizeof *s); memset(r,0,sizeof *r);
    FILE *f = fopen(path,"r"); char line[2048]; unsigned schema=0; size_t ln=0;
    if (!f) { seterr(err,errlen,"cannot open model"); return -1; }
    while (fgets(line,sizeof line,f)) { ln++; char *save=NULL, *tok=strtok_r(line," \t\r\n",&save); if(!tok || tok[0]=='#') continue;
        if(!strcmp(tok,"schema")){ char *v=strtok_r(NULL," \t\r\n",&save); if(!v || sscanf(v,"%u",&schema)!=1 || schema!=WCT_SCHEMA_VERSION){seterr(err,errlen,"unsupported schema");goto fail;} }
        else if(!strcmp(tok,"state_graph")){ char *id=strtok_r(NULL," \t\r\n",&save), *init=strtok_r(NULL," \t\r\n",&save); if(!id||!init){seterr(err,errlen,"line missing state_graph fields");goto fail;} free(s->id); free(s->initial); s->id=dupstr(id);s->initial=dupstr(init); if(!s->id||!s->initial)goto oom; }
        else if(!strcmp(tok,"state")){ char *id=strtok_r(NULL," \t\r\n",&save); if(!id||addstr(&s->states,&s->state_count,id)){seterr(err,errlen,"invalid state");goto fail;} }
        else if(!strcmp(tok,"transition")){ char *a[5]; for(int i=0;i<5;i++)a[i]=strtok_r(NULL," \t\r\n",&save); if(!a[4]){seterr(err,errlen,"line missing transition fields");goto fail;} wct_transition *t=realloc(s->transitions,(s->transition_count+1)*sizeof *t); if(!t)goto oom; s->transitions=t; t=&t[s->transition_count]; memset(t,0,sizeof *t); t->id=dupstr(a[0]);t->from=dupstr(a[1]);t->to=dupstr(a[2]);t->input=dupstr(a[3]);t->expect=dupstr(a[4]); if(!t->id||!t->from||!t->to||!t->input||!t->expect)goto oom; s->transition_count++; }
        else if(!strcmp(tok,"relation_graph")){ char *id=strtok_r(NULL," \t\r\n",&save); if(!id){seterr(err,errlen,"line missing relation_graph id");goto fail;} free(r->id); r->id=dupstr(id); if(!r->id)goto oom; }
        else if(!strcmp(tok,"call")){ char *id=strtok_r(NULL," \t\r\n",&save); if(!id){seterr(err,errlen,"line missing call id");goto fail;} wct_call *c=realloc(r->calls,(r->call_count+1)*sizeof *c); if(!c)goto oom; r->calls=c; c=&c[r->call_count]; memset(c,0,sizeof *c); c->id=dupstr(id); if(!c->id)goto oom; char *arg; while((arg=strtok_r(NULL," \t\r\n",&save))){ if(addstr(&c->args,&c->argc,arg))goto oom; } r->call_count++; }
        else if(!strcmp(tok,"relation")){ char *a=strtok_r(NULL," \t\r\n",&save), *b=strtok_r(NULL," \t\r\n",&save); if(!a||!b){seterr(err,errlen,"line missing relation fields");goto fail;} wct_relation *x=realloc(r->relations,(r->relation_count+1)*sizeof *x);if(!x)goto oom;r->relations=x;x=&x[r->relation_count];x->from=dupstr(a);x->to=dupstr(b);if(!x->from||!x->to)goto oom;r->relation_count++; }
        else { char msg[128]; snprintf(msg,sizeof msg,"line %zu: unknown directive",ln);seterr(err,errlen,msg);goto fail; }
    }
    fclose(f);
    if (schema != WCT_SCHEMA_VERSION) {
        seterr(err, errlen, "missing schema");
        wct_state_graph_free(s);
        wct_relation_graph_free(r);
        return -1;
    }
    return 0;
oom: seterr(err,errlen,"out of memory");
fail: fclose(f); wct_state_graph_free(s); wct_relation_graph_free(r); return -1;
}

int wct_validate_state(const wct_state_graph *g, char *err, size_t n) {
    if (!g || !g->initial || find_state(g, g->initial) < 0) {
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

int wct_validate_relation(const wct_relation_graph *g, char *err, size_t n) {
    if (!g) { seterr(err, n, "null relation graph"); return -1; }
    for (size_t i = 0; i < g->call_count; i++) {
        if (!g->calls[i].id || !*g->calls[i].id) {
            seterr(err, n, "empty call id");
            return -1;
        }
        for (size_t j = 0; j < g->calls[i].argc; ++j) {
            const char *arg = g->calls[i].args[j];
            if (arg && arg[0] == '$' && arg[1] != '\0' &&
                find_call(g, arg + 1) < 0) {
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
    for (size_t i = 0; i < g->relation_count; i++)
        ind[(size_t)find_call(g, g->relations[i].to)]++;
    size_t done = 0;
    while (1) {
        int found = -1;
        for (size_t i = 0; i < g->call_count; i++)
            if (ind[i] == 0) { found = (int)i; break; }
        if (found < 0) break;
        ind[(size_t)found] = (size_t)-1;
        done++;
        for (size_t i = 0; i < g->relation_count; i++) {
            if (find_call(g, g->relations[i].from) == found)
                ind[(size_t)find_call(g, g->relations[i].to)]--;
        }
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
    char err[128];
    if (wct_validate_state(g, err, sizeof err)) {
        r->error = dupstr(err);
        return -1;
    }
    size_t max = lim.max_steps ? lim.max_steps : g->transition_count;
    unsigned char *seen = calloc(g->transition_count ? g->transition_count : 1, 1);
    unsigned char *reachable = calloc(g->state_count ? g->state_count : 1, 1);
    size_t *queue = calloc(g->state_count ? g->state_count : 1, sizeof *queue);
    if (!seen || !reachable || !queue) {
        free(seen); free(reachable); free(queue);
        r->error = dupstr("out of memory"); return -1;
    }
    int initial = find_state(g, g->initial);
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
    for (size_t step = 0; step < max; step++) {
        int picked = -1;
        for (size_t i = 0; i < g->transition_count; i++) {
            const wct_transition *candidate = &g->transitions[i];
            int source = find_state(g, candidate->from);
            if (!seen[i] && source >= 0 && reachable[(size_t)source]) {
                picked = (int)i;
                break;
            }
        }
        if (picked < 0) break;
        const wct_transition *t = &g->transitions[(size_t)picked];
        char *actual = NULL;
        int rc = fn ? fn(t->input, &actual, ctx) : 0;
        if (rc || !actual || strcmp(actual, t->expect)) {
            r->failures++;
            r->failed_step = step + 1;
            r->scenario = dupstr(t->id);
            r->expected = dupstr(t->expect);
            r->actual = actual ? dupstr(actual) : NULL;
            r->error = dupstr(rc ? "transition callback failed" : "transition expectation failed");
            free(actual);
            break;
        }
        free(actual);
        seen[(size_t)picked] = 1;
        r->steps++;
        r->covered++;
    }
    for (size_t i = 0; i < g->transition_count; i++) {
        int source = find_state(g, g->transitions[i].from);
        if (source < 0 || !reachable[(size_t)source] || !seen[i]) r->uncovered++;
    }
    free(queue); free(reachable); free(seen);
    if (!r->failures && r->uncovered) { r->error = dupstr("uncovered transition"); return -1; }
    return r->failures ? -1 : 0;
}

int wct_run_relation(const wct_relation_graph *g, wct_call_fn fn, void *ctx,
                    wct_limits lim, wct_report *r) {
    if (!r) return -1;
    memset(r, 0, sizeof *r);
    r->seed = lim.seed;
    char err[128];
    if (wct_validate_relation(g, err, sizeof err)) {
        r->error = dupstr(err);
        return -1;
    }
    size_t n = g->call_count;
    size_t max = lim.max_flows ? lim.max_flows : n;
    unsigned char *done = calloc(n ? n : 1, 1);
    char **res = calloc(n ? n : 1, sizeof *res);
    if (!done || !res) {
        free(done); free(res); r->error = dupstr("out of memory"); return -1;
    }
    for (size_t step = 0; step < max; step++) {
        int pick = -1;
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
            if (!ready) continue;
            if (pick < 0 || strcmp(g->calls[i].id, g->calls[(size_t)pick].id) < 0)
                pick = (int)i;
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
        int rc = fn ? fn(c->id, args, c->argc, &out, ctx) : 0;
        free(args);
        if (rc || !out) {
            r->failures++;
            r->failed_step = step + 1;
            r->scenario = dupstr(c->id);
            r->error = dupstr(rc ? "call callback failed" : "call callback returned no result");
            free(out);
            break;
        }
        res[(size_t)pick] = out;
        done[(size_t)pick] = 1;
        r->steps++;
        r->covered++;
    }
    for (size_t i = 0; i < n; i++) if (!done[i]) r->uncovered++;
    for (size_t i = 0; i < n; i++) free(res[i]);
    free(res);
    free(done);
    return r->failures ? -1 : 0;
}
