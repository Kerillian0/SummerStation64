/**
 * @file party.h
 * @brief Party mode: how many controllers are plugged in.
 *
 * With two or more, a "Party" step joins the Games tab's C-up / C-down
 * filter, showing only the games for at least that many players
 * (games_filter.c), and Random game picks among them. Under the clock
 * the four ports show in place of the memory badge, the ones in use lit
 * (feature `controller_marks`).
 *
 * For testing without enough controllers: `party_test = 3` in
 * sd:/menu/options.ini counts that many, in ports 1 to 3, whatever is
 * plugged in. It is not offered in Settings; take the line out to count
 * the real ones again.
 */

#ifndef PARTY_H__
#define PARTY_H__

#include <stdbool.h>

/** How many controllers are plugged in (or `party_test`'s number). */
int party_players (void);

/** Whether a controller is in this port (0 to 3). */
bool party_port_in_use (int port);

#endif /* PARTY_H__ */
