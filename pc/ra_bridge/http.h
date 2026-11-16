/**
 * @file http.h
 * @brief One HTTPS request at a time, through Windows' own WinHTTP, for the
 * RetroAchievements web API (rcheevos builds the URL and POST data).
 */

#ifndef HTTP_H__
#define HTTP_H__

#include <stdbool.h>
#include <stddef.h>

/**
 * Send a request (POST when `post_data` is given, else GET).
 * @param body  Out: the answer, NUL-terminated; free() it.
 * @param status Out: the HTTP status code.
 */
bool http_request (const char *url, const char *post_data, const char *content_type, char **body, size_t *length, int *status);

/** What went wrong last. */
const char *http_error (void);

#endif
