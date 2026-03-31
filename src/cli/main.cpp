#include "cli/command_line.hpp"
#include "common/logger.hpp"
#include "common/http_client.hpp"
#include "common/stream_quality.hpp"
#include "parser/stream_extractor.hpp"
#include "downloader/download_manager.hpp"
#include <iostream>
#include <thread>
#include <filesystem>
#include <cstdlib>

using namespace n_m3u8dl;

LogLevel parse_log_level(const std::string& level) {
    if (level == "DEBUG") return LogLevel::DEBUG;
    if (level == "INFO") return LogLevel::INFO;
    if (level == "WARN") return LogLevel::WARN;
    if (level == "ERROR") return LogLevel::ERROR;
    if (level == "OFF") return LogLevel::OFF;
    return LogLevel::INFO;
}

void print_streams(const std::vector<StreamSpec>& streams) {
    std::cout << "\n=== Available Streams ===" << std::endl;
    for (size_t i = 0; i < streams.size(); ++i) {
        std::cout << "[" << i << "] " << streams[i].to_string() << std::endl;
    }
    std::cout << std::endl;
}

int select_stream_interactive(const std::vector<StreamSpec>& streams) {
    if (streams.empty()) {
        return -1;
    }

    if (streams.size() == 1) {
        Logger::info("Only one stream available, auto-selecting");
        return 0;
    }

    std::cout << "Select stream (0-" << (streams.size() - 1) << ", or q to quit): ";
    std::string input;
    std::getline(std::cin, input);

    if (input == "q" || input == "Q") {
        return -1;
    }

    try {
        int choice = std::stoi(input);
        if (choice >= 0 && choice < static_cast<int>(streams.size())) {
            return choice;
        } else {
            std::cerr << "Invalid selection. Please choose 0-" << (streams.size() - 1) << std::endl;
            return -1;
        }
    } catch (...) {
        std::cerr << "Invalid input. Please enter a number." << std::endl;
        return -1;
    }
}

bool mux_with_ffmpeg(const std::string& input_file, const std::string& output_file,
                     const std::string& ffmpeg_path) {
    Logger::info("Muxing with FFmpeg: " + input_file + " -> " + output_file);

    // Build FFmpeg command
    std::string command = ffmpeg_path + " -i \"" + input_file + "\" -c copy \"" + output_file + "\" -y";

    Logger::debug("Executing: " + command);

    int result = std::system(command.c_str());

    if (result == 0) {
        Logger::info("FFmpeg muxing completed successfully");
        return true;
    } else {
        Logger::error("FFmpeg muxing failed with exit code: " + std::to_string(result));
        return false;
    }
}

std::string find_ffmpeg_binary() {
    // Try common locations
    const char* paths[] = {
        "ffmpeg",                          // In PATH
        "/usr/bin/ffmpeg",                 // Linux
        "/usr/local/bin/ffmpeg",           // Homebrew/custom install
        "/opt/local/bin/ffmpeg",           // MacPorts
        "/opt/homebrew/bin/ffmpeg"         // Apple Silicon Homebrew
    };

    for (const char* path : paths) {
        std::string test_cmd = std::string(path) + " -version > /dev/null 2>&1";
        if (std::system(test_cmd.c_str()) == 0) {
            Logger::debug("Found FFmpeg at: " + std::string(path));
            return std::string(path);
        }
    }

    Logger::warn("FFmpeg not found in common locations");
    return "ffmpeg";  // Fallback to PATH
}

int main(int argc, char** argv) {
    try {
        // Parse command line
        auto options = parse_arguments(argc, argv);

        // Initialize logging
        if (!options.no_log) {
            Logger::init(parse_log_level(options.log_level),
                        options.log_file.value_or(""));
        }

        Logger::info("N_m3u8DL-RE C++ Port v0.5.1");
        Logger::info("Input: " + options.input_url);

        // Initialize HTTP client
        HttpClient::global_init();

        // Extract streams
        StreamExtractor extractor(options.input_url);
        auto streams = extractor.extract();

        if (streams.empty()) {
            Logger::error("No streams found");
            HttpClient::global_cleanup();
            return 1;
        }

        Logger::info("Found " + std::to_string(streams.size()) + " stream(s)");

        // Sort streams by quality (best first) before displaying
        StreamQuality::sort_by_quality(streams);

        print_streams(streams);

        if (options.skip_download) {
            Logger::info("Skipping download as requested");
            HttpClient::global_cleanup();
            return 0;
        }

        // Stream selection
        int stream_index = 0;

        if (options.select_stream.has_value()) {
            // User specified stream index via --select-stream
            stream_index = *options.select_stream;
            if (stream_index < 0 || stream_index >= static_cast<int>(streams.size())) {
                Logger::error("Invalid stream index: " + std::to_string(stream_index));
                Logger::error("Available: 0-" + std::to_string(streams.size() - 1));
                HttpClient::global_cleanup();
                return 1;
            }
            Logger::info("Selected stream [" + std::to_string(stream_index) + "] via command line");
        } else if (options.auto_select || streams.size() == 1) {
            // Auto-select best quality stream (streams are already sorted)
            stream_index = 0;
            Logger::info("Auto-selected best quality stream [0]");
        } else {
            // Interactive selection
            stream_index = select_stream_interactive(streams);
            if (stream_index < 0) {
                Logger::info("Download cancelled by user");
                HttpClient::global_cleanup();
                return 0;
            }
        }

        auto& selected_stream = streams[stream_index];
        Logger::info("Downloading: " + selected_stream.to_short_string());

        // If stream doesn't have playlist, fetch it (for HLS master playlists)
        if (!selected_stream.playlist && !selected_stream.url.empty()) {
            Logger::info("Fetching media playlist: " + selected_stream.url);
            StreamExtractor media_extractor(selected_stream.url);
            auto media_streams = media_extractor.extract();
            if (!media_streams.empty() && media_streams[0].playlist) {
                selected_stream.playlist = media_streams[0].playlist;
            } else {
                Logger::error("Failed to fetch media playlist");
                HttpClient::global_cleanup();
                return 1;
            }
        }

        // Setup download config
        DownloadConfig download_config;
        download_config.tmp_dir = options.tmp_dir.value_or("./temp");
        download_config.save_dir = options.save_dir.value_or(".");
        download_config.save_name = options.save_name.value_or("output");
        download_config.headers = options.headers;
        download_config.download_retry_count = options.download_retry_count;
        download_config.timeout = options.http_request_timeout;
        download_config.binary_merge = options.binary_merge;
        download_config.delete_after_done = options.del_after_done;

        // Create download manager and start download
        DownloadManager manager(download_config);

        if (!options.skip_merge) {
            bool success = manager.download_stream(selected_stream);

            if (!success) {
                Logger::error("Download failed");
                HttpClient::global_cleanup();
                return 1;
            }

            // FFmpeg muxing if requested
            if (options.mux_to_mp4) {
                std::string input_file = manager.get_last_output_file();
                std::string output_file = input_file;

                // Replace extension with .mp4
                size_t dot_pos = output_file.find_last_of('.');
                if (dot_pos != std::string::npos) {
                    output_file = output_file.substr(0, dot_pos) + ".mp4";
                } else {
                    output_file += ".mp4";
                }

                // Find FFmpeg binary
                std::string ffmpeg_path = options.ffmpeg_path.value_or(find_ffmpeg_binary());

                // Mux with FFmpeg
                bool mux_success = mux_with_ffmpeg(input_file, output_file, ffmpeg_path);

                if (mux_success) {
                    Logger::info("All done! Output: " + output_file);

                    // Optionally delete intermediate file
                    if (options.del_after_done) {
                        Logger::info("Removing intermediate file: " + input_file);
                        std::filesystem::remove(input_file);
                    }
                } else {
                    Logger::warn("FFmpeg muxing failed, but download succeeded");
                    Logger::info("Output: " + input_file);
                }
            } else {
                Logger::info("All done!");
            }

            HttpClient::global_cleanup();
            return 0;
        } else {
            Logger::info("Skipping merge as requested");
            HttpClient::global_cleanup();
            return 0;
        }

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        HttpClient::global_cleanup();
        return 1;
    }

    return 0;
}
