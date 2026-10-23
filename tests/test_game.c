#include "game.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>

static void invariants(const Game *g)
{
    uint32_t i, enemies = 0, players = 0;
    assert(g->player_x >= 16 * GAME_Q && g->player_x <= 276 * GAME_Q);
    assert(g->player_y >= 16 * GAME_Q && g->player_y <= 282 * GAME_Q);
    assert(g->hp <= GAME_BOSS_HP && g->lives <= 3);
    for (i = 0; i < GAME_ENEMY_CAP + GAME_PLAYER_CAP; ++i) {
        const Bullet *b = i < GAME_ENEMY_CAP ? &g->enemy[i] : &g->player[i - GAME_ENEMY_CAP];
        if (!b->active) continue;
        if (i < GAME_ENEMY_CAP) ++enemies; else ++players;
        assert(b->ttl > 0);
        assert(b->x >= GAME_LEFT * GAME_Q && b->x <= GAME_RIGHT * GAME_Q);
        assert(b->y >= GAME_TOP * GAME_Q && b->y <= GAME_BOTTOM * GAME_Q);
    }
    assert(enemies == g->active_enemy && players == g->active_player);
    assert(enemies <= g->enemy_limit && players <= GAME_PLAYER_CAP);
    assert(g->peak_enemy <= g->enemy_limit);
}

static void test_demo(void)
{
    Game a, b;
    uint32_t i, medium_peak = 0, limits[] = {128, 256, 512};
    for (i = 0; i < 3; ++i) {
        uint32_t t;
        game_init(&a, GMODE_DEMO, limits[i]);
        game_init(&b, GMODE_DEMO, limits[i]);
        for (t = 0; t < GAME_DEMO_TICKS; ++t) {
            game_step(&a, 0); game_step(&b, 0);
            invariants(&a);
            if (!(t % 29)) assert(game_hash(&a) == game_hash(&b));
            if (t + 1 < GAME_DEMO_TICKS) assert(a.state == GSTATE_RUNNING);
        }
        assert(a.state == GSTATE_WON && a.tick == GAME_DEMO_TICKS);
        assert(a.lives == 3 && a.hp > 0 && a.score > 0);
        assert(a.peak_enemy > 20);
        if (limits[i] == 256) medium_peak = a.peak_enemy;
        if (limits[i] == 512) assert(a.peak_enemy > medium_peak);
        {
            uint32_t hash = game_hash(&a);
            game_step(&a, 0);
            assert(hash == game_hash(&a));
        }
        printf("demo limit=%u peak=%u dropped=%u hash=%08x\n",
               limits[i], a.peak_enemy, a.dropped, game_hash(&a));
    }
}

static void test_controls_and_reset(void)
{
    Game g;
    uint32_t i, tick;
    game_init(&g, GMODE_PLAY, 129);
    assert(g.enemy_limit == 256 && g.state == GSTATE_TITLE);
    game_step(&g, GKEY_RIGHT | GKEY_FIRE);
    assert(!g.tick && !g.active_player);
    game_step(&g, GKEY_START);
    assert(g.state == GSTATE_RUNNING && !g.tick);
    game_step(&g, GKEY_START); /* held START must not keep resetting */
    assert(g.tick == 1);
    for (i = 0; i < 300; ++i) game_step(&g, GKEY_LEFT | GKEY_UP);
    assert(g.player_x == 16 * GAME_Q && g.player_y == 16 * GAME_Q);
    game_step(&g, GKEY_PAUSE);
    tick = g.tick;
    for (i = 0; i < 20; ++i) game_step(&g, GKEY_PAUSE | GKEY_RIGHT);
    assert(g.state == GSTATE_PAUSED && g.tick == tick);
    game_step(&g, 0); game_step(&g, GKEY_PAUSE);
    assert(g.state == GSTATE_RUNNING && g.tick == tick);
    game_step(&g, 0); game_step(&g, GKEY_START);
    assert(g.hp == GAME_BOSS_HP && g.lives == 3 && !g.tick);
    assert(!g.active_enemy && !g.active_player && !g.dropped && !g.score);
    game_step(&g, GKEY_LEVEL);
    assert(g.enemy_limit == 512 && g.state == GSTATE_TITLE);
    game_step(&g, 0); game_step(&g, GKEY_LEVEL);
    assert(g.enemy_limit == 128);
    game_step(&g, GKEY_DEMO);
    assert(g.mode == GMODE_DEMO && g.state == GSTATE_RUNNING);
    game_step(&g, GKEY_START);
    assert(g.mode == GMODE_PLAY && g.state == GSTATE_RUNNING && !g.tick);
}

static void plant_hit(Game *g)
{
    uint32_t i;
    for (i = 0; i < 3; ++i) {
        Bullet *b = &g->enemy[i];
        b->x = g->player_x; b->y = g->player_y;
        b->vx = b->vy = 0; b->active = 1; b->ttl = 500;
    }
    g->active_enemy = 3;
    g->enemy_cooldown = 12;
}

static void test_collisions(void)
{
    Game g;
    uint32_t i;
    game_init(&g, GMODE_PLAY, 128); game_step(&g, GKEY_START);
    plant_hit(&g);
    game_step(&g, 0);
    assert(g.lives == 2 && g.invulnerable == 120);
    for (i = 0; i < 119; ++i) game_step(&g, 0);
    assert(g.lives == 2 && g.invulnerable == 1);
    game_step(&g, 0);
    assert(g.lives == 1 && g.invulnerable == 120);
    for (i = 0; i < 120; ++i) game_step(&g, 0);
    assert(g.lives == 0 && g.state == GSTATE_LOST);
    game_step(&g, GKEY_START);
    assert(g.state == GSTATE_RUNNING && g.lives == 3 && !g.active_enemy);
    g.hp = 1;
    g.player[0].x = 106 * GAME_Q; g.player[0].y = 53 * GAME_Q;
    g.player[0].vy = -5 * GAME_Q; g.player[0].ttl = 20;
    g.player[0].active = 1; g.active_player = 1;
    game_step(&g, 0);
    assert(g.hp == 0 && g.state == GSTATE_WON && g.score == 10);
}

static void test_pool_limit_and_wrap(void)
{
    Game g;
    uint32_t i;
    game_init(&g, GMODE_PLAY, 128); game_step(&g, GKEY_START);
    for (i = 0; i < g.enemy_limit; ++i) {
        g.enemy[i].x = g.enemy[i].y = 100 * GAME_Q;
        g.enemy[i].ttl = 1000; g.enemy[i].active = 1;
    }
    g.active_enemy = g.peak_enemy = g.enemy_limit;
    game_step(&g, 0);
    assert(g.active_enemy == 128 && g.dropped == 22);
    invariants(&g);
    g.tick = UINT32_MAX - 1u;
    game_step(&g, 0); game_step(&g, 0);
    assert(g.tick == 0 && g.state == GSTATE_RUNNING);
    invariants(&g);
}

static void test_long_random_inputs(void)
{
    Game g;
    uint32_t i, random = 0x12345678u;
    game_init(&g, GMODE_PLAY, 512); game_step(&g, GKEY_START);
    for (i = 0; i < 50000; ++i) {
        random = random * 1664525u + 1013904223u;
        if (g.state == GSTATE_LOST || g.state == GSTATE_WON) {
            game_step(&g, GKEY_START); game_step(&g, 0);
        }
        game_step(&g, (random >> 16) & 31u);
        invariants(&g);
    }
}

int main(void)
{
    test_demo(); test_controls_and_reset(); test_collisions();
    test_pool_limit_and_wrap(); test_long_random_inputs();
    game_init(0, GMODE_PLAY, 128); game_step(0, 0); assert(!game_hash(0));
    printf("game tests passed; sizeof(Game)=%zu bytes\n", sizeof(Game));
    return 0;
}
