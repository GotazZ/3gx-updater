#ifndef PLUGIN_MANAGER_HPP
#define PLUGIN_MANAGER_HPP

#include <string>
#include <vector>

struct PluginInfo {
    std::string id;
    std::string name;
    std::string author;
    std::string description;
    std::string repo;
    std::vector<std::string> titleIds;
    std::string category;
    bool installed = false;
};

class PluginManager {
public:
    static std::vector<PluginInfo> parseIndexJson(const std::string& jsonStr);
    static std::string getLatestReleaseDownloadUrl(const std::string& repo);
    static bool installPlugin(const PluginInfo& plugin, const std::string& downloadUrl);
    static bool isPluginInstalled(const PluginInfo& plugin);
};

#endif
