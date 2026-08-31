#include "app_updater.hpp"
#include <cJSON.h>
#include <cstdio>
#include <cstring>
#include <curl/curl.h>
#include <3ds.h>

const char* AppUpdater::CURRENT_VERSION = "2.0.0";

AppUpdater::UpdateInfo AppUpdater::checkForUpdates(const std::string& repo) {
    UpdateInfo info = {false, "", "", ""};

    if (repo.empty()) return info;

    std::string apiUrl = "https://api.github.com/repos/" + repo + "/releases/latest";
    std::string jsonResp = Network::fetchUrl(apiUrl);

    if (jsonResp.empty()) return info;

    cJSON* root = cJSON_Parse(jsonResp.c_str());
    if (!root) return info;

    cJSON* tagName = cJSON_GetObjectItem(root, "tag_name");
    cJSON* body = cJSON_GetObjectItem(root, "body");
    cJSON* assets = cJSON_GetObjectItem(root, "assets");

    if (cJSON_IsString(tagName)) {
        info.latestVersion = cJSON_GetStringValue(tagName);
        if (info.latestVersion != CURRENT_VERSION) {
            info.available = true;
        }
    }

    if (cJSON_IsString(body)) {
        info.changelog = cJSON_GetStringValue(body);
    }

    if (cJSON_IsArray(assets) && info.available) {
        int assetCount = cJSON_GetArraySize(assets);
        for (int i = 0; i < assetCount; i++) {
            cJSON* asset = cJSON_GetArrayItem(assets, i);
            cJSON* name = cJSON_GetObjectItem(asset, "name");
            cJSON* url = cJSON_GetObjectItem(asset, "browser_download_url");

            if (cJSON_IsString(name) && cJSON_IsString(url)) {
                std::string filename = cJSON_GetStringValue(name);
                if (filename.length() >= 5 && filename.substr(filename.length() - 5) == ".3dsx") {
                    info.downloadUrl = cJSON_GetStringValue(url);
                    break;
                }
            }
        }
    }

    cJSON_Delete(root);
    return info;
}

bool AppUpdater::downloadUpdate(const std::string& url, const std::string& outputPath, NetworkProgress* progress) {
    if (url.empty()) return false;

    FILE* fp = fopen(outputPath.c_str(), "wb");
    if (!fp) return false;

    CURL* curl = curl_easy_init();
    if (!curl) {
        fclose(fp);
        return false;
    }

    NetworkProgress prog = {0, 0, 0.0f};
    if (progress) {
        prog = *progress;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "3GX-Updater/" + std::string(CURRENT_VERSION));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
        [](void* ptr, size_t size, size_t nmemb, void* stream) -> size_t {
            FILE* file = static_cast<FILE*>(stream);
            return fwrite(ptr, size, nmemb, file);
        });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION,
        [](void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t, curl_off_t) -> int {
            NetworkProgress* p = static_cast<NetworkProgress*>(clientp);
            p->dlTotal = (long long)dltotal;
            p->dlNow = (long long)dlnow;
            if (dltotal > 0) {
                p->percent = (float)dlnow / (float)dltotal * 100.0f;
            }
            return 0;
        });
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    if (progress) {
        *progress = prog;
    }

    return (res == CURLE_OK);
}

bool AppUpdater::applyUpdate(const std::string& sourcePath, const std::string& targetPath) {
    FILE* src = fopen(sourcePath.c_str(), "rb");
    if (!src) return false;

    FILE* dst = fopen(targetPath.c_str(), "wb");
    if (!dst) {
        fclose(src);
        return false;
    }

    char buf[4096];
    size_t read;
    while ((read = fread(buf, 1, sizeof(buf), src)) > 0) {
        if (fwrite(buf, 1, read, dst) != read) {
            fclose(src);
            fclose(dst);
            return false;
        }
    }

    fclose(src);
    fclose(dst);
    return true;
}