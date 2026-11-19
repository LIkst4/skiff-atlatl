/* SPDX-License-Identifier: GPL-3.0-only */
/* The editor's files, built in (cmake/embed.cmake writes resources.cpp): web/editor.html, its fonts, its modules,
   the shim. */
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>

namespace fm1 {

struct Resource {
    const char *path;
    const uint8_t *data;
    size_t size;
};
const Resource *findResource(const std::string &path);

}  // namespace fm1
