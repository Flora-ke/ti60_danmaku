#include "game.h"
#include <string.h>

/* Unit circle in Q8, twenty equally spaced directions. All speed scaling is
 * integer arithmetic. The 40-direction table permits alternating half-steps. */
static const int16_t circle[40][2] = {
    {256,0},{253,40},{243,79},{228,116},{207,150},{181,181},
    {150,207},{116,228},{79,243},{40,253},{0,256},{-40,253},
    {-79,243},{-116,228},{-150,207},{-181,181},{-207,150},
    {-228,116},{-243,79},{-253,40},{-256,0},{-253,-40},
    {-243,-79},{-228,-116},{-207,-150},{-181,-181},
    {-150,-207},{-116,-228},{-79,-243},{-40,-253},{0,-256},
    {40,-253},{79,-243},{116,-228},{150,-207},{181,-181},
    {207,-150},{228,-116},{243,-79},{253,-40}
};

static uint32_t preset(uint32_t limit)
{
    return limit <= 128u ? 128u : (limit <= 256u ? 256u : 512u);
}

/* All play-experience settings live here; bullet/pacing/render settings do not. */
static const GamePlayConfig play_configs[2] = {
    {10u, 180u, 1u}, {3u, 120u, 0u}
};
static const GamePlayConfig demo_config = {3u, 120u, 1u};

const GamePlayConfig *game_play_config(const Game *g)
{
    if (g && g->mode == GMODE_DEMO) return &demo_config;
    return &play_configs[g && g->difficulty == GDIFFICULTY_STANDARD ? 1 : 0];
}

void game_init(Game *g, GameMode mode, uint32_t limit)
{
    game_init_config(g, mode, limit, GDIFFICULTY_BEGINNER);
}

void game_init_config(Game *g, GameMode mode, uint32_t limit,
                      GameDifficulty difficulty)
{
    if (!g) return;
    memset(g, 0, sizeof(*g));
    g->mode = mode == GMODE_DEMO ? GMODE_DEMO : GMODE_PLAY;
    g->difficulty = difficulty == GDIFFICULTY_STANDARD ?
        GDIFFICULTY_STANDARD : GDIFFICULTY_BEGINNER;
    g->state = g->mode == GMODE_DEMO ? GSTATE_RUNNING : GSTATE_TITLE;
    g->enemy_limit = preset(limit);
    g->player_x = 146 * GAME_Q;
    g->player_y = 254 * GAME_Q;
    g->boss_x = 146 * GAME_Q;
    g->boss_y = 48 * GAME_Q;
    g->hp = GAME_BOSS_HP;
    g->lives = game_play_config(g)->starting_lives;
}

static int32_t clamp(int32_t x, int32_t lo, int32_t hi)
{
    return x < lo ? lo : (x > hi ? hi : x);
}

static void add_enemy(Game *g, int32_t vx, int32_t vy, uint8_t kind)
{
    uint32_t i;
    if (g->active_enemy >= g->enemy_limit) {
        ++g->dropped;
        return;
    }
    for (i = 0; i < GAME_ENEMY_CAP; ++i) {
        Bullet *b = &g->enemy[i];
        if (b->active) continue;
        b->x = g->boss_x; b->y = g->boss_y;
        b->vx = vx; b->vy = vy;
        b->ttl = 360; b->kind = kind; b->active = 1;
        ++g->active_enemy;
        if (g->active_enemy > g->peak_enemy) g->peak_enemy = g->active_enemy;
        return;
    }
    ++g->dropped;
}

static void emit(Game *g)
{
    uint32_t i;
    int32_t sweep;
    /* Original Flandre-like combination: twenty radial stars and two
     * oscillating downward shots: normal presets fire every twelve ticks
     * (200 ms); the 512 preset doubles density with six ticks (100 ms).
     * Half-step rotation improves layer separation in this demo adaptation. */
    for (i = 0; i < 20; ++i) {
        uint32_t d = (2u * i + g->ring_phase) % 40u;
        add_enemy(g, circle[d][0] * 9 / 8,
                     circle[d][1] * 9 / 8, GBULLET_RING);
    }
    sweep = (int32_t)(g->ring_phase % 20u);
    if (sweep > 10) sweep = 20 - sweep;
    sweep = (sweep - 5) * 48;
    add_enemy(g, sweep - 80, 448, GBULLET_SWEEP);
    add_enemy(g, -sweep + 80, 448, GBULLET_SWEEP);
    g->ring_phase = (uint16_t)((g->ring_phase + 1u) % 40u);
}

static void shoot(Game *g)
{
    uint32_t i;
    for (i = 0; i < GAME_PLAYER_CAP; ++i) {
        Bullet *b = &g->player[i];
        if (b->active) continue;
        b->x = g->player_x; b->y = g->player_y - 6 * GAME_Q;
        b->vx = 0; b->vy = -5 * GAME_Q;
        b->ttl = 90; b->kind = GBULLET_PLAYER; b->active = 1;
        ++g->active_player;
        return;
    }
    ++g->dropped;
}

static int overlap(int32_t ax, int32_t ay, int32_t bx, int32_t by, int radius)
{
    int32_t dx = ax / GAME_Q - bx / GAME_Q;
    int32_t dy = ay / GAME_Q - by / GAME_Q;
    return dx * dx + dy * dy <= radius * radius;
}

static int expired(Bullet *b)
{
    b->x += b->vx; b->y += b->vy;
    if (b->ttl) --b->ttl;
    return !b->ttl || b->x < GAME_LEFT * GAME_Q ||
        b->x > GAME_RIGHT * GAME_Q || b->y < GAME_TOP * GAME_Q ||
        b->y > GAME_BOTTOM * GAME_Q;
}

static void move_bullets(Game *g)
{
    uint32_t i;
    for (i = 0; i < GAME_ENEMY_CAP; ++i) {
        Bullet *b = &g->enemy[i];
        if (!b->active) continue;
        if (expired(b)) {
            b->active = 0; --g->active_enemy;
        } else if (g->mode == GMODE_PLAY && !g->invulnerable &&
                   overlap(b->x, b->y, g->player_x, g->player_y, 4)) {
            b->active = 0; --g->active_enemy;
            if (g->lives) --g->lives;
            g->invulnerable = game_play_config(g)->hurt_invulnerable_ticks;
            if (!g->lives) g->state = GSTATE_LOST;
        }
    }
    for (i = 0; i < GAME_PLAYER_CAP; ++i) {
        Bullet *b = &g->player[i];
        if (!b->active) continue;
        if (expired(b)) {
            b->active = 0; --g->active_player;
        } else if (overlap(b->x, b->y, g->boss_x, g->boss_y, 14)) {
            b->active = 0; --g->active_player;
            g->score += 10;
            if (g->hp > 1 || g->mode == GMODE_PLAY) {
                if (g->hp) --g->hp;
            }
            if (!g->hp && g->state != GSTATE_LOST) g->state = GSTATE_WON;
        }
    }
}

static void move_actor(Game *g, uint32_t keys)
{
    uint32_t phase;
    int32_t horizontal, vertical;
    /* 240-tick triangular motion, +/- 40 px, exactly repeatable on every CPU. */
    phase = g->tick % 240u;
    horizontal = phase <= 120u ? (int32_t)phase : 240 - (int32_t)phase;
    g->boss_x = 106 * GAME_Q + horizontal * (80 * GAME_Q) / 120;
    if (g->mode == GMODE_DEMO) {
        phase = g->tick % 360u;
        horizontal = phase <= 180u ? (int32_t)phase : 360 - (int32_t)phase;
        g->player_x = 40 * GAME_Q + horizontal * (212 * GAME_Q) / 180;
        g->player_y = 254 * GAME_Q;
        return;
    }
    horizontal = ((keys & GKEY_RIGHT) != 0) - ((keys & GKEY_LEFT) != 0);
    vertical = ((keys & GKEY_DOWN) != 0) - ((keys & GKEY_UP) != 0);
    /* Diagonal movement uses 181/256 (~1/sqrt(2)) to preserve speed. */
    phase = horizontal && vertical ? 362u : 512u;
    g->player_x = clamp(g->player_x + horizontal * (int32_t)phase,
                        16 * GAME_Q, 276 * GAME_Q);
    g->player_y = clamp(g->player_y + vertical * (int32_t)phase,
                        16 * GAME_Q, 282 * GAME_Q);
}

void game_step(Game *g, uint32_t keys)
{
    uint32_t edge;
    if (!g) return;
    keys &= (1u << 10) - 1u;
    edge = keys & ~g->prev_keys;
    g->prev_keys = keys;
    if (edge & (GKEY_DEMO | GKEY_LEVEL | GKEY_START)) {
        uint32_t limit = g->enemy_limit;
        GameMode mode = g->mode;
        if (edge & GKEY_DEMO) mode = GMODE_DEMO;
        else if (edge & GKEY_LEVEL) limit = limit == 128 ? 256 : (limit == 256 ? 512 : 128);
        else mode = GMODE_PLAY;
        game_init_config(g, mode, limit, g->difficulty);
        g->prev_keys = keys; /* Held commands cannot repeatedly reset. */
        if (!(edge & GKEY_LEVEL) || (edge & GKEY_DEMO)) g->state = GSTATE_RUNNING;
        return;
    }
    if ((edge & GKEY_DIFFICULTY) &&
        (g->state == GSTATE_TITLE || g->state == GSTATE_WON ||
         g->state == GSTATE_LOST)) {
        GameDifficulty difficulty = g->difficulty == GDIFFICULTY_BEGINNER ?
            GDIFFICULTY_STANDARD : GDIFFICULTY_BEGINNER;
        game_init_config(g, GMODE_PLAY, g->enemy_limit, difficulty);
        g->prev_keys = keys;
        return;
    }
    if (edge & GKEY_PAUSE) {
        if (g->state == GSTATE_RUNNING) g->state = GSTATE_PAUSED;
        else if (g->state == GSTATE_PAUSED) g->state = GSTATE_RUNNING;
        return;
    }
    if (g->state != GSTATE_RUNNING) return;
    if (g->invulnerable) --g->invulnerable;
    if (g->fire_cooldown) --g->fire_cooldown;
    if (g->enemy_cooldown) --g->enemy_cooldown;
    move_actor(g, keys);
    if (!g->enemy_cooldown) {
        emit(g);
        g->enemy_cooldown = g->enemy_limit == 512u ? 6 : 12;
    }
    if (!g->fire_cooldown && ((keys & GKEY_FIRE) || game_play_config(g)->auto_fire)) {
        shoot(g);
        g->fire_cooldown = 6;
    }
    move_bullets(g);
    ++g->tick;
    if (g->mode == GMODE_DEMO && g->tick >= GAME_DEMO_TICKS) {
        g->state = GSTATE_WON;
    }
}

static uint32_t hash_word(uint32_t h, uint32_t word)
{
    unsigned i;
    for (i = 0; i < 4; ++i) { h = (h ^ (word & 255u)) * 16777619u; word >>= 8; }
    return h;
}

uint32_t game_hash(const Game *g)
{
    uint32_t h = 2166136261u, i;
    const GamePlayConfig *config;
    if (!g) return 0;
#define H(field) h = hash_word(h, (uint32_t)g->field)
    H(player_x); H(player_y); H(boss_x); H(boss_y); H(hp); H(lives);
    H(score); H(tick); H(active_enemy); H(active_player); H(dropped);
    H(peak_enemy); H(mode); H(state); H(enemy_limit); H(prev_keys);
    H(invulnerable); H(fire_cooldown); H(enemy_cooldown); H(ring_phase);
    H(difficulty);
#undef H
    config = game_play_config(g);
    h = hash_word(h, config->starting_lives);
    h = hash_word(h, config->hurt_invulnerable_ticks);
    h = hash_word(h, config->auto_fire);
    for (i = 0; i < GAME_ENEMY_CAP + GAME_PLAYER_CAP; ++i) {
        const Bullet *b = i < GAME_ENEMY_CAP ? &g->enemy[i] : &g->player[i - GAME_ENEMY_CAP];
        h = hash_word(h, (uint32_t)b->x); h = hash_word(h, (uint32_t)b->y);
        h = hash_word(h, (uint32_t)b->vx); h = hash_word(h, (uint32_t)b->vy);
        h = hash_word(h, b->ttl); h = hash_word(h, b->active); h = hash_word(h, b->kind);
    }
    return h;
}
