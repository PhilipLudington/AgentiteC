/*
 * Agentite Particle System Implementation
 *
 * High-performance particle system using pool allocation and batch rendering.
 */

#include "agentite/agentite.h"
#include "agentite/particle.h"
#include "agentite/error.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ============================================================================
 * Internal Types
 * ============================================================================ */

/* Individual particle state */
typedef struct Particle {
    float x, y;              /* Position */
    float vx, vy;            /* Velocity */
    float ax, ay;            /* Acceleration */
    float lifetime;          /* Current lifetime remaining */
    float max_lifetime;      /* Original lifetime (for interpolation) */
    float size;              /* Current size */
    float start_size;        /* Initial size */
    float end_size;          /* Final size */
    float rotation;          /* Current rotation in degrees */
    float angular_velocity;  /* Rotation speed */
    Agentite_Color color;    /* Current color */
    Agentite_Color start_color; /* Initial color */
    Agentite_Color end_color;   /* Final color */
    float gravity;           /* Per-particle gravity */
    float drag;              /* Air resistance */
    uint32_t frame;          /* Current animation frame */
    float frame_time;        /* Time in current frame */
    bool active;             /* Is this slot in use? */

    /* Track which emitter spawned this particle (for local space) */
    Agentite_ParticleEmitter *emitter;
} Particle;

/* Emitter state */
struct Agentite_ParticleEmitter {
    Agentite_ParticleSystem *system;
    Agentite_ParticleEmitterConfig config;

    /* Transform */
    float x, y;
    float rotation;
    float scale_x, scale_y;

    /* Emission state */
    bool active;
    bool paused;
    bool finished;
    float emit_accumulator;  /* Accumulated time for emission */
    float burst_timer;       /* Timer for burst intervals */
    float duration_elapsed;  /* Time elapsed (for timed mode) */
    uint32_t particle_count; /* Active particles from this emitter */

    /* Linked list for system management */
    Agentite_ParticleEmitter *next;
    Agentite_ParticleEmitter *prev;
};

/* Main particle system */
struct Agentite_ParticleSystem {
    Particle *particles;     /* Particle pool */
    uint32_t max_particles;
    uint32_t active_count;

    /* Emitter management */
    Agentite_ParticleEmitter *emitters_head;
    uint32_t max_emitters;
    uint32_t emitter_count;

    /* Default white texture for untextured particles */
    Agentite_Texture *default_texture;
};

/* ============================================================================
 * Random Number Helpers
 * ============================================================================ */

static float randf(void) {
    return (float)rand() / (float)RAND_MAX;
}

static float rand_range(float min, float max) {
    return min + randf() * (max - min);
}

static float deg_to_rad(float deg) {
    return deg * (float)(M_PI / 180.0);
}

/* ============================================================================
 * Easing Functions
 * ============================================================================ */

float agentite_ease(Agentite_EaseFunc func, float t) {
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;

    switch (func) {
        case AGENTITE_EASE_LINEAR:
            return t;

        case AGENTITE_EASE_IN_QUAD:
            return t * t;

        case AGENTITE_EASE_OUT_QUAD:
            return t * (2.0f - t);

        case AGENTITE_EASE_IN_OUT_QUAD:
            if (t < 0.5f) return 2.0f * t * t;
            return -1.0f + (4.0f - 2.0f * t) * t;

        case AGENTITE_EASE_IN_CUBIC:
            return t * t * t;

        case AGENTITE_EASE_OUT_CUBIC: {
            float f = t - 1.0f;
            return f * f * f + 1.0f;
        }

        case AGENTITE_EASE_IN_OUT_CUBIC:
            if (t < 0.5f) return 4.0f * t * t * t;
            return (t - 1.0f) * (2.0f * t - 2.0f) * (2.0f * t - 2.0f) + 1.0f;

        case AGENTITE_EASE_IN_EXPO:
            return powf(2.0f, 10.0f * (t - 1.0f));

        case AGENTITE_EASE_OUT_EXPO:
            return 1.0f - powf(2.0f, -10.0f * t);

        case AGENTITE_EASE_IN_OUT_EXPO:
            if (t < 0.5f) return 0.5f * powf(2.0f, 20.0f * t - 10.0f);
            return 1.0f - 0.5f * powf(2.0f, -20.0f * t + 10.0f);

        default:
            return t;
    }
}

/* ============================================================================
 * Color Utilities
 * ============================================================================ */

Agentite_Color agentite_color_lerp(Agentite_Color a, Agentite_Color b, float t) {
    return (Agentite_Color){
        a.r + (b.r - a.r) * t,
        a.g + (b.g - a.g) * t,
        a.b + (b.b - a.b) * t,
        a.a + (b.a - a.a) * t
    };
}

Agentite_Color agentite_color_from_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (Agentite_Color){
        r / 255.0f,
        g / 255.0f,
        b / 255.0f,
        a / 255.0f
    };
}

Agentite_Color agentite_color_from_hex(uint32_t hex) {
    if (hex <= 0xFFFFFF) {
        /* RGB format: 0xRRGGBB */
        return (Agentite_Color){
            ((hex >> 16) & 0xFF) / 255.0f,
            ((hex >> 8) & 0xFF) / 255.0f,
            (hex & 0xFF) / 255.0f,
            1.0f
        };
    }
    /* RGBA format: 0xRRGGBBAA */
    return (Agentite_Color){
        ((hex >> 24) & 0xFF) / 255.0f,
        ((hex >> 16) & 0xFF) / 255.0f,
        ((hex >> 8) & 0xFF) / 255.0f,
        (hex & 0xFF) / 255.0f
    };
}

/* ============================================================================
 * Particle System Lifecycle
 * ============================================================================ */

Agentite_ParticleSystem *agentite_particle_system_create(
    const Agentite_ParticleSystemConfig *config)
{
    Agentite_ParticleSystemConfig cfg = AGENTITE_PARTICLE_SYSTEM_DEFAULT;
    if (config) {
        cfg = *config;
    }

    if (cfg.max_particles == 0) cfg.max_particles = 10000;
    if (cfg.max_emitters == 0) cfg.max_emitters = 64;

    Agentite_ParticleSystem *ps = AGENTITE_ALLOC(Agentite_ParticleSystem);
    if (!ps) {
        agentite_set_error("Failed to allocate particle system");
        return NULL;
    }

    ps->particles = AGENTITE_ALLOC_ARRAY(Particle, cfg.max_particles);
    if (!ps->particles) {
        agentite_set_error("Failed to allocate particle pool (%u particles)", cfg.max_particles);
        free(ps);
        return NULL;
    }

    ps->max_particles = cfg.max_particles;
    ps->active_count = 0;
    ps->emitters_head = NULL;
    ps->max_emitters = cfg.max_emitters;
    ps->emitter_count = 0;
    ps->default_texture = NULL;

    return ps;
}

void agentite_particle_system_destroy(Agentite_ParticleSystem *ps) {
    if (!ps) return;

    /* Destroy all emitters */
    Agentite_ParticleEmitter *emitter = ps->emitters_head;
    while (emitter) {
        Agentite_ParticleEmitter *next = emitter->next;
        free(emitter);
        emitter = next;
    }

    free(ps->particles);
    free(ps);
}

void agentite_particle_system_clear(Agentite_ParticleSystem *ps) {
    if (!ps) return;

    /* Mark all particles as inactive */
    for (uint32_t i = 0; i < ps->max_particles; i++) {
        ps->particles[i].active = false;
    }
    ps->active_count = 0;

    /* Reset emitter particle counts */
    Agentite_ParticleEmitter *emitter = ps->emitters_head;
    while (emitter) {
        emitter->particle_count = 0;
        emitter = emitter->next;
    }
}

uint32_t agentite_particle_system_get_count(const Agentite_ParticleSystem *ps) {
    return ps ? ps->active_count : 0;
}

uint32_t agentite_particle_system_get_capacity(const Agentite_ParticleSystem *ps) {
    return ps ? ps->max_particles : 0;
}

/* ============================================================================
 * Particle Spawning
 * ============================================================================ */

static Particle *find_free_particle(Agentite_ParticleSystem *ps) {
    /* Simple linear search for free slot */
    for (uint32_t i = 0; i < ps->max_particles; i++) {
        if (!ps->particles[i].active) {
            return &ps->particles[i];
        }
    }
    return NULL;
}

static void get_spawn_position(Agentite_ParticleEmitter *emitter, float *out_x, float *out_y) {
    const Agentite_ParticleEmitterConfig *cfg = &emitter->config;
    float local_x = 0.0f, local_y = 0.0f;

    switch (cfg->shape) {
        case AGENTITE_EMITTER_POINT:
            local_x = 0.0f;
            local_y = 0.0f;
            break;

        case AGENTITE_EMITTER_LINE: {
            float t = randf();
            local_x = t * cfg->line_end.x;
            local_y = t * cfg->line_end.y;
            break;
        }

        case AGENTITE_EMITTER_CIRCLE: {
            float angle = randf() * 2.0f * (float)M_PI;
            float radius = randf() * cfg->radius;
            local_x = cosf(angle) * radius;
            local_y = sinf(angle) * radius;
            break;
        }

        case AGENTITE_EMITTER_CIRCLE_EDGE: {
            float angle = randf() * 2.0f * (float)M_PI;
            local_x = cosf(angle) * cfg->radius;
            local_y = sinf(angle) * cfg->radius;
            break;
        }

        case AGENTITE_EMITTER_RECTANGLE: {
            local_x = rand_range(-cfg->width * 0.5f, cfg->width * 0.5f);
            local_y = rand_range(-cfg->height * 0.5f, cfg->height * 0.5f);
            break;
        }

        case AGENTITE_EMITTER_RECTANGLE_EDGE: {
            int side = rand() % 4;
            switch (side) {
                case 0: /* Top */
                    local_x = rand_range(-cfg->width * 0.5f, cfg->width * 0.5f);
                    local_y = -cfg->height * 0.5f;
                    break;
                case 1: /* Bottom */
                    local_x = rand_range(-cfg->width * 0.5f, cfg->width * 0.5f);
                    local_y = cfg->height * 0.5f;
                    break;
                case 2: /* Left */
                    local_x = -cfg->width * 0.5f;
                    local_y = rand_range(-cfg->height * 0.5f, cfg->height * 0.5f);
                    break;
                case 3: /* Right */
                    local_x = cfg->width * 0.5f;
                    local_y = rand_range(-cfg->height * 0.5f, cfg->height * 0.5f);
                    break;
            }
            break;
        }
    }

    /* Apply emitter scale and rotation */
    local_x *= emitter->scale_x;
    local_y *= emitter->scale_y;

    if (emitter->rotation != 0.0f) {
        float cos_r = cosf(deg_to_rad(emitter->rotation));
        float sin_r = sinf(deg_to_rad(emitter->rotation));
        float rx = local_x * cos_r - local_y * sin_r;
        float ry = local_x * sin_r + local_y * cos_r;
        local_x = rx;
        local_y = ry;
    }

    *out_x = emitter->x + local_x;
    *out_y = emitter->y + local_y;
}

static void spawn_particle(Agentite_ParticleEmitter *emitter) {
    Agentite_ParticleSystem *ps = emitter->system;
    const Agentite_ParticleConfig *pcfg = &emitter->config.particle;

    Particle *p = find_free_particle(ps);
    if (!p) return;

    /* Position */
    get_spawn_position(emitter, &p->x, &p->y);

    /* Velocity */
    float speed = rand_range(pcfg->speed_min, pcfg->speed_max);
    float base_dir = rand_range(pcfg->direction_min, pcfg->direction_max);
    float spread = rand_range(-pcfg->spread * 0.5f, pcfg->spread * 0.5f);
    float dir = base_dir + spread + emitter->rotation;
    float dir_rad = deg_to_rad(dir);
    p->vx = cosf(dir_rad) * speed;
    p->vy = sinf(dir_rad) * speed;

    /* Acceleration */
    p->ax = pcfg->acceleration.x;
    p->ay = pcfg->acceleration.y;
    p->gravity = pcfg->gravity;
    p->drag = pcfg->drag;

    /* Lifetime */
    p->lifetime = rand_range(pcfg->lifetime_min, pcfg->lifetime_max);
    p->max_lifetime = p->lifetime;

    /* Size */
    p->start_size = rand_range(pcfg->start_size_min, pcfg->start_size_max);
    p->end_size = rand_range(pcfg->end_size_min, pcfg->end_size_max);
    p->size = p->start_size;

    /* Color */
    if (pcfg->randomize_start_color) {
        p->start_color = agentite_color_lerp(pcfg->start_color, pcfg->start_color_alt, randf());
    } else {
        p->start_color = pcfg->start_color;
    }
    p->end_color = pcfg->end_color;
    p->color = p->start_color;

    /* Rotation */
    p->rotation = rand_range(pcfg->start_rotation_min, pcfg->start_rotation_max);
    p->angular_velocity = rand_range(pcfg->angular_velocity_min, pcfg->angular_velocity_max);

    /* Animation */
    if (pcfg->random_start_frame && pcfg->frame_count > 1) {
        p->frame = (uint32_t)(rand() % pcfg->frame_count);
    } else {
        p->frame = 0;
    }
    p->frame_time = 0.0f;

    /* Emitter reference (for local space) */
    p->emitter = emitter;

    /* Activate */
    p->active = true;
    ps->active_count++;
    emitter->particle_count++;
}

/* ============================================================================
 * Emitter Lifecycle
 * ============================================================================ */

Agentite_ParticleEmitter *agentite_particle_emitter_create(
    Agentite_ParticleSystem *ps,
    const Agentite_ParticleEmitterConfig *config)
{
    if (!ps) {
        agentite_set_error("Particle system is NULL");
        return NULL;
    }

    if (ps->emitter_count >= ps->max_emitters) {
        agentite_set_error("Maximum emitter count reached (%u)", ps->max_emitters);
        return NULL;
    }

    Agentite_ParticleEmitter *emitter = AGENTITE_ALLOC(Agentite_ParticleEmitter);
    if (!emitter) {
        agentite_set_error("Failed to allocate emitter");
        return NULL;
    }

    emitter->system = ps;

    if (config) {
        emitter->config = *config;
    } else {
        Agentite_ParticleEmitterConfig default_cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
        emitter->config = default_cfg;
    }

    /* Initialize transform */
    emitter->x = 0.0f;
    emitter->y = 0.0f;
    emitter->rotation = 0.0f;
    emitter->scale_x = 1.0f;
    emitter->scale_y = 1.0f;

    /* Initialize state */
    emitter->active = false;
    emitter->paused = false;
    emitter->finished = false;
    emitter->emit_accumulator = 0.0f;
    emitter->burst_timer = 0.0f;
    emitter->duration_elapsed = 0.0f;
    emitter->particle_count = 0;

    /* Add to linked list */
    emitter->next = ps->emitters_head;
    emitter->prev = NULL;
    if (ps->emitters_head) {
        ps->emitters_head->prev = emitter;
    }
    ps->emitters_head = emitter;
    ps->emitter_count++;

    /* Prewarm if requested */
    if (emitter->config.prewarm) {
        emitter->active = true;
        /* Simulate a few seconds of particles */
        float prewarm_time = 2.0f;
        float dt = 1.0f / 60.0f;
        for (float t = 0.0f; t < prewarm_time; t += dt) {
            /* Emit particles */
            if (emitter->config.mode == AGENTITE_EMISSION_CONTINUOUS) {
                emitter->emit_accumulator += dt;
                float interval = 1.0f / emitter->config.emission_rate;
                while (emitter->emit_accumulator >= interval) {
                    emitter->emit_accumulator -= interval;
                    spawn_particle(emitter);
                }
            }

            /* Update particles */
            for (uint32_t i = 0; i < ps->max_particles; i++) {
                Particle *p = &ps->particles[i];
                if (!p->active) continue;

                p->lifetime -= dt;
                if (p->lifetime <= 0.0f) {
                    p->active = false;
                    ps->active_count--;
                    if (p->emitter) p->emitter->particle_count--;
                    continue;
                }

                /* Simple physics */
                p->vy += p->gravity * dt;
                p->vx += p->ax * dt;
                p->vy += p->ay * dt;
                p->x += p->vx * dt;
                p->y += p->vy * dt;
            }
        }
        emitter->active = false;
    }

    return emitter;
}

void agentite_particle_emitter_destroy(Agentite_ParticleEmitter *emitter) {
    if (!emitter) return;

    Agentite_ParticleSystem *ps = emitter->system;

    /* Remove from linked list */
    if (emitter->prev) {
        emitter->prev->next = emitter->next;
    } else {
        ps->emitters_head = emitter->next;
    }
    if (emitter->next) {
        emitter->next->prev = emitter->prev;
    }
    ps->emitter_count--;

    /* Clear emitter reference from particles (they'll continue to live) */
    for (uint32_t i = 0; i < ps->max_particles; i++) {
        if (ps->particles[i].emitter == emitter) {
            ps->particles[i].emitter = NULL;
        }
    }

    free(emitter);
}

/* ============================================================================
 * Emitter Control
 * ============================================================================ */

void agentite_particle_emitter_start(Agentite_ParticleEmitter *emitter) {
    if (!emitter) return;
    emitter->active = true;
    emitter->paused = false;
    emitter->finished = false;
}

void agentite_particle_emitter_stop(Agentite_ParticleEmitter *emitter) {
    if (!emitter) return;
    emitter->active = false;
}

void agentite_particle_emitter_pause(Agentite_ParticleEmitter *emitter) {
    if (!emitter) return;
    emitter->paused = true;
}

void agentite_particle_emitter_resume(Agentite_ParticleEmitter *emitter) {
    if (!emitter) return;
    emitter->paused = false;
}

void agentite_particle_emitter_reset(Agentite_ParticleEmitter *emitter) {
    if (!emitter) return;
    emitter->emit_accumulator = 0.0f;
    emitter->burst_timer = 0.0f;
    emitter->duration_elapsed = 0.0f;
    emitter->finished = false;
}

void agentite_particle_emitter_burst(Agentite_ParticleEmitter *emitter, uint32_t count) {
    if (!emitter) return;

    if (count == 0) {
        count = emitter->config.burst_count;
    }

    for (uint32_t i = 0; i < count; i++) {
        spawn_particle(emitter);
    }
}

bool agentite_particle_emitter_is_active(const Agentite_ParticleEmitter *emitter) {
    return emitter ? emitter->active && !emitter->paused : false;
}

bool agentite_particle_emitter_is_finished(const Agentite_ParticleEmitter *emitter) {
    return emitter ? emitter->finished : true;
}

uint32_t agentite_particle_emitter_get_count(const Agentite_ParticleEmitter *emitter) {
    return emitter ? emitter->particle_count : 0;
}

/* ============================================================================
 * Emitter Transform
 * ============================================================================ */

void agentite_particle_emitter_set_position(Agentite_ParticleEmitter *emitter, float x, float y) {
    if (!emitter) return;
    emitter->x = x;
    emitter->y = y;
}

void agentite_particle_emitter_get_position(const Agentite_ParticleEmitter *emitter, float *x, float *y) {
    if (!emitter) return;
    if (x) *x = emitter->x;
    if (y) *y = emitter->y;
}

void agentite_particle_emitter_set_rotation(Agentite_ParticleEmitter *emitter, float degrees) {
    if (!emitter) return;
    emitter->rotation = degrees;
}

void agentite_particle_emitter_set_scale(Agentite_ParticleEmitter *emitter, float scale_x, float scale_y) {
    if (!emitter) return;
    emitter->scale_x = scale_x;
    emitter->scale_y = scale_y;
}

/* ============================================================================
 * Emitter Properties (Runtime Modification)
 * ============================================================================ */

void agentite_particle_emitter_set_rate(Agentite_ParticleEmitter *emitter, float rate) {
    if (!emitter || rate <= 0.0f) return;
    emitter->config.emission_rate = rate;
}

void agentite_particle_emitter_set_mode(Agentite_ParticleEmitter *emitter, Agentite_EmissionMode mode) {
    if (!emitter) return;
    emitter->config.mode = mode;
}

void agentite_particle_emitter_set_texture(Agentite_ParticleEmitter *emitter, Agentite_Texture *texture) {
    if (!emitter) return;
    emitter->config.texture = texture;
    emitter->config.use_sprite = false;
}

void agentite_particle_emitter_set_sprite(Agentite_ParticleEmitter *emitter, const Agentite_Sprite *sprite) {
    if (!emitter || !sprite) return;
    emitter->config.sprite = *sprite;
    emitter->config.use_sprite = true;
}

void agentite_particle_emitter_set_blend(Agentite_ParticleEmitter *emitter, Agentite_ParticleBlend blend) {
    if (!emitter) return;
    emitter->config.blend = blend;
}

void agentite_particle_emitter_set_colors(Agentite_ParticleEmitter *emitter,
                                          Agentite_Color start, Agentite_Color end) {
    if (!emitter) return;
    emitter->config.particle.start_color = start;
    emitter->config.particle.end_color = end;
}

void agentite_particle_emitter_set_sizes(Agentite_ParticleEmitter *emitter,
                                         float start_min, float start_max,
                                         float end_min, float end_max) {
    if (!emitter) return;
    emitter->config.particle.start_size_min = start_min;
    emitter->config.particle.start_size_max = start_max;
    emitter->config.particle.end_size_min = end_min;
    emitter->config.particle.end_size_max = end_max;
}

void agentite_particle_emitter_set_gravity(Agentite_ParticleEmitter *emitter, float gravity) {
    if (!emitter) return;
    emitter->config.particle.gravity = gravity;
}

void agentite_particle_emitter_set_lifetime(Agentite_ParticleEmitter *emitter, float min, float max) {
    if (!emitter) return;
    emitter->config.particle.lifetime_min = min;
    emitter->config.particle.lifetime_max = max;
}

void agentite_particle_emitter_set_speed(Agentite_ParticleEmitter *emitter, float min, float max) {
    if (!emitter) return;
    emitter->config.particle.speed_min = min;
    emitter->config.particle.speed_max = max;
}

/* ============================================================================
 * System Update
 * ============================================================================ */

void agentite_particle_system_update(Agentite_ParticleSystem *ps, float dt) {
    if (!ps || dt <= 0.0f) return;

    /* Update all emitters */
    Agentite_ParticleEmitter *emitter = ps->emitters_head;
    while (emitter) {
        if (emitter->active && !emitter->paused && !emitter->finished) {
            const Agentite_ParticleEmitterConfig *cfg = &emitter->config;

            switch (cfg->mode) {
                case AGENTITE_EMISSION_CONTINUOUS: {
                    emitter->emit_accumulator += dt;
                    float interval = 1.0f / cfg->emission_rate;
                    while (emitter->emit_accumulator >= interval) {
                        emitter->emit_accumulator -= interval;
                        spawn_particle(emitter);
                    }
                    break;
                }

                case AGENTITE_EMISSION_BURST: {
                    if (cfg->burst_interval > 0.0f) {
                        emitter->burst_timer += dt;
                        if (emitter->burst_timer >= cfg->burst_interval) {
                            emitter->burst_timer = 0.0f;
                            agentite_particle_emitter_burst(emitter, 0);
                        }
                    }
                    break;
                }

                case AGENTITE_EMISSION_TIMED: {
                    emitter->duration_elapsed += dt;
                    if (emitter->duration_elapsed >= cfg->duration) {
                        emitter->finished = true;
                        emitter->active = false;
                    } else {
                        emitter->emit_accumulator += dt;
                        float interval = 1.0f / cfg->emission_rate;
                        while (emitter->emit_accumulator >= interval) {
                            emitter->emit_accumulator -= interval;
                            spawn_particle(emitter);
                        }
                    }
                    break;
                }
            }
        }
        emitter = emitter->next;
    }

    /* Update all particles */
    for (uint32_t i = 0; i < ps->max_particles; i++) {
        Particle *p = &ps->particles[i];
        if (!p->active) continue;

        /* Update lifetime */
        p->lifetime -= dt;
        if (p->lifetime <= 0.0f) {
            p->active = false;
            ps->active_count--;
            if (p->emitter) p->emitter->particle_count--;
            continue;
        }

        /* Calculate life progress (0 = just spawned, 1 = about to die) */
        float life_t = 1.0f - (p->lifetime / p->max_lifetime);

        /* Apply physics */
        p->vy += p->gravity * dt;
        p->vx += p->ax * dt;
        p->vy += p->ay * dt;

        /* Apply drag */
        if (p->drag > 0.0f) {
            float drag_factor = 1.0f - (p->drag * dt);
            if (drag_factor < 0.0f) drag_factor = 0.0f;
            p->vx *= drag_factor;
            p->vy *= drag_factor;
        }

        /* Update position */
        p->x += p->vx * dt;
        p->y += p->vy * dt;

        /* Interpolate size */
        Agentite_EaseFunc size_ease = AGENTITE_EASE_LINEAR;
        if (p->emitter) {
            size_ease = p->emitter->config.particle.size_ease;
        }
        float size_t = agentite_ease(size_ease, life_t);
        p->size = p->start_size + (p->end_size - p->start_size) * size_t;

        /* Interpolate color */
        Agentite_EaseFunc color_ease = AGENTITE_EASE_LINEAR;
        if (p->emitter) {
            color_ease = p->emitter->config.particle.color_ease;
        }
        float color_t = agentite_ease(color_ease, life_t);
        p->color = agentite_color_lerp(p->start_color, p->end_color, color_t);

        /* Update rotation */
        p->rotation += p->angular_velocity * dt;

        /* Update animation frame */
        if (p->emitter && p->emitter->config.particle.frame_count > 1) {
            const Agentite_ParticleConfig *pcfg = &p->emitter->config.particle;
            p->frame_time += dt;
            float frame_duration = 1.0f / pcfg->frame_rate;
            while (p->frame_time >= frame_duration) {
                p->frame_time -= frame_duration;
                p->frame++;
                if (p->frame >= pcfg->frame_count) {
                    if (pcfg->loop_animation) {
                        p->frame = 0;
                    } else {
                        p->frame = pcfg->frame_count - 1;
                    }
                }
            }
        }
    }
}

/* ============================================================================
 * Rendering
 * ============================================================================ */

void agentite_particle_system_draw(Agentite_ParticleSystem *ps, Agentite_SpriteRenderer *sr) {
    agentite_particle_system_draw_camera(ps, sr, NULL);
}

void agentite_particle_system_draw_camera(Agentite_ParticleSystem *ps,
                                          Agentite_SpriteRenderer *sr,
                                          Agentite_Camera *camera) {
    if (!ps || !sr) return;

    /* Set camera if provided */
    if (camera) {
        agentite_sprite_set_camera(sr, camera);
    }

    /* Draw all active particles */
    for (uint32_t i = 0; i < ps->max_particles; i++) {
        Particle *p = &ps->particles[i];
        if (!p->active) continue;

        /* Calculate world position */
        float draw_x = p->x;
        float draw_y = p->y;

        /* For local space, offset by emitter position */
        if (p->emitter && p->emitter->config.space == AGENTITE_PARTICLE_LOCAL) {
            draw_x += p->emitter->x;
            draw_y += p->emitter->y;
        }

        /* Get sprite to draw */
        Agentite_Sprite sprite = {0};
        if (p->emitter) {
            if (p->emitter->config.use_sprite) {
                sprite = p->emitter->config.sprite;
            } else if (p->emitter->config.texture) {
                sprite = agentite_sprite_from_texture(p->emitter->config.texture);
            }
        }

        /* Skip if no texture */
        if (!sprite.texture) {
            /* Use a simple colored square (would need default texture) */
            continue;
        }

        /* Handle animation frames */
        if (p->emitter && p->emitter->config.particle.frame_count > 1) {
            uint32_t frame_count = p->emitter->config.particle.frame_count;
            float frame_w = sprite.src_w / frame_count;
            sprite.src_x += p->frame * frame_w;
            sprite.src_w = frame_w;
        }

        /* Calculate scale from particle size */
        float scale = p->size / sprite.src_w;
        if (sprite.src_w == 0) scale = 1.0f;

        /* Draw with full transform */
        agentite_sprite_draw_full(sr, &sprite,
                                  draw_x, draw_y,
                                  scale, scale,
                                  p->rotation,
                                  0.5f, 0.5f,  /* Center origin */
                                  p->color.r, p->color.g, p->color.b, p->color.a);
    }

    /* Restore camera state */
    if (camera) {
        agentite_sprite_set_camera(sr, NULL);
    }
}

/* ============================================================================
 * Preset Emitters
 * ============================================================================ */

Agentite_ParticleEmitter *agentite_particle_preset_explosion(
    Agentite_ParticleSystem *ps,
    float x, float y,
    Agentite_Color color,
    float scale)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_POINT;
    cfg.mode = AGENTITE_EMISSION_BURST;
    cfg.burst_count = (uint32_t)(50 * scale);
    cfg.blend = AGENTITE_BLEND_ADDITIVE;

    cfg.particle.lifetime_min = 0.3f;
    cfg.particle.lifetime_max = 0.8f;
    cfg.particle.speed_min = 100.0f * scale;
    cfg.particle.speed_max = 300.0f * scale;
    cfg.particle.direction_min = 0.0f;
    cfg.particle.direction_max = 360.0f;
    cfg.particle.gravity = 200.0f;
    cfg.particle.drag = 0.5f;
    cfg.particle.start_size_min = 8.0f * scale;
    cfg.particle.start_size_max = 16.0f * scale;
    cfg.particle.end_size_min = 2.0f * scale;
    cfg.particle.end_size_max = 4.0f * scale;
    cfg.particle.start_color = color;
    cfg.particle.end_color = (Agentite_Color){color.r, color.g * 0.5f, 0.0f, 0.0f};
    cfg.particle.size_ease = AGENTITE_EASE_OUT_QUAD;
    cfg.particle.color_ease = AGENTITE_EASE_IN_QUAD;

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_position(emitter, x, y);
        agentite_particle_emitter_burst(emitter, 0);
    }
    return emitter;
}

Agentite_ParticleEmitter *agentite_particle_preset_smoke(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float rate)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_CIRCLE;
    cfg.radius = 10.0f;
    cfg.mode = AGENTITE_EMISSION_CONTINUOUS;
    cfg.emission_rate = rate;
    cfg.blend = AGENTITE_BLEND_ALPHA;

    cfg.particle.lifetime_min = 2.0f;
    cfg.particle.lifetime_max = 4.0f;
    cfg.particle.speed_min = 20.0f;
    cfg.particle.speed_max = 40.0f;
    cfg.particle.direction_min = 250.0f;  /* Upward with slight variation */
    cfg.particle.direction_max = 290.0f;
    cfg.particle.gravity = -20.0f;  /* Slight upward drift */
    cfg.particle.drag = 0.1f;
    cfg.particle.start_size_min = 16.0f;
    cfg.particle.start_size_max = 24.0f;
    cfg.particle.end_size_min = 48.0f;
    cfg.particle.end_size_max = 64.0f;
    cfg.particle.start_color = (Agentite_Color){0.5f, 0.5f, 0.5f, 0.6f};
    cfg.particle.end_color = (Agentite_Color){0.3f, 0.3f, 0.3f, 0.0f};
    cfg.particle.angular_velocity_min = -30.0f;
    cfg.particle.angular_velocity_max = 30.0f;
    cfg.particle.size_ease = AGENTITE_EASE_OUT_QUAD;

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_position(emitter, x, y);
    }
    return emitter;
}

Agentite_ParticleEmitter *agentite_particle_preset_fire(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float scale)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_CIRCLE;
    cfg.radius = 8.0f * scale;
    cfg.mode = AGENTITE_EMISSION_CONTINUOUS;
    cfg.emission_rate = 50.0f * scale;
    cfg.blend = AGENTITE_BLEND_ADDITIVE;

    cfg.particle.lifetime_min = 0.5f;
    cfg.particle.lifetime_max = 1.0f;
    cfg.particle.speed_min = 30.0f * scale;
    cfg.particle.speed_max = 60.0f * scale;
    cfg.particle.direction_min = 250.0f;
    cfg.particle.direction_max = 290.0f;
    cfg.particle.spread = 20.0f;
    cfg.particle.gravity = -100.0f * scale;  /* Strong upward */
    cfg.particle.drag = 0.2f;
    cfg.particle.start_size_min = 12.0f * scale;
    cfg.particle.start_size_max = 20.0f * scale;
    cfg.particle.end_size_min = 4.0f * scale;
    cfg.particle.end_size_max = 8.0f * scale;
    cfg.particle.start_color = (Agentite_Color){1.0f, 0.8f, 0.2f, 1.0f};
    cfg.particle.end_color = (Agentite_Color){1.0f, 0.2f, 0.0f, 0.0f};
    cfg.particle.randomize_start_color = true;
    cfg.particle.start_color_alt = (Agentite_Color){1.0f, 0.5f, 0.1f, 1.0f};
    cfg.particle.color_ease = AGENTITE_EASE_IN_QUAD;
    cfg.particle.angular_velocity_min = -90.0f;
    cfg.particle.angular_velocity_max = 90.0f;

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_position(emitter, x, y);
    }
    return emitter;
}

Agentite_ParticleEmitter *agentite_particle_preset_sparks(
    Agentite_ParticleSystem *ps,
    float x, float y,
    Agentite_Color color)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_POINT;
    cfg.mode = AGENTITE_EMISSION_BURST;
    cfg.burst_count = 20;
    cfg.blend = AGENTITE_BLEND_ADDITIVE;

    cfg.particle.lifetime_min = 0.3f;
    cfg.particle.lifetime_max = 0.6f;
    cfg.particle.speed_min = 150.0f;
    cfg.particle.speed_max = 300.0f;
    cfg.particle.direction_min = 0.0f;
    cfg.particle.direction_max = 360.0f;
    cfg.particle.gravity = 400.0f;
    cfg.particle.drag = 0.3f;
    cfg.particle.start_size_min = 2.0f;
    cfg.particle.start_size_max = 4.0f;
    cfg.particle.end_size_min = 1.0f;
    cfg.particle.end_size_max = 2.0f;
    cfg.particle.start_color = color;
    cfg.particle.end_color = (Agentite_Color){color.r, color.g, color.b, 0.0f};

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_position(emitter, x, y);
        agentite_particle_emitter_burst(emitter, 0);
    }
    return emitter;
}

Agentite_ParticleEmitter *agentite_particle_preset_rain(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float width, float height,
    float intensity)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_RECTANGLE;
    cfg.width = width;
    cfg.height = 1.0f;  /* Thin strip at top */
    cfg.mode = AGENTITE_EMISSION_CONTINUOUS;
    cfg.emission_rate = 100.0f * intensity;
    cfg.blend = AGENTITE_BLEND_ALPHA;

    float fall_time = height / 500.0f;  /* Time to fall through area */
    cfg.particle.lifetime_min = fall_time * 0.8f;
    cfg.particle.lifetime_max = fall_time * 1.2f;
    cfg.particle.speed_min = 450.0f;
    cfg.particle.speed_max = 550.0f;
    cfg.particle.direction_min = 80.0f;  /* Mostly down with slight angle */
    cfg.particle.direction_max = 100.0f;
    cfg.particle.gravity = 200.0f;
    cfg.particle.start_size_min = 2.0f;
    cfg.particle.start_size_max = 3.0f;
    cfg.particle.end_size_min = 2.0f;
    cfg.particle.end_size_max = 3.0f;
    cfg.particle.start_color = (Agentite_Color){0.7f, 0.7f, 0.9f, 0.6f};
    cfg.particle.end_color = (Agentite_Color){0.7f, 0.7f, 0.9f, 0.3f};

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_position(emitter, x, y);
    }
    return emitter;
}

Agentite_ParticleEmitter *agentite_particle_preset_snow(
    Agentite_ParticleSystem *ps,
    float x, float y,
    float width, float height,
    float intensity)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_RECTANGLE;
    cfg.width = width;
    cfg.height = 1.0f;
    cfg.mode = AGENTITE_EMISSION_CONTINUOUS;
    cfg.emission_rate = 30.0f * intensity;
    cfg.blend = AGENTITE_BLEND_ALPHA;

    float fall_time = height / 50.0f;
    cfg.particle.lifetime_min = fall_time * 0.8f;
    cfg.particle.lifetime_max = fall_time * 1.2f;
    cfg.particle.speed_min = 30.0f;
    cfg.particle.speed_max = 60.0f;
    cfg.particle.direction_min = 70.0f;
    cfg.particle.direction_max = 110.0f;
    cfg.particle.acceleration = (Agentite_Vec2){0.0f, 0.0f};
    cfg.particle.gravity = 20.0f;
    cfg.particle.drag = 0.05f;
    cfg.particle.start_size_min = 4.0f;
    cfg.particle.start_size_max = 8.0f;
    cfg.particle.end_size_min = 4.0f;
    cfg.particle.end_size_max = 8.0f;
    cfg.particle.start_color = AGENTITE_COLOR_WHITE;
    cfg.particle.end_color = (Agentite_Color){1.0f, 1.0f, 1.0f, 0.5f};
    cfg.particle.angular_velocity_min = -45.0f;
    cfg.particle.angular_velocity_max = 45.0f;

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_position(emitter, x, y);
    }
    return emitter;
}

Agentite_ParticleEmitter *agentite_particle_preset_trail(
    Agentite_ParticleSystem *ps,
    Agentite_Color color,
    float size)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_POINT;
    cfg.mode = AGENTITE_EMISSION_CONTINUOUS;
    cfg.emission_rate = 60.0f;
    cfg.space = AGENTITE_PARTICLE_WORLD;  /* Trail stays in place */
    cfg.blend = AGENTITE_BLEND_ADDITIVE;

    cfg.particle.lifetime_min = 0.2f;
    cfg.particle.lifetime_max = 0.4f;
    cfg.particle.speed_min = 0.0f;
    cfg.particle.speed_max = 10.0f;
    cfg.particle.direction_min = 0.0f;
    cfg.particle.direction_max = 360.0f;
    cfg.particle.start_size_min = size;
    cfg.particle.start_size_max = size * 1.2f;
    cfg.particle.end_size_min = size * 0.2f;
    cfg.particle.end_size_max = size * 0.4f;
    cfg.particle.start_color = color;
    cfg.particle.end_color = (Agentite_Color){color.r, color.g, color.b, 0.0f};
    cfg.particle.size_ease = AGENTITE_EASE_OUT_QUAD;
    cfg.particle.color_ease = AGENTITE_EASE_IN_QUAD;

    return agentite_particle_emitter_create(ps, &cfg);
}

Agentite_ParticleEmitter *agentite_particle_preset_dust(
    Agentite_ParticleSystem *ps,
    float x, float y,
    Agentite_Color color)
{
    Agentite_ParticleEmitterConfig cfg = AGENTITE_PARTICLE_EMITTER_DEFAULT;
    cfg.shape = AGENTITE_EMITTER_CIRCLE;
    cfg.radius = 20.0f;
    cfg.mode = AGENTITE_EMISSION_BURST;
    cfg.burst_count = 15;
    cfg.blend = AGENTITE_BLEND_ALPHA;

    cfg.particle.lifetime_min = 0.8f;
    cfg.particle.lifetime_max = 1.5f;
    cfg.particle.speed_min = 20.0f;
    cfg.particle.speed_max = 60.0f;
    cfg.particle.direction_min = 200.0f;
    cfg.particle.direction_max = 340.0f;
    cfg.particle.gravity = 100.0f;
    cfg.particle.drag = 0.3f;
    cfg.particle.start_size_min = 4.0f;
    cfg.particle.start_size_max = 8.0f;
    cfg.particle.end_size_min = 2.0f;
    cfg.particle.end_size_max = 4.0f;
    cfg.particle.start_color = color;
    cfg.particle.end_color = (Agentite_Color){color.r, color.g, color.b, 0.0f};

    Agentite_ParticleEmitter *emitter = agentite_particle_emitter_create(ps, &cfg);
    if (emitter) {
        agentite_particle_emitter_set_position(emitter, x, y);
        agentite_particle_emitter_burst(emitter, 0);
    }
    return emitter;
}
