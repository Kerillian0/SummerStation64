/**
 * @file http.c
 * @brief HTTPS through WinHTTP (see http.h).
 */

#include <windows.h>
#include <winhttp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "rcheevos/src/util/rc_version.h"

#define BRIDGE_VERSION "0.1"

static char error_text[256];

const char *http_error (void) {
    return error_text;
}

static wchar_t *wide (const char *text) {
    int n = MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
    wchar_t *out = malloc(n * sizeof(wchar_t));
    if (out) {
        MultiByteToWideChar(CP_UTF8, 0, text, -1, out, n);
    }
    return out;
}

bool http_request (const char *url, const char *post_data, const char *content_type, char **body, size_t *length, int *status) {
    *body = NULL;
    *length = 0;
    *status = 0;
    bool ok = false;
    HINTERNET session = NULL, connection = NULL, request = NULL;
    wchar_t *wurl = wide(url);
    wchar_t host[256], path[2048];
    URL_COMPONENTS parts = { .dwStructSize = sizeof(URL_COMPONENTS),
                             .lpszHostName = host, .dwHostNameLength = 256,
                             .lpszUrlPath = path, .dwUrlPathLength = 2048 };
    if (!wurl || !WinHttpCrackUrl(wurl, 0, 0, &parts)) {
        snprintf(error_text, sizeof(error_text), "could not read the address %s", url);
        goto done;
    }

    char agent[128];
    snprintf(agent, sizeof(agent), "SummerStation64-Bridge/%s (Windows) rcheevos/%s", BRIDGE_VERSION, rc_version_string());
    wchar_t *wagent = wide(agent);
    session = WinHttpOpen(wagent, WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    free(wagent);
    if (session) {
        connection = WinHttpConnect(session, host, parts.nPort, 0);
    }
    if (connection) {
        request = WinHttpOpenRequest(connection, post_data ? L"POST" : L"GET", path, NULL, WINHTTP_NO_REFERER,
                                     WINHTTP_DEFAULT_ACCEPT_TYPES, (parts.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0);
    }
    if (!request) {
        snprintf(error_text, sizeof(error_text), "could not open a connection (Windows error %lu)", (unsigned long) GetLastError());
        goto done;
    }

    wchar_t headers[256] = L"";
    if (post_data) {
        char header[200];
        snprintf(header, sizeof(header), "Content-Type: %s\r\n", content_type ? content_type : "application/x-www-form-urlencoded");
        MultiByteToWideChar(CP_UTF8, 0, header, -1, headers, 256);
    }
    DWORD post_length = post_data ? (DWORD) strlen(post_data) : 0;
    if (!WinHttpSendRequest(request, post_data ? headers : WINHTTP_NO_ADDITIONAL_HEADERS, post_data ? (DWORD) -1L : 0,
                            (LPVOID) post_data, post_length, post_length, 0)
            || !WinHttpReceiveResponse(request, NULL)) {
        snprintf(error_text, sizeof(error_text), "the request failed (Windows error %lu): no internet?", (unsigned long) GetLastError());
        goto done;
    }

    DWORD code = 0, code_size = sizeof(code);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &code_size, WINHTTP_NO_HEADER_INDEX);
    *status = (int) code;

    size_t have = 0, room = 16384;
    char *data = malloc(room + 1);
    for (;;) {
        DWORD available = 0, got = 0;
        if (!data || !WinHttpQueryDataAvailable(request, &available)) {
            break;
        }
        if (available == 0) {
            ok = true;
            break;
        }
        if (have + available > room) {
            room = (have + available) * 2;
            char *bigger = realloc(data, room + 1);
            if (!bigger) {
                break;
            }
            data = bigger;
        }
        if (!WinHttpReadData(request, data + have, available, &got)) {
            break;
        }
        have += got;
    }
    if (ok) {
        data[have] = '\0';
        *body = data;
        *length = have;
    } else {
        free(data);
        snprintf(error_text, sizeof(error_text), "reading the answer failed (Windows error %lu)", (unsigned long) GetLastError());
    }

done:
    if (request) WinHttpCloseHandle(request);
    if (connection) WinHttpCloseHandle(connection);
    if (session) WinHttpCloseHandle(session);
    free(wurl);
    return ok;
}
