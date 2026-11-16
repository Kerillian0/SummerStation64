/**
 * @file ra_web.c
 * @brief The RetroAchievements web API, for reading only (see ra_web.h).
 */

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "ra_web.h"
#include "rcheevos/include/rc_api_user.h"

static char user[64];
static char token[128];
static char error_text[512];

const char *ra_web_error (void) {
    return error_text;
}

const char *ra_web_user (void) {
    return user;
}

/* Send rcheevos's request; the answer's body stays valid until free(). */
static bool ask_server (rc_api_request_t *request, rc_api_server_response_t *answer, char **body) {
    size_t length = 0;
    int status = 0;
    if (!http_request(request->url, request->post_data, request->content_type, body, &length, &status)) {
        snprintf(error_text, sizeof(error_text), "%s", http_error());
        return false;
    }
    answer->body = *body;
    answer->body_length = length;
    answer->http_status_code = status;
    return true;
}

/* The token file sits beside the .exe. */
static void login_file (char *out, size_t size) {
    char exe[MAX_PATH];
    DWORD n = GetModuleFileNameA(NULL, exe, sizeof(exe));
    char *slash = (n > 0) ? strrchr(exe, '\\') : NULL;
    if (slash) {
        slash[1] = '\0';
    } else {
        exe[0] = '\0';
    }
    snprintf(out, size, "%sra_bridge_login.txt", exe);
}

static void read_line (const char *prompt, char *out, size_t size, bool hidden) {
    printf("%s", prompt);
    fflush(stdout);
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(input, &mode);
    if (hidden) {
        SetConsoleMode(input, mode & ~ENABLE_ECHO_INPUT);
    }
    if (!fgets(out, (int) size, stdin)) {
        out[0] = '\0';
    }
    if (hidden) {
        SetConsoleMode(input, mode);
        printf("\n");
    }
    out[strcspn(out, "\r\n")] = '\0';
}

static bool login_with (const char *name, const char *password, const char *saved_token) {
    rc_api_login_request_t params = { .username = name, .password = password, .api_token = saved_token };
    rc_api_request_t request;
    if (rc_api_init_login_request(&request, &params) != RC_OK) {
        snprintf(error_text, sizeof(error_text), "could not build the login request");
        return false;
    }
    rc_api_server_response_t answer;
    char *body = NULL;
    bool sent = ask_server(&request, &answer, &body);
    rc_api_destroy_request(&request);
    if (!sent) {
        return false;
    }
    rc_api_login_response_t response;
    int result = rc_api_process_login_server_response(&response, &answer);
    bool ok = (result == RC_OK) && response.response.succeeded && response.api_token;
    if (ok) {
        snprintf(user, sizeof(user), "%s", response.username ? response.username : name);
        snprintf(token, sizeof(token), "%s", response.api_token);
    } else {
        snprintf(error_text, sizeof(error_text), "login refused: %s",
                 response.response.error_message ? response.response.error_message : "no reason given");
    }
    rc_api_destroy_login_response(&response);
    free(body);
    return ok;
}

bool ra_web_login (void) {
    char path[MAX_PATH + 32];
    login_file(path, sizeof(path));

    /* A token from before. */
    FILE *f = fopen(path, "r");
    if (f) {
        char name[64] = "", saved[128] = "";
        if (fscanf(f, "%63s %127s", name, saved) == 2 && login_with(name, NULL, saved)) {
            fclose(f);
            return true;
        }
        fclose(f);
        printf("The saved login no longer works (%s); log in again.\n", error_text);
    }

    /* Ask once; keep only the token. */
    char name[64], password[128];
    printf("Log in to RetroAchievements (only to download achievement definitions: nothing is sent to your account).\n");
    read_line("Username: ", name, sizeof(name), false);
    read_line("Password: ", password, sizeof(password), true);
    bool ok = login_with(name, password, NULL);
    SecureZeroMemory(password, sizeof(password));
    if (!ok) {
        return false;
    }
    f = fopen(path, "w");
    if (f) {
        fprintf(f, "%s %s\n", user, token);
        fclose(f);
        printf("Logged in as %s; the token (not the password) is kept in %s\n", user, path);
    }
    return true;
}

bool ra_web_resolve_hash (const char *hash, uint32_t *game_id) {
    rc_api_resolve_hash_request_t params = { .game_hash = hash };
    rc_api_request_t request;
    *game_id = 0;
    if (rc_api_init_resolve_hash_request(&request, &params) != RC_OK) {
        snprintf(error_text, sizeof(error_text), "could not build the hash request");
        return false;
    }
    rc_api_server_response_t answer;
    char *body = NULL;
    bool sent = ask_server(&request, &answer, &body);
    rc_api_destroy_request(&request);
    if (!sent) {
        return false;
    }
    rc_api_resolve_hash_response_t response;
    bool ok = (rc_api_process_resolve_hash_server_response(&response, &answer) == RC_OK) && response.response.succeeded;
    if (ok) {
        *game_id = response.game_id;
    } else {
        snprintf(error_text, sizeof(error_text), "hash lookup refused: %s",
                 response.response.error_message ? response.response.error_message : "no reason given");
    }
    rc_api_destroy_resolve_hash_response(&response);
    free(body);
    return ok;
}

bool ra_web_fetch_game (uint32_t game_id, rc_api_fetch_game_data_response_t *game) {
    rc_api_fetch_game_data_request_t params = { .username = user, .api_token = token, .game_id = game_id };
    rc_api_request_t request;
    if (rc_api_init_fetch_game_data_request(&request, &params) != RC_OK) {
        snprintf(error_text, sizeof(error_text), "could not build the game data request");
        return false;
    }
    rc_api_server_response_t answer;
    char *body = NULL;
    bool sent = ask_server(&request, &answer, &body);
    rc_api_destroy_request(&request);
    if (!sent) {
        return false;
    }
    bool ok = (rc_api_process_fetch_game_data_server_response(game, &answer) == RC_OK) && game->response.succeeded;
    if (!ok) {
        snprintf(error_text, sizeof(error_text), "game data refused: %s",
                 game->response.error_message ? game->response.error_message : "no reason given");
    }
    free(body);     /* the response keeps its own copies */
    return ok;
}
