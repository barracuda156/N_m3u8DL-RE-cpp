#pragma once

#include "common/types.hpp"
#include "common/http_client.hpp"
#include <string>
#include <map>
#include <memory>

namespace n_m3u8dl {

struct DownloadConfig {
    std::string tmp_dir = "./temp";
    std::string save_dir = ".";
    std::string save_name = "output";  // Default filename
    std::map<std::string, std::string> headers;
    int download_retry_count = 3;
    int timeout = 100;
    bool binary_merge = false;
    bool delete_after_done = true;
};

class SimpleDownloader {
public:
    explicit SimpleDownloader(const DownloadConfig& config);

    DownloadResult download_segment(const MediaSegment& segment, const std::string& save_path);

private:
    DownloadConfig config_;

    DownloadResult download_with_retry(const std::string& url, const std::string& path,
                                       int64_t start_range = -1, int64_t end_range = -1);

    void decrypt_segment(const std::string& file_path, const EncryptInfo& encrypt_info);
};

} // namespace n_m3u8dl
