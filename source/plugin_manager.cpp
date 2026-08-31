#include "plugin_manager.hpp"
#include "network.hpp"
#include "cJSON.h"
#include <sys/stat.h>
#include <iostream>

static void createDirIfNeeded(const std::string& path) {
    struct stat st = {0};
    if (stat(path.c_str(), &st) == -1) {
        mkdir(path.c_str(), 0777);
    }
}

std::vector<PluginInfo> PluginManager::parseIndexJson(const std::string& jsonStr) {
    std::vector<PluginInfo> list;
    cJSON* root = cJSON_Parse(jsonStr.c_str());
    if (!root) return list;

    cJSON* plugins = cJSON_GetObjectItem(root, "plugins");
    if (cJSON_IsArray(plugins)) {
        int count = cJSON_GetArraySize(plugins);
        for (int i = 0; i < count; i++) {
            cJSON* item = cJSON_GetArrayItem(plugins, i);
            if (!item) continue;

            PluginInfo p;
            cJSON* id = cJSON_GetObjectItem(item, "id");
            cJSON* name = cJSON_GetObjectItem(item, "name");
            cJSON* author = cJSON_GetObjectItem(item, "author");
            cJSON* desc = cJSON_GetObjectItem(item, "description");
            cJSON* repo = cJSON_GetObjectItem(item, "repo");
            cJSON* cat = cJSON_GetObjectItem(item, "category");
            cJSON* tids = cJSON_GetObjectItem(item, "titleIds");

            if (cJSON_IsString(id)) p.id = id->valuestring;
            if (cJSON_IsString(name)) p.name = name->valuestring;
            if (cJSON_IsString(author)) p.author = author->valuestring;
            if (cJSON_IsString(desc)) p.description = desc->valuestring;
            if (cJSON_IsString(repo)) p.repo = repo->valuestring;
            if (cJSON_IsString(cat)) p.category = cat->valuestring;

            if (cJSON_IsArray(tids)) {
                int tidCount = cJSON_GetArraySize(tids);
                for (int j = 0; j < tidCount; j++) {
                    cJSON* tid = cJSON_GetArrayItem(tids, j);
                    if (cJSON_IsString(tid)) p.titleIds.push_back(tid->valuestring);
                }
            }

            p.installed = isPluginInstalled(p);
            list.push_back(p);
        }
    }

    cJSON_Delete(root);
    return list;
}

std::string PluginManager::getLatestReleaseDownloadUrl(const std::string& repo) {
    if (repo.empty()) return "";

    std::string apiUrl = "https://api.github.com/repos/" + repo + "/releases/latest";
    std::string jsonResp = Network::fetchUrl(apiUrl);

    if (jsonResp.empty()) return "";

    cJSON* root = cJSON_Parse(jsonResp.c_str());
    if (!root) return "";

    std::string downloadUrl = "";
    cJSON* assets = cJSON_GetObjectItem(root, "assets");
    if (cJSON_IsArray(assets)) {
        int assetCount = cJSON_GetArraySize(assets);
        for (int i = 0; i < assetCount; i++) {
            cJSON* asset = cJSON_GetArrayItem(assets, i);
            cJSON* name = cJSON_GetObjectItem(asset, "name");
            cJSON* url = cJSON_GetObjectItem(asset, "browser_download_url");

            if (cJSON_IsString(name) && cJSON_IsString(url)) {
                std::string filename = name->valuestring;
                if (filename.length() >= 4 && filename.substr(filename.length() - 4) == ".3gx") {
                    downloadUrl = url->valuestring;
                    break;
                }
            }
        }
    }

    cJSON_Delete(root);
    return downloadUrl;
}

bool PluginManager::isPluginInstalled(const PluginInfo& plugin) {
    if (plugin.titleIds.empty()) return false;
    std::string targetPath = "/luma/plugins/" + plugin.titleIds[0] + "/plugin.3gx";
    struct stat buffer;
    return (stat(targetPath.c_str(), &buffer) == 0);
}

bool PluginManager::installPlugin(const PluginInfo& plugin, const std::string& downloadUrl) {
    if (downloadUrl.empty() || plugin.titleIds.empty()) return false;

    createDirIfNeeded("/luma");
    createDirIfNeeded("/luma/plugins");

    bool success = true;
    for (const auto& tid : plugin.titleIds) {
        std::string dirPath = "/luma/plugins/" + tid;
        createDirIfNeeded(dirPath);

        std::string targetPath = dirPath + "/plugin.3gx";
        if (!Network::downloadFile(downloadUrl, targetPath)) {
            success = false;
        }
    }

    return success;
}
