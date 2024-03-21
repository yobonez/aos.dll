//#include <time.h>
#include <stdio.h>
#include <string.h>
#include <json.h>
#include <windows.h>
#include <winhttp.h>
#include <processenv.h>

#include <Presence.h>

#define MAX_RESPONSE 65535

game_state state;
game_state old_state;

const char* app_id = "699358451494682714";
int player_id = 0;
int validator_enabled = 0;

int player_count = 1;
int old_player_count = 1;

const char* tool_descriptions[4] = {
    "Digging",
    "Building",
    "",
    "Throwing nades"
};
const char* weapon_descriptions[3]= {
    "Rifle",
    "SMG",
    "Shotgun"
};
const char* tool_images[4] = {
    "largeimagekey_spade",
    "largeimagekey_block",
    "",
    "largeimagekey_grenade"
};
const char* weapon_images[3] = {
    "largeimagekey_rifle",
    "largeimagekey_smg",
    "largeimagekey_shotgun"
};

// handlers that inform about something may be good for logger module perhaps?
// void handle_discord_ready(const DiscordUser* user) { return; }
// void handle_discord_disconnected(int error_code, const char* message) { return; }
// void handle_discord_error(int error_code, const char* message) { return; }
// void handle_discord_join(const char* join_secret) { return; }
// void handle_discord_spectate(const char* spectate_secret) { return; }
// void handle_discord_join_request(const DiscordUser* request) { return; }

void discord_init() {
    // DiscordEventHandlers handlers;
    // handlers.ready = handle_discord_ready;
    // handlers.disconnected = handle_discord_disconnected;
    // handlers.errored = handle_discord_error;
    // handlers.joinGame = handle_discord_join;
    // handlers.spectateGame = handle_discord_spectate;
    // handlers.joinRequest = handle_discord_join_request;

    Discord_Initialize(app_id, NULL, 1, NULL);
}

void discord_shutdown() {
    Discord_Shutdown();
}

void validate_player_count() {
    if (validator_enabled) {
        int ply_count = 0;
        char* player_connected_addr = (char*)(client_base + 0x7ce94);
        
        for(int i = 0; i < 32; i++) {
            if (*(int*)(player_connected_addr + (i * 0x3a8)) == 1) ply_count++;
        }

        player_count = ply_count;
    }
    else return;
}
void decrement_player_count() {
    player_count--;
}

void get_current_game_state() {
    state.current_tool = *((int*)(client_base + 0x13cf808));
    state.current_weapon = *((int*)((client_base + 0x7ce5c) + (player_id * 0x3a8)));
    state.current_team = *((int*)((client_base + 0x7ce58) + (player_id * 0x3a8)));
    state.intel_holder_t1 = *((int*)(client_base + 0x13cf958));
    state.intel_holder_t2 = *((int*)(client_base + 0x13cf924));
}

void get_server_info() {
    // request buildandshoot for serverlist (https://learn.microsoft.com/en-us/windows/win32/api/winhttp/nf-winhttp-winhttpquerydataavailable#examples)
    DWORD bytes_available = 0;
    DWORD bytes_read = 0;
    LPSTR http_data_buffer;
    int is_successfull = FALSE;
    HINTERNET session = NULL, connection = NULL, request = NULL;

    LPSTR full_response = (LPSTR)malloc(sizeof(char) * MAX_RESPONSE);
    int index_to_copy = 0;

    session = WinHttpOpen( L"Mozilla/5.0 (X11; Linux x86_64; rv:10.0) Gecko/20100101 Firefox/10.0",  
                            WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                            WINHTTP_NO_PROXY_NAME, 
                            WINHTTP_NO_PROXY_BYPASS, 0 );

    if(session)
        connection = WinHttpConnect( session, L"services.buildandshoot.com",
                                INTERNET_DEFAULT_HTTPS_PORT, 0 );
        else
            printf("no session\n");

    if(connection)
        request = WinHttpOpenRequest( connection, L"GET", L"/serverlist.json",
                                    NULL, WINHTTP_NO_REFERER, 
                                    WINHTTP_DEFAULT_ACCEPT_TYPES, 
                                    WINHTTP_FLAG_SECURE );
        else
            printf("no connection (variable)\n");

    if(request)
        is_successfull = WinHttpSendRequest( request,
                                    WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                    WINHTTP_NO_REQUEST_DATA, 0, 
                                    0, 0 );
        else
            printf("no request\n");


    if(is_successfull)
        is_successfull = WinHttpReceiveResponse( request, NULL );
        else
            printf("no results\n");

    if(is_successfull)
    {
        do 
        {
            bytes_available = 0;
            if( !WinHttpQueryDataAvailable( request, &bytes_available ) )
                printf( "Error %lu in WinHttpQueryDataAvailable.\n",
                        GetLastError( ) );

            http_data_buffer = malloc(bytes_available+1);
            if( !http_data_buffer )
            {
                printf( "Out of memory\n" );
                bytes_available=0;
            }
            else
            {
                ZeroMemory( http_data_buffer, bytes_available+1 );

                if( !WinHttpReadData( request, (LPVOID)http_data_buffer, 
                                    bytes_available, &bytes_read ) )
                    printf( "Error %lu in WinHttpReadData.\n", GetLastError( ) );
                else {
                    for(unsigned int i = 0; i < bytes_read; i++) {
                        full_response[i + index_to_copy] = http_data_buffer[i];
                    }
                }
                index_to_copy += bytes_read;
                free(http_data_buffer);
            }
        } while( bytes_available > 0 );
    }


    if(!is_successfull)
        printf( "Error %ld has occurred.\n", GetLastError( ) );

    if(request) WinHttpCloseHandle(request);
    if(connection) WinHttpCloseHandle(connection);
    if(session) WinHttpCloseHandle(session);

    // parse json, get objects
    int server_idx = 0;
    int serverlist_len = strlen((const char*)full_response) + 1;
    json_tokener* tokener = json_tokener_new();
    json_object* serverlist = json_tokener_parse_ex(tokener, (const char*)full_response, serverlist_len);
    json_tokener_free(tokener);

    json_object* server_instance;
    LPSTR command_line = GetCommandLineA();
    char* identifier = strstr(command_line, "aos://");
    identifier[strlen(identifier) - 1] = '\0'; // remove useless quote

    while(1)
    {
        server_instance = json_object_array_get_idx(serverlist, server_idx);
        if (server_instance == NULL) { break; }
        json_object* server_identifier;
        json_object_object_get_ex(server_instance, "identifier", &server_identifier);

        const char* cmp_identifier = json_object_get_string(server_identifier);
        if (strcmp(identifier, cmp_identifier) == 0) {
            json_object *server_name, *map_name, *max_players;
            json_object_object_get_ex(server_instance, "name", &server_name);
            json_object_object_get_ex(server_instance, "map", &map_name);
            json_object_object_get_ex(server_instance, "players_max", &max_players);

            // strcpy(state.server_name, json_object_get_string(server_name));
            // strcpy(state.map_name, json_object_get_string(map_name));
            strcpy(state.server_name, "asdjfhdjlfld");
            strcpy(state.map_name, "bruhmomento");
            state.max_players = json_object_get_int(max_players);
            break;
        }

        server_idx++;
    } 

    // add into state object
    // profit
}

void update_presence(){
    if (presence_enabled)
    {
        player_id = *((int*)(client_base+0x13b1cf0));
        // int64_t playtime_start = time(0); // map change
        get_current_game_state();

        if ((state.current_team == old_state.current_team) &&
            (state.current_tool == old_state.current_tool) &&
            (state.current_weapon == old_state.current_weapon) &&
            (state.intel_holder_t1 == old_state.intel_holder_t1) &&
            (state.intel_holder_t2 == old_state.intel_holder_t2) && 
            (player_count == old_player_count)) 
        { return; }

        old_state = state;
        old_player_count = player_count;

        DiscordRichPresence presence;
        memset(&presence, 0, sizeof(DiscordRichPresence));
        // presence.startTimestamp = playtime_start;

        char s_name_buf[128], m_name_buf[128];
        sprintf(s_name_buf, "%s", state.server_name);
        sprintf(m_name_buf, "%s", state.map_name);
        presence.details = s_name_buf;
        presence.state = m_name_buf;

        if (player_id == -1) {
            presence.largeImageKey = "largeimagekey_loading";
            presence.largeImageText = "Loading map...";
        }
        else {
            if (state.current_team == -2) {
                presence.largeImageKey = "largeimagekey_teamselection";
                presence.largeImageText = "Choosing team";
            }
            else if (state.current_team == -1) {
                validator_enabled = 1;
                presence.largeImageKey = "largeimagekey_spectating";
                presence.largeImageText = "Spectating";
            }
            else {
                validator_enabled = 1;
                if (state.current_tool != 2) {
                    presence.largeImageKey = tool_images[state.current_tool];
                    presence.largeImageText = tool_descriptions[state.current_tool];
                }
                else {
                    presence.largeImageKey = weapon_images[state.current_weapon];
                    presence.largeImageText = weapon_descriptions[state.current_weapon];
                }
                
                if (state.intel_holder_t1 == player_id || state.intel_holder_t2 == player_id) {
                    presence.smallImageKey = "smallimagekey_intel";
                    presence.smallImageText = "Holds enemy intel!";
                }
                else {
                    presence.smallImageKey = "ace_of_spades";
                    char ply_count_buf[128];
                    sprintf(ply_count_buf, "Players: %d/%d", player_count, state.max_players);
                    presence.smallImageText = ply_count_buf;
                }
            }
        }


        Discord_UpdatePresence(&presence);
    }
    else {
        Discord_ClearPresence();
    }
}

void init_rich_presence() {
    discord_init();
    get_server_info();
    update_presence();

    //create_richpresence_menu();
}
