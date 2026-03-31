#include "downloader/simple_downloader.hpp"
#include "common/crypto.hpp"
#include "common/util.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <filesystem>
#include <thread>
#include <chrono>

namespace n_m3u8dl {

SimpleDownloader::SimpleDownloader(const DownloadConfig& config)
    : config_(config) {
}

DownloadResult SimpleDownloader::download_with_retry(const std::string& url, const std::string& path,
                                                     int64_t start_range, int64_t end_range) {
    DownloadResult result;
    result.actual_file_path = path;

    for (int retry = 0; retry < config_.download_retry_count; ++retry) {
        try {
            // Create a new HttpClient for each request to avoid thread-safety issues
            HttpClient client;
            client.set_timeout(config_.timeout);
            client.set_ssl_verify(false);

            for (const auto& [key, value] : config_.headers) {
                client.add_header(key, value);
            }

            if (start_range >= 0 && end_range >= 0) {
                client.set_range(start_range, end_range);
            }

            auto bytes = client.get_bytes(url);
            if (bytes.empty()) {
                Logger::warn("Empty response for: " + url);
                if (retry < config_.download_retry_count - 1) {
                    // Exponential backoff: 2^retry seconds
                    int delay = (1 << retry);  // 1, 2, 4, 8 seconds
                    std::this_thread::sleep_for(std::chrono::seconds(delay));
                    continue;
                }
                return result;
            }

            if (!Util::write_file_bytes(path, bytes)) {
                Logger::error("Failed to write file: " + path);
                return result;
            }

            result.success = true;
            return result;

        } catch (const std::exception& e) {
            Logger::error("Download error: " + std::string(e.what()));
            if (retry < config_.download_retry_count - 1) {
                // Exponential backoff: 2^retry seconds
                int delay = (1 << retry);  // 1, 2, 4, 8 seconds
                Logger::info("Retrying in " + std::to_string(delay) + "s... (" +
                           std::to_string(retry + 1) + "/" +
                           std::to_string(config_.download_retry_count) + ")");
                std::this_thread::sleep_for(std::chrono::seconds(delay));
            }
        }
    }

    return result;
}

void SimpleDownloader::decrypt_segment(const std::string& file_path, const EncryptInfo& encrypt_info) {
    if (encrypt_info.method == EncryptMethod::NONE) {
        return;
    }

    if (!encrypt_info.key || !encrypt_info.iv) {
        Logger::warn("Missing decryption key or IV for: " + file_path);
        return;
    }

    try {
        switch (encrypt_info.method) {
            case EncryptMethod::AES_128:
                AESUtil::decrypt_file_inplace(file_path, *encrypt_info.key, *encrypt_info.iv, false);
                Logger::debug("Decrypted (AES-128 CBC): " + file_path);
                break;

            case EncryptMethod::AES_128_ECB:
                AESUtil::decrypt_file_inplace(file_path, *encrypt_info.key, {}, true);
                Logger::debug("Decrypted (AES-128 ECB): " + file_path);
                break;

            case EncryptMethod::CHACHA20:
                if (auto data = Util::read_file_bytes(file_path)) {
                    auto decrypted = ChaCha20Util::decrypt_per_1024_bytes(*data, *encrypt_info.key, *encrypt_info.iv);
                    Util::write_file_bytes(file_path, decrypted);
                    Logger::debug("Decrypted (ChaCha20): " + file_path);
                }
                break;

            default:
                Logger::warn("Unsupported encryption method");
                break;
        }
    } catch (const std::exception& e) {
        Logger::error("Decryption failed: " + std::string(e.what()));
    }
}

DownloadResult SimpleDownloader::download_segment(const MediaSegment& segment, const std::string& save_path) {
    // Check if already downloaded
    if (Util::file_exists(save_path)) {
        DownloadResult result;
        result.success = true;
        result.actual_file_path = save_path;
        return result;
    }

    std::string temp_path = save_path + ".tmp";

    int64_t start_range = segment.start_range.value_or(-1);
    int64_t end_range = segment.stop_range.value_or(-1);

    auto result = download_with_retry(segment.url, temp_path, start_range, end_range);

    if (!result.success) {
        return result;
    }

    // Decrypt if needed
    decrypt_segment(temp_path, segment.encrypt_info);

    // Rename to final path
    std::filesystem::rename(temp_path, save_path);
    result.actual_file_path = save_path;

    return result;
}

} // namespace n_m3u8dl
