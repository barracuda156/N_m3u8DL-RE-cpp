#pragma once

#include "common/types.hpp"
#include "downloader/simple_downloader.hpp"
#include <vector>
#include <string>
#include <memory>

namespace n_m3u8dl {

class DownloadManager {
public:
    explicit DownloadManager(const DownloadConfig& config);

    // track_suffix distinguishes segment temp files and the output file
    // name when downloading multiple tracks (e.g. video + audio) into the
    // same tmp_dir/save_dir, so they don't collide or overwrite each other.
    bool download_stream(const StreamSpec& stream, const std::string& track_suffix = "");
    std::string get_last_output_file() const { return last_output_file_; }

private:
    DownloadConfig config_;
    std::unique_ptr<SimpleDownloader> downloader_;
    std::string last_output_file_;

    void merge_segments(const std::vector<std::string>& segment_files, const std::string& output_file);
    void cleanup_temp_files(const std::vector<std::string>& files);
};

} // namespace n_m3u8dl
