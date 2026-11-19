/* SPDX-License-Identifier: GPL-3.0-only
 * Prints each track parameter's stored id (core.h p_sid) and whether it is one of DRUM's lane models (those share
 * their ids with SYNTH's own: core.h sid_lanes), one line each: "p sid lanes". plugin/tools/gen_params.py reads it. */
#include <stdio.h>
#include "core.h"

int main(void)
{
    uint32_t p, s;
    for (p = 0; p < P_COUNT; p++) {
        int lanes = 0;
        for (s = 0; s < SID_COUNT; s++)
            if (sid_p_of(s, SID_COUNT, 1) == p && sid_p_of(s, SID_COUNT, 0) != p)
                lanes = 1;
        printf("%u %u %d\n", p, p_sid(p), lanes);
    }
    printf("SID_COUNT %u P_E0 %u SID_E0 %u\n", SID_COUNT, (uint32_t)P_E0, SID_E0);
    return 0;
}
