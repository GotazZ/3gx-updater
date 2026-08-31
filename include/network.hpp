#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <string>

struct NetworkProgress {
    long long dlNow;
    long long dlTotal;
    float percent;
};

class Network {
public:
    static bool init();
    static void exit();
    static std::string fetchUrl(const std::string& url);
    static bool downloadFile(const std::string& url, const std::string& outputPath);
    static bool getFileSize(const std::string& url, long long& outSize);
    static void setProgressCallback(NetworkProgress* progress);
    static NetworkProgress getProgress();
    static void resetProgress();
};

#endif
