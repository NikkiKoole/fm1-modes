/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
#ifndef FM1_LAUNCHER_H
#define FM1_LAUNCHER_H
#include "fm1.h"
/* Called first in every control poll. 0: the stock UI owns the panel;
 * 1: the launcher or one of its modes owns it; 2: closing back to the stock UI. */
int launcher_control(const struct fm1_controls *controls);
unsigned launcher_active(void);
/* the modes it opens */
int game_control(const struct fm1_controls *controls);
unsigned game_active(void);
#endif
