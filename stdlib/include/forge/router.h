/*
 * Forge Stdlib - Client-Side Router
 *
 * Hash-based and History API router for single-page apps.
 *
 * Usage:
 *   #include <forge/router.h>
 *
 *   forge_router_add("/",        home_handler,    NULL);
 *   forge_router_add("/about",   about_handler,   NULL);
 *   forge_router_add("/user/:id",user_handler,    NULL);
 *   forge_router_start();
 *
 *   void home_handler(forge_route_t *route, void *ctx) {
 *       // render home component
 *   }
 */

#ifndef FORGE_ROUTER_H
#define FORGE_ROUTER_H

#include <forge/types.h>

#define FORGE_ROUTER_MAX_ROUTES  64
#define FORGE_ROUTER_MAX_PARAMS  8
#define FORGE_ROUTER_PARAM_LEN   64

/* ─── Route Match ──────────────────────────────────────────────────────────── */

typedef struct {
    char  path[256];
    char  param_names[FORGE_ROUTER_MAX_PARAMS][FORGE_ROUTER_PARAM_LEN];
    char  param_vals [FORGE_ROUTER_MAX_PARAMS][FORGE_ROUTER_PARAM_LEN];
    int   param_count;
    void *userdata;
} forge_route_t;

typedef void (*forge_route_handler)(const forge_route_t *route, void *userdata);

/* ─── Router Mode ──────────────────────────────────────────────────────────── */

typedef enum {
    FORGE_ROUTER_HASH,     /* #/path — works without server config */
    FORGE_ROUTER_HISTORY,  /* /path  — requires server rewrite rule */
} ForgeRouterMode;

/* ─── Public API ───────────────────────────────────────────────────────────── */

void forge_router_init(ForgeRouterMode mode);
void forge_router_add(const char *pattern, forge_route_handler handler, void *userdata);
void forge_router_start(void);
void forge_router_navigate(const char *path);
void forge_router_back(void);
void forge_router_forward(void);

/* Get current path */
const char *forge_router_current_path(void);

/* Register a not-found handler */
void forge_router_not_found(forge_route_handler handler, void *userdata);

#endif /* FORGE_ROUTER_H */
