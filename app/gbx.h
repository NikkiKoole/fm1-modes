/* SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception */
/* GROOVEBOX: four tracks (DRUMS BASS KEYS LEAD), one way of doing things. See GROOVEBOX-SPEC.md.
 * Stage 1: the overview and the DRUMS track (the CR-78's voices and sequencer); BASS, KEYS and
 * LEAD are placeholders. */
#ifndef FM1_GBX_H
#define FM1_GBX_H
#include "fm1.h"
int gbx_control(const struct fm1_controls *controls);
unsigned gbx_active(void);
#endif
