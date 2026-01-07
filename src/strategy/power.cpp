/*
 * Agentite Power Network System
 *
 * Grid-based power distribution for factory and strategy games.
 * Uses Union-Find for efficient network connectivity tracking.
 *
 * Ported from AgentiteZ (Zig) power system.
 */

#include "agentite/agentite.h"
#include "agentite/power.h"
#include "agentite/error.h"
#include "agentite/validate.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>

/*============================================================================
 * Internal Structures
 *============================================================================*/

/* Union-Find node for network tracking */
typedef struct {
    int parent;
    int rank;
} UFNode;

struct Agentite_PowerSystem {
    /* Grid dimensions */
    int grid_width;
    int grid_height;

    /* Poles */
    Agentite_PowerPole poles[AGENTITE_POWER_MAX_POLES];
    bool pole_active[AGENTITE_POWER_MAX_POLES];
    int pole_count;

    /* Producers */
    Agentite_PowerProducer producers[AGENTITE_POWER_MAX_NODES];
    bool producer_active[AGENTITE_POWER_MAX_NODES];
    int producer_count;

    /* Consumers */
    Agentite_PowerConsumer consumers[AGENTITE_POWER_MAX_NODES];
    bool consumer_active[AGENTITE_POWER_MAX_NODES];
    int consumer_count;

    /* Union-Find for network connectivity */
    UFNode uf_nodes[AGENTITE_POWER_MAX_POLES];

    /* Network tracking */
    int network_ids[AGENTITE_POWER_MAX_NETWORKS];
    int network_count;

    /* Configuration */
    float connection_multiplier;
    float brownout_threshold;

    /* Callbacks */
    Agentite_PowerCallback callback;
    void *callback_userdata;

    /* Dirty flag for recalculation */
    bool needs_recalc;
};

/*============================================================================
 * Union-Find Implementation
 *============================================================================*/

static void uf_init(Agentite_PowerSystem *ps) {
    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        ps->uf_nodes[i].parent = i;
        ps->uf_nodes[i].rank = 0;
    }
}

static int uf_find(Agentite_PowerSystem *ps, int x) {
    if (ps->uf_nodes[x].parent != x) {
        ps->uf_nodes[x].parent = uf_find(ps, ps->uf_nodes[x].parent);  /* Path compression */
    }
    return ps->uf_nodes[x].parent;
}

static void uf_union(Agentite_PowerSystem *ps, int x, int y) {
    int px = uf_find(ps, x);
    int py = uf_find(ps, y);

    if (px == py) return;

    /* Union by rank */
    if (ps->uf_nodes[px].rank < ps->uf_nodes[py].rank) {
        ps->uf_nodes[px].parent = py;
    } else if (ps->uf_nodes[px].rank > ps->uf_nodes[py].rank) {
        ps->uf_nodes[py].parent = px;
    } else {
        ps->uf_nodes[py].parent = px;
        ps->uf_nodes[px].rank++;
    }
}

/*============================================================================
 * Internal Helpers
 *============================================================================*/

static int distance_squared(int x1, int y1, int x2, int y2) {
    int dx = x2 - x1;
    int dy = y2 - y1;
    return dx * dx + dy * dy;
}

static bool poles_can_connect(const Agentite_PowerSystem *ps, int pole_a, int pole_b) {
    const Agentite_PowerPole *a = &ps->poles[pole_a];
    const Agentite_PowerPole *b = &ps->poles[pole_b];

    /* Connection range is based on larger pole's radius */
    int range_a = (int)(a->radius * ps->connection_multiplier);
    int range_b = (int)(b->radius * ps->connection_multiplier);

    /* Substations have extra connection range */
    if (a->is_substation) range_a = (int)(range_a * 1.5f);
    if (b->is_substation) range_b = (int)(range_b * 1.5f);

    int max_range = range_a > range_b ? range_a : range_b;
    int dist_sq = distance_squared(a->x, a->y, b->x, b->y);

    return dist_sq <= max_range * max_range;
}

static bool point_in_range(int px, int py, int cx, int cy, int radius) {
    int dist_sq = distance_squared(px, py, cx, cy);
    return dist_sq <= radius * radius;
}

static void rebuild_networks(Agentite_PowerSystem *ps) {
    /* Reset union-find */
    uf_init(ps);

    /* Connect poles that are in range */
    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        if (!ps->pole_active[i]) continue;

        for (int j = i + 1; j < AGENTITE_POWER_MAX_POLES; j++) {
            if (!ps->pole_active[j]) continue;

            if (poles_can_connect(ps, i, j)) {
                uf_union(ps, i, j);
            }
        }
    }

    /* Assign network IDs to poles */
    ps->network_count = 0;
    memset(ps->network_ids, -1, sizeof(ps->network_ids));

    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        if (!ps->pole_active[i]) continue;

        int root = uf_find(ps, i);

        /* Check if this root already has a network ID */
        int network_id = -1;
        for (int n = 0; n < ps->network_count; n++) {
            if (ps->network_ids[n] == root) {
                network_id = n;
                break;
            }
        }

        if (network_id < 0 && ps->network_count < AGENTITE_POWER_MAX_NETWORKS) {
            network_id = ps->network_count;
            ps->network_ids[ps->network_count++] = root;
        }

        ps->poles[i].network_id = network_id;
    }

    /* Assign producers and consumers to networks */
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (!ps->producer_active[i]) continue;

        Agentite_PowerProducer *prod = &ps->producers[i];
        prod->network_id = -1;

        /* Find covering pole */
        for (int p = 0; p < AGENTITE_POWER_MAX_POLES; p++) {
            if (!ps->pole_active[p]) continue;

            const Agentite_PowerPole *pole = &ps->poles[p];
            if (point_in_range(prod->x, prod->y, pole->x, pole->y, pole->radius)) {
                prod->network_id = pole->network_id;
                break;
            }
        }
    }

    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (!ps->consumer_active[i]) continue;

        Agentite_PowerConsumer *cons = &ps->consumers[i];
        cons->network_id = -1;

        /* Find covering pole */
        for (int p = 0; p < AGENTITE_POWER_MAX_POLES; p++) {
            if (!ps->pole_active[p]) continue;

            const Agentite_PowerPole *pole = &ps->poles[p];
            if (point_in_range(cons->x, cons->y, pole->x, pole->y, pole->radius)) {
                cons->network_id = pole->network_id;
                break;
            }
        }
    }

    ps->needs_recalc = false;
}

static void update_consumer_satisfaction(Agentite_PowerSystem *ps) {
    for (int n = 0; n < ps->network_count; n++) {
        /* Calculate network totals */
        int production = 0;
        int consumption = 0;

        for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
            if (ps->producer_active[i] && ps->producers[i].network_id == n &&
                ps->producers[i].active) {
                production += ps->producers[i].production;
            }
        }

        for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
            if (ps->consumer_active[i] && ps->consumers[i].network_id == n &&
                ps->consumers[i].active) {
                consumption += ps->consumers[i].consumption;
            }
        }

        /* Calculate satisfaction ratio */
        float ratio = consumption > 0 ? (float)production / consumption : 1.0f;

        /* Update consumer satisfaction */
        for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
            if (ps->consumer_active[i] && ps->consumers[i].network_id == n) {
                ps->consumers[i].satisfied = (ratio >= 1.0f);
            }
        }
    }

    /* Consumers not connected to any network are not satisfied */
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (ps->consumer_active[i] && ps->consumers[i].network_id < 0) {
            ps->consumers[i].satisfied = false;
        }
    }
}

static void ensure_updated(Agentite_PowerSystem *ps) {
    if (ps->needs_recalc) {
        rebuild_networks(ps);
        update_consumer_satisfaction(ps);
    }
}

/*============================================================================
 * Power System Creation
 *============================================================================*/

Agentite_PowerSystem *agentite_power_create(int grid_width, int grid_height) {
    Agentite_PowerSystem *ps = AGENTITE_ALLOC(Agentite_PowerSystem);
    if (!ps) {
        agentite_set_error("Failed to allocate power system");
        return NULL;
    }

    memset(ps, 0, sizeof(Agentite_PowerSystem));
    ps->grid_width = grid_width > 0 ? grid_width : 100;
    ps->grid_height = grid_height > 0 ? grid_height : 100;
    ps->connection_multiplier = AGENTITE_POWER_CONNECTION_MULT;
    ps->brownout_threshold = AGENTITE_POWER_BROWNOUT_THRESHOLD;

    uf_init(ps);

    return ps;
}

void agentite_power_destroy(Agentite_PowerSystem *ps) {
    if (ps) {
        free(ps);
    }
}

void agentite_power_reset(Agentite_PowerSystem *ps) {
    AGENTITE_VALIDATE_PTR(ps);

    memset(ps->pole_active, 0, sizeof(ps->pole_active));
    memset(ps->producer_active, 0, sizeof(ps->producer_active));
    memset(ps->consumer_active, 0, sizeof(ps->consumer_active));
    ps->pole_count = 0;
    ps->producer_count = 0;
    ps->consumer_count = 0;
    ps->network_count = 0;
    ps->needs_recalc = false;

    uf_init(ps);
}

/*============================================================================
 * Pole Management
 *============================================================================*/

static int add_pole_internal(Agentite_PowerSystem *ps, int x, int y, int radius, bool is_substation) {
    AGENTITE_VALIDATE_PTR_RET(ps, AGENTITE_POWER_INVALID_ID);

    /* Find free slot */
    int id = -1;
    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        if (!ps->pole_active[i]) {
            id = i;
            break;
        }
    }

    if (id < 0) {
        agentite_set_error("Power: Maximum poles reached (%d/%d)", ps->pole_count, AGENTITE_POWER_MAX_POLES);
        return AGENTITE_POWER_INVALID_ID;
    }

    Agentite_PowerPole *pole = &ps->poles[id];
    pole->id = id;
    pole->x = x;
    pole->y = y;
    pole->radius = radius > 0 ? radius : 5;
    pole->network_id = -1;
    pole->is_substation = is_substation;

    ps->pole_active[id] = true;
    ps->pole_count++;
    ps->needs_recalc = true;

    return id;
}

int agentite_power_add_pole(Agentite_PowerSystem *ps, int x, int y, int radius) {
    return add_pole_internal(ps, x, y, radius, false);
}

int agentite_power_add_substation(Agentite_PowerSystem *ps, int x, int y, int radius) {
    return add_pole_internal(ps, x, y, radius, true);
}

bool agentite_power_remove_pole(Agentite_PowerSystem *ps, int pole_id) {
    AGENTITE_VALIDATE_PTR_RET(ps, false);

    if (pole_id < 0 || pole_id >= AGENTITE_POWER_MAX_POLES || !ps->pole_active[pole_id]) {
        return false;
    }

    ps->pole_active[pole_id] = false;
    ps->pole_count--;
    ps->needs_recalc = true;

    return true;
}

const Agentite_PowerPole *agentite_power_get_pole(const Agentite_PowerSystem *ps, int pole_id) {
    AGENTITE_VALIDATE_PTR_RET(ps, NULL);

    if (pole_id < 0 || pole_id >= AGENTITE_POWER_MAX_POLES || !ps->pole_active[pole_id]) {
        return NULL;
    }

    return &ps->poles[pole_id];
}

int agentite_power_get_pole_at(const Agentite_PowerSystem *ps, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(ps, AGENTITE_POWER_INVALID_ID);

    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        if (ps->pole_active[i] && ps->poles[i].x == x && ps->poles[i].y == y) {
            return i;
        }
    }

    return AGENTITE_POWER_INVALID_ID;
}

int agentite_power_get_network_poles(
    const Agentite_PowerSystem *ps,
    int network_id,
    int *out_ids,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(ps, 0);
    AGENTITE_VALIDATE_PTR_RET(out_ids, 0);

    /* Need to cast away const for ensure_updated - this is safe as we only read after */
    Agentite_PowerSystem *mutable_ps = (Agentite_PowerSystem *)ps;
    ensure_updated(mutable_ps);

    int count = 0;
    for (int i = 0; i < AGENTITE_POWER_MAX_POLES && count < max_count; i++) {
        if (ps->pole_active[i] && ps->poles[i].network_id == network_id) {
            out_ids[count++] = i;
        }
    }

    return count;
}

/*============================================================================
 * Producer Management
 *============================================================================*/

int agentite_power_add_producer(Agentite_PowerSystem *ps, int x, int y, int production) {
    AGENTITE_VALIDATE_PTR_RET(ps, AGENTITE_POWER_INVALID_ID);

    /* Find free slot */
    int id = -1;
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (!ps->producer_active[i]) {
            id = i;
            break;
        }
    }

    if (id < 0) {
        agentite_set_error("Power: Maximum producers reached (%d/%d)", ps->producer_count, AGENTITE_POWER_MAX_NODES);
        return AGENTITE_POWER_INVALID_ID;
    }

    Agentite_PowerProducer *prod = &ps->producers[id];
    prod->id = id;
    prod->x = x;
    prod->y = y;
    prod->production = production > 0 ? production : 0;
    prod->network_id = -1;
    prod->entity_id = -1;
    prod->active = true;

    ps->producer_active[id] = true;
    ps->producer_count++;
    ps->needs_recalc = true;

    return id;
}

bool agentite_power_remove_producer(Agentite_PowerSystem *ps, int producer_id) {
    AGENTITE_VALIDATE_PTR_RET(ps, false);

    if (producer_id < 0 || producer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->producer_active[producer_id]) {
        return false;
    }

    ps->producer_active[producer_id] = false;
    ps->producer_count--;
    ps->needs_recalc = true;

    return true;
}

void agentite_power_set_producer_active(Agentite_PowerSystem *ps, int producer_id, bool active) {
    AGENTITE_VALIDATE_PTR(ps);

    if (producer_id < 0 || producer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->producer_active[producer_id]) {
        return;
    }

    ps->producers[producer_id].active = active;
    ps->needs_recalc = true;
}

void agentite_power_set_production(Agentite_PowerSystem *ps, int producer_id, int production) {
    AGENTITE_VALIDATE_PTR(ps);

    if (producer_id < 0 || producer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->producer_active[producer_id]) {
        return;
    }

    ps->producers[producer_id].production = production > 0 ? production : 0;
    ps->needs_recalc = true;
}

const Agentite_PowerProducer *agentite_power_get_producer(
    const Agentite_PowerSystem *ps,
    int producer_id)
{
    AGENTITE_VALIDATE_PTR_RET(ps, NULL);

    if (producer_id < 0 || producer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->producer_active[producer_id]) {
        return NULL;
    }

    return &ps->producers[producer_id];
}

/*============================================================================
 * Consumer Management
 *============================================================================*/

int agentite_power_add_consumer(Agentite_PowerSystem *ps, int x, int y, int consumption) {
    AGENTITE_VALIDATE_PTR_RET(ps, AGENTITE_POWER_INVALID_ID);

    /* Find free slot */
    int id = -1;
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (!ps->consumer_active[i]) {
            id = i;
            break;
        }
    }

    if (id < 0) {
        agentite_set_error("Power: Maximum consumers reached (%d/%d)", ps->consumer_count, AGENTITE_POWER_MAX_NODES);
        return AGENTITE_POWER_INVALID_ID;
    }

    Agentite_PowerConsumer *cons = &ps->consumers[id];
    cons->id = id;
    cons->x = x;
    cons->y = y;
    cons->consumption = consumption > 0 ? consumption : 0;
    cons->network_id = -1;
    cons->entity_id = -1;
    cons->active = true;
    cons->satisfied = false;

    ps->consumer_active[id] = true;
    ps->consumer_count++;
    ps->needs_recalc = true;

    return id;
}

bool agentite_power_remove_consumer(Agentite_PowerSystem *ps, int consumer_id) {
    AGENTITE_VALIDATE_PTR_RET(ps, false);

    if (consumer_id < 0 || consumer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->consumer_active[consumer_id]) {
        return false;
    }

    ps->consumer_active[consumer_id] = false;
    ps->consumer_count--;
    ps->needs_recalc = true;

    return true;
}

void agentite_power_set_consumer_active(Agentite_PowerSystem *ps, int consumer_id, bool active) {
    AGENTITE_VALIDATE_PTR(ps);

    if (consumer_id < 0 || consumer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->consumer_active[consumer_id]) {
        return;
    }

    ps->consumers[consumer_id].active = active;
    ps->needs_recalc = true;
}

void agentite_power_set_consumption(Agentite_PowerSystem *ps, int consumer_id, int consumption) {
    AGENTITE_VALIDATE_PTR(ps);

    if (consumer_id < 0 || consumer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->consumer_active[consumer_id]) {
        return;
    }

    ps->consumers[consumer_id].consumption = consumption > 0 ? consumption : 0;
    ps->needs_recalc = true;
}

const Agentite_PowerConsumer *agentite_power_get_consumer(
    const Agentite_PowerSystem *ps,
    int consumer_id)
{
    AGENTITE_VALIDATE_PTR_RET(ps, NULL);

    if (consumer_id < 0 || consumer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->consumer_active[consumer_id]) {
        return NULL;
    }

    return &ps->consumers[consumer_id];
}

bool agentite_power_is_consumer_powered(const Agentite_PowerSystem *ps, int consumer_id) {
    AGENTITE_VALIDATE_PTR_RET(ps, false);

    Agentite_PowerSystem *mutable_ps = (Agentite_PowerSystem *)ps;
    ensure_updated(mutable_ps);

    if (consumer_id < 0 || consumer_id >= AGENTITE_POWER_MAX_NODES ||
        !ps->consumer_active[consumer_id]) {
        return false;
    }

    return ps->consumers[consumer_id].satisfied;
}

/*============================================================================
 * Network Queries
 *============================================================================*/

int agentite_power_get_network_at(const Agentite_PowerSystem *ps, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(ps, AGENTITE_POWER_INVALID_ID);

    Agentite_PowerSystem *mutable_ps = (Agentite_PowerSystem *)ps;
    ensure_updated(mutable_ps);

    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        if (!ps->pole_active[i]) continue;

        const Agentite_PowerPole *pole = &ps->poles[i];
        if (point_in_range(x, y, pole->x, pole->y, pole->radius)) {
            return pole->network_id;
        }
    }

    return AGENTITE_POWER_INVALID_ID;
}

Agentite_PowerStatus agentite_power_get_status_at(const Agentite_PowerSystem *ps, int x, int y) {
    AGENTITE_VALIDATE_PTR_RET(ps, AGENTITE_POWER_UNPOWERED);

    int network_id = agentite_power_get_network_at(ps, x, y);
    if (network_id < 0) {
        return AGENTITE_POWER_UNPOWERED;
    }

    Agentite_NetworkStats stats;
    if (!agentite_power_get_network_stats(ps, network_id, &stats)) {
        return AGENTITE_POWER_UNPOWERED;
    }

    return stats.status;
}

bool agentite_power_is_covered(const Agentite_PowerSystem *ps, int x, int y) {
    return agentite_power_get_network_at(ps, x, y) >= 0;
}

bool agentite_power_get_network_stats(
    const Agentite_PowerSystem *ps,
    int network_id,
    Agentite_NetworkStats *out_stats)
{
    AGENTITE_VALIDATE_PTR_RET(ps, false);
    AGENTITE_VALIDATE_PTR_RET(out_stats, false);

    Agentite_PowerSystem *mutable_ps = (Agentite_PowerSystem *)ps;
    ensure_updated(mutable_ps);

    if (network_id < 0 || network_id >= ps->network_count) {
        return false;
    }

    memset(out_stats, 0, sizeof(Agentite_NetworkStats));
    out_stats->network_id = network_id;

    /* Count poles */
    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        if (ps->pole_active[i] && ps->poles[i].network_id == network_id) {
            out_stats->pole_count++;
        }
    }

    /* Sum production */
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (ps->producer_active[i] && ps->producers[i].network_id == network_id) {
            out_stats->producer_count++;
            if (ps->producers[i].active) {
                out_stats->total_production += ps->producers[i].production;
            }
        }
    }

    /* Sum consumption */
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (ps->consumer_active[i] && ps->consumers[i].network_id == network_id) {
            out_stats->consumer_count++;
            if (ps->consumers[i].active) {
                out_stats->total_consumption += ps->consumers[i].consumption;
            }
        }
    }

    /* Calculate satisfaction ratio and status */
    if (out_stats->total_consumption > 0) {
        out_stats->satisfaction_ratio =
            (float)out_stats->total_production / out_stats->total_consumption;
    } else {
        out_stats->satisfaction_ratio = out_stats->total_production > 0 ? 1.0f : 0.0f;
    }

    if (out_stats->total_production <= 0) {
        out_stats->status = AGENTITE_POWER_UNPOWERED;
    } else if (out_stats->satisfaction_ratio < mutable_ps->brownout_threshold) {
        out_stats->status = AGENTITE_POWER_BROWNOUT;
    } else if (out_stats->satisfaction_ratio >= 1.0f) {
        out_stats->status = AGENTITE_POWER_POWERED;
    } else {
        out_stats->status = AGENTITE_POWER_BROWNOUT;
    }

    return true;
}

int agentite_power_get_networks(const Agentite_PowerSystem *ps, int *out_ids, int max_count) {
    AGENTITE_VALIDATE_PTR_RET(ps, 0);
    AGENTITE_VALIDATE_PTR_RET(out_ids, 0);

    Agentite_PowerSystem *mutable_ps = (Agentite_PowerSystem *)ps;
    ensure_updated(mutable_ps);

    int count = ps->network_count < max_count ? ps->network_count : max_count;
    for (int i = 0; i < count; i++) {
        out_ids[i] = i;
    }

    return count;
}

int agentite_power_get_total_production(const Agentite_PowerSystem *ps) {
    AGENTITE_VALIDATE_PTR_RET(ps, 0);

    int total = 0;
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (ps->producer_active[i] && ps->producers[i].active) {
            total += ps->producers[i].production;
        }
    }

    return total;
}

int agentite_power_get_total_consumption(const Agentite_PowerSystem *ps) {
    AGENTITE_VALIDATE_PTR_RET(ps, 0);

    int total = 0;
    for (int i = 0; i < AGENTITE_POWER_MAX_NODES; i++) {
        if (ps->consumer_active[i] && ps->consumers[i].active) {
            total += ps->consumers[i].consumption;
        }
    }

    return total;
}

/*============================================================================
 * Coverage Queries
 *============================================================================*/

int agentite_power_get_pole_coverage(
    const Agentite_PowerSystem *ps,
    int pole_id,
    int *out_cells,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(ps, 0);
    AGENTITE_VALIDATE_PTR_RET(out_cells, 0);

    if (pole_id < 0 || pole_id >= AGENTITE_POWER_MAX_POLES || !ps->pole_active[pole_id]) {
        return 0;
    }

    const Agentite_PowerPole *pole = &ps->poles[pole_id];
    int count = 0;

    for (int dy = -pole->radius; dy <= pole->radius && count < max_count; dy++) {
        for (int dx = -pole->radius; dx <= pole->radius && count < max_count; dx++) {
            int x = pole->x + dx;
            int y = pole->y + dy;

            if (x < 0 || x >= ps->grid_width || y < 0 || y >= ps->grid_height) {
                continue;
            }

            if (point_in_range(x, y, pole->x, pole->y, pole->radius)) {
                out_cells[count * 2] = x;
                out_cells[count * 2 + 1] = y;
                count++;
            }
        }
    }

    return count;
}

int agentite_power_get_network_coverage(
    const Agentite_PowerSystem *ps,
    int network_id,
    int *out_cells,
    int max_count)
{
    AGENTITE_VALIDATE_PTR_RET(ps, 0);
    AGENTITE_VALIDATE_PTR_RET(out_cells, 0);

    Agentite_PowerSystem *mutable_ps = (Agentite_PowerSystem *)ps;
    ensure_updated(mutable_ps);

    /* Use a simple array to track unique cells */
    int *unique_cells = (int *)malloc(max_count * 2 * sizeof(int));
    if (!unique_cells) return 0;

    int count = 0;

    for (int p = 0; p < AGENTITE_POWER_MAX_POLES; p++) {
        if (!ps->pole_active[p] || ps->poles[p].network_id != network_id) continue;

        const Agentite_PowerPole *pole = &ps->poles[p];

        for (int dy = -pole->radius; dy <= pole->radius; dy++) {
            for (int dx = -pole->radius; dx <= pole->radius; dx++) {
                int x = pole->x + dx;
                int y = pole->y + dy;

                if (x < 0 || x >= ps->grid_width || y < 0 || y >= ps->grid_height) {
                    continue;
                }

                if (!point_in_range(x, y, pole->x, pole->y, pole->radius)) {
                    continue;
                }

                /* Check if already in list */
                bool found = false;
                for (int i = 0; i < count; i++) {
                    if (unique_cells[i * 2] == x && unique_cells[i * 2 + 1] == y) {
                        found = true;
                        break;
                    }
                }

                if (!found && count < max_count) {
                    unique_cells[count * 2] = x;
                    unique_cells[count * 2 + 1] = y;
                    count++;
                }
            }
        }
    }

    memcpy(out_cells, unique_cells, count * 2 * sizeof(int));
    free(unique_cells);

    return count;
}

int agentite_power_find_nearest_pole(
    const Agentite_PowerSystem *ps,
    int x,
    int y,
    int *out_x,
    int *out_y)
{
    AGENTITE_VALIDATE_PTR_RET(ps, -1);

    Agentite_PowerSystem *mutable_ps = (Agentite_PowerSystem *)ps;
    ensure_updated(mutable_ps);

    int nearest_dist = -1;
    int nearest_x = 0, nearest_y = 0;

    for (int i = 0; i < AGENTITE_POWER_MAX_POLES; i++) {
        if (!ps->pole_active[i]) continue;

        /* Only consider poles in powered networks */
        Agentite_NetworkStats stats;
        if (!agentite_power_get_network_stats(ps, ps->poles[i].network_id, &stats)) {
            continue;
        }
        if (stats.status == AGENTITE_POWER_UNPOWERED) {
            continue;
        }

        const Agentite_PowerPole *pole = &ps->poles[i];
        int dist_sq = distance_squared(x, y, pole->x, pole->y);
        int dist = (int)sqrtf((float)dist_sq);

        if (nearest_dist < 0 || dist < nearest_dist) {
            nearest_dist = dist;
            nearest_x = pole->x;
            nearest_y = pole->y;
        }
    }

    if (nearest_dist >= 0) {
        if (out_x) *out_x = nearest_x;
        if (out_y) *out_y = nearest_y;
    }

    return nearest_dist;
}

/*============================================================================
 * Network Updates
 *============================================================================*/

void agentite_power_recalculate(Agentite_PowerSystem *ps) {
    AGENTITE_VALIDATE_PTR(ps);

    ps->needs_recalc = true;
    ensure_updated(ps);
}

void agentite_power_set_callback(
    Agentite_PowerSystem *ps,
    Agentite_PowerCallback callback,
    void *userdata)
{
    AGENTITE_VALIDATE_PTR(ps);
    ps->callback = callback;
    ps->callback_userdata = userdata;
}

/*============================================================================
 * Configuration
 *============================================================================*/

void agentite_power_set_connection_multiplier(Agentite_PowerSystem *ps, float multiplier) {
    AGENTITE_VALIDATE_PTR(ps);
    ps->connection_multiplier = multiplier > 0.0f ? multiplier : 1.0f;
    ps->needs_recalc = true;
}

void agentite_power_set_brownout_threshold(Agentite_PowerSystem *ps, float threshold) {
    AGENTITE_VALIDATE_PTR(ps);
    ps->brownout_threshold = threshold > 0.0f && threshold < 1.0f ? threshold : 0.5f;
}

/*============================================================================
 * Utility
 *============================================================================*/

const char *agentite_power_status_name(Agentite_PowerStatus status) {
    switch (status) {
        case AGENTITE_POWER_UNPOWERED: return "Unpowered";
        case AGENTITE_POWER_BROWNOUT:  return "Brownout";
        case AGENTITE_POWER_POWERED:   return "Powered";
        default:                       return "Unknown";
    }
}

const char *agentite_power_node_type_name(Agentite_PowerNodeType type) {
    switch (type) {
        case AGENTITE_POWER_POLE:       return "Pole";
        case AGENTITE_POWER_SUBSTATION: return "Substation";
        case AGENTITE_POWER_PRODUCER:   return "Producer";
        case AGENTITE_POWER_CONSUMER:   return "Consumer";
        default:                        return "Unknown";
    }
}
