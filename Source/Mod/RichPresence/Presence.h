#include <discord_rpc.h>

extern int presence_enabled;
extern int client_base;

void init_rich_presence();
void switch_rich_presence();
void switch_btn();
void update_presence();

void trigger_player_count_validation();
void decrement_player_count();
void get_server_info(int triggered_by_packet, uint8_t game_mode_id);

typedef struct game_state {
    // offline
    int is_alive;
    int current_tool;
    int current_weapon;
    int current_team;
    int intel_holder_t1;
    int intel_holder_t2;

    // online - state data trigger
    char identifier[32];
    char server_name[32];
    char map_name[32];
    char game_mode[32];
    uint8_t game_mode_id;
    int max_players;
    int64_t playtime_start;
} game_state;

typedef struct presence_button {
    char label[32];
    char url[128];
} presence_button;
