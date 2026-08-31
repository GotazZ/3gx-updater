#include "network.hpp"
#include <3ds.h>
#include <curl/curl.h>
#include <malloc.h>
#include <cstdio>

#define SOC_ALIGN       0x1000
#define SOC_BUFFERSIZE  0x100000

static u32* socBuffer = nullptr;

static size_t writeStringCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    std::string* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

static size_t writeFileCallback(void* ptr, size_t size, size_t nmemb, void* stream) {
    FILE* file = static_cast<FILE*>(stream);
    return fwrite(ptr, size, nmemb, file);
}

bool Network::init() {
    socBuffer = static_cast<u32*>(memalign(SOC_ALIGN, SOC_BUFFERSIZE));
    if (!socBuffer) return false;

    Result res = socInit(socBuffer, SOC_BUFFERSIZE);
    if (R_FAILED(res)) {
        free(socBuffer);
        socBuffer = nullptr;
        return false;
    }

    curl_global_init(CURL_GLOBAL_ALL);
    return true;
}

void Network::exit() {
    curl_global_cleanup();
    if (socBuffer) {
        socExit();
        free(socBuffer);
        socBuffer = nullptr;
    }
}

std::string Network::fetchUrl(const std::string& url) {
    CURL* curl = curl_easy_init();
    std::string response;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "3GX-Updater/1.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeStringCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }

    return response;
}

bool Network::downloadFile(const std::string& url, const std::string& outputPath) {
    CURL* curl = curl_easy_init();
    if (!curl) return false;

    FILE* fp = fopen(outputPath.c_str(), "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "3GX-Updater/1.0");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFileCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

    CURLcode res = curl_easy_perform(curl);
    fclose(fp);
    curl_easy_cleanup(curl);

    return (res == CURLE_OK);
}
