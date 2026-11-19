/* SPDX-License-Identifier: GPL-3.0-only */
/* The CLAP entry (clap-wrapper builds the VST3, AUv2 and standalone around it). */
#include <clap/clap.h>

extern "C" {
bool fm1_entry_init(const char *path);
void fm1_entry_deinit(void);
const void *fm1_entry_get_factory(const char *id);

#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"
#endif
CLAP_EXPORT const clap_plugin_entry_t clap_entry = {CLAP_VERSION_INIT, fm1_entry_init, fm1_entry_deinit,
                                                    fm1_entry_get_factory};
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}
