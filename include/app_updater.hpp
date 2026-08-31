#ifndef APP_UPDATER_HPP
#define APP_UPDATER_HPP

#include <string>
#include "network.hpp"

class AppUpdater {
public:
    static const char* CURRENT_VERSION;

    struct UpdateInfo {
        bool available;
        std::string latestVersion;
        std::string downloadUrl;
        std::string changelog;
    };

    static UpdateInfo checkForUpdates(const std::string& repo);
    static bool downloadUpdate(const std::string& url, const std::string& outputPath, NetworkProgress* progress);
    static bool applyUpdate(const std::string& sourcePath, const std::string& targetPath);
};

#endif