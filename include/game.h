#ifndef TI60_GAME_H
#define TI60_GAME_H

#include <stdint.h>

#define GAME_WIDTH 400
#define GAME_HEIGHT 300
#define GAME_Q 256
#define GAME_HZ 60u
#define GAME_DEMO_TICKS (60u * GAME_HZ)
#define GAME_ENEMY_CAP 512u
#define GAME_PLAYER_CAP 64u
#define GAME_BOSS_HP 360u
#define GAME_LEFT 12
#define GAME_RIGHT 280
#define GAME_TOP 12
#define GAME_BOTTOM 286

enum {
    GKEY_LEFT = 1u << 0, GKEY_RIGHT = 1u << 1,
    GKEY_UP = 1u << 2, GKEY_DOWN = 1u << 3,
    GKEY_FIRE = 1u << 4, GKEY_START = 1u << 5,
    GKEY_PAUSE = 1u << 6, GKEY_DEMO = 1u << 7,
    GKEY_LEVEL = 1u << 8, GKEY_DIFFICULTY = 1u << 9
};
typedef enum { GMODE_PLAY = 0, GMODE_DEMO = 1 } GameMode;
typedef enum {
    GDIFFICULTY_BEGINNER = 0, GDIFFICULTY_STANDARD = 1
} GameDifficulty;
typedef struct {
    uint32_t starting_lives;
    uint16_t hurt_invulnerable_ticks;
    uint8_t auto_fire;
} GamePlayConfig;
typedef enum {
    GSTATE_TITLE = 0, GSTATE_RUNNING, GSTATE_PAUSED, GSTATE_WON, GSTATE_LOST
} GameState;
enum { GBULLET_RING = 0, GBULLET_SWEEP = 1, GBULLET_PLAYER = 2 };

/* Positions and velocities are Q8: 256 means one pixel or one pixel/tick.
 * A bullet shares its sprite with other bullets; it never owns image memory. */
typedef struct {
    int32_t x, y, vx, vy;
    uint16_t ttl;
    uint8_t active, kind;
} Bullet;

typedef struct {
    int32_t player_x, player_y, boss_x, boss_y;
    uint32_t hp, lives, score, tick;
    uint32_t active_enemy, active_player, dropped, peak_enemy;
    GameMode mode;
    GameState state;
    GameDifficulty difficulty;
    uint32_t enemy_limit;
    uint32_t prev_keys;
    uint16_t invulnerable, fire_cooldown, enemy_cooldown;
    uint16_t ring_phase;
    Bullet enemy[GAME_ENEMY_CAP];
    Bullet player[GAME_PLAYER_CAP];
} Game;

/* No allocation, floating point, SDL, hardware access or runtime trig.
 * limit is normalized to the next preset (128/256/512). These are intensity
 * presets: 128/256 fire every 12 ticks; 512 fires every 6 ticks. Each volley
 * contains twenty radial shots and two sweeping shots. Capacity is a hard cap.
 * PLAY starts at TITLE; DEMO starts immediately and ends after 3600 ticks.
 * hp is Boss HP. Demo is invulnerable; Boss HP stays >= 1 until time expires.
 * game_init selects BEGINNER (10 lives, 180 hurt ticks, auto-fire).
 * STANDARD has 3 lives, 120 hurt ticks and requires held FIRE.
 * Demo independently uses 3 lives, auto-fire and complete invulnerability;
 * selected difficulty is preserved for a later START into manual play.
 */
void game_init(Game *game, GameMode mode, uint32_t limit);
void game_init_config(Game *game, GameMode mode, uint32_t limit,
                      GameDifficulty difficulty);

/* Returns immutable static configuration; NULL selects the beginner profile. */
const GamePlayConfig *game_play_config(const Game *game);

/* Call exactly once per 1/60 s of simulated time. Movement/FIRE are held keys.
 * START, PAUSE, DEMO, LEVEL and DIFFICULTY use rising edges internally.
 * START enters/restarts PLAY in RUNNING, including from demo, pause or end.
 * DEMO restarts in demo mode. LEVEL cycles 128/256/512 and resets all state
 * (PLAY returns to TITLE, DEMO runs). Both keep the selected difficulty.
 * DIFFICULTY toggles BEGINNER/STANDARD only in TITLE/WON/LOST and clears the
 * previous run into PLAY/TITLE; ignored while running or paused.
 * PAUSE toggles RUNNING/PAUSED. Reset priority is DEMO > LEVEL > START >
 * DIFFICULTY > PAUSE. Restart clears bullets, score, tick and protection.
 * Paused/title/end states do not advance tick, bullets or cooldowns.
 * Pixel-circle hit tests use truncated pixel centers: player radius 2,
 * enemy bullet radius 2, player shot radius 2, Boss radius 12.
 * Inputs beyond the bits above are ignored. NULL is accepted as a no-op.
 */
void game_step(Game *game, uint32_t keys);

/* Stable FNV-1a over explicitly serialized scalar fields (no struct padding).
 * Includes difficulty/configuration: hashes differ from pre-profile versions
 * even where demo simulation and bullet trajectories remain unchanged. */
uint32_t game_hash(const Game *game);

#endif
