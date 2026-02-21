/*
 * Forge Stdlib - Animation Utilities
 *
 * RequestAnimationFrame-based animations and transitions.
 *
 * Usage:
 *   #include <forge/animate.h>
 *
 *   // Animate a value from 0 to 100 over 300ms with ease-out
 *   forge_tween_t *t = forge_tween(0, 100, 300, FORGE_EASE_OUT,
 *                                  my_update_fn, my_done_fn, ctx);
 *
 *   void my_update_fn(float value, void *ctx) {
 *       ((MyState*)ctx)->opacity = value;
 *       forge_schedule_update(forge_ctx_get(el_id));
 *   }
 */

#ifndef FORGE_ANIMATE_H
#define FORGE_ANIMATE_H

#include <forge/types.h>

/* ─── Easing Functions ─────────────────────────────────────────────────────── */

typedef enum {
    FORGE_EASE_LINEAR,
    FORGE_EASE_IN,
    FORGE_EASE_OUT,
    FORGE_EASE_IN_OUT,
    FORGE_EASE_SPRING,    /* spring physics */
    FORGE_EASE_BOUNCE,
    FORGE_EASE_ELASTIC,
} ForgeEasing;

float forge_ease(float t, ForgeEasing easing);

/* ─── Tween ────────────────────────────────────────────────────────────────── */

typedef void (*forge_tween_update_fn)(float value, void *userdata);
typedef void (*forge_tween_done_fn)  (void *userdata);

typedef struct forge_tween {
    float              from;
    float              to;
    float              duration_ms;
    float              elapsed_ms;
    ForgeEasing        easing;
    forge_tween_update_fn on_update;
    forge_tween_done_fn   on_done;
    void              *userdata;
    int                running;
    int                id;
} forge_tween_t;

forge_tween_t *forge_tween(float from, float to, float duration_ms,
                            ForgeEasing easing,
                            forge_tween_update_fn on_update,
                            forge_tween_done_fn on_done,
                            void *userdata);

void forge_tween_cancel(forge_tween_t *t);
void forge_tween_tick(float delta_ms);   /* called by runtime RAF loop */

/* ─── Spring ───────────────────────────────────────────────────────────────── */

typedef struct {
    float value;
    float velocity;
    float target;
    float stiffness;  /* default: 170 */
    float damping;    /* default: 26  */
    float mass;       /* default: 1   */
    forge_tween_update_fn on_update;
    void *userdata;
} forge_spring_t;

forge_spring_t *forge_spring(float initial, float stiffness, float damping,
                              forge_tween_update_fn on_update, void *userdata);
void forge_spring_set_target(forge_spring_t *s, float target);
void forge_spring_tick(forge_spring_t *s, float delta_ms);

/* ─── CSS Transition Helper ────────────────────────────────────────────────── */

/* Emit inline CSS transition on a DOM node */
void forge_dom_transition(forge_dom_node_t *el, const char *property,
                           float duration_ms, ForgeEasing easing);

/* ─── Keyframe Sequence ────────────────────────────────────────────────────── */

typedef struct {
    float  time;   /* 0.0 - 1.0 (normalized position in animation) */
    float  value;
} ForgeKeyframe;

float forge_keyframe_sample(const ForgeKeyframe *frames, int count, float t,
                             ForgeEasing easing);

#endif /* FORGE_ANIMATE_H */
