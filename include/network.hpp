#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <string>

class Network {
public:
    static bool init();
    static void exit();
    static std::string fetchUrl(const std::string& url);
    static bool downloadFile(const std::string& url, const std::string& outputPath);
};

#endif
