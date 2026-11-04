// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <string>
#include <vector>

namespace mt::airwindows {

struct PluginDoc {
    const char* name;
    const char* category;
    const char* summary;
    const char* description;
};

const PluginDoc* getDoc(const std::string& name);
std::vector<const PluginDoc*> search(const std::string& query);
std::vector<const PluginDoc*> getByCategory(const std::string& category);
const std::vector<std::string>& getCategories();
size_t getPluginCount();

} // namespace mt::airwindows
