/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* LOOP DECK: banks of loops that always fit together, toggled live. See deck.c. */
#ifndef FM1_DECK_H
#define FM1_DECK_H
#include "fm1.h"
int deck_control(const struct fm1_controls *controls);
unsigned deck_active(void);
#endif
