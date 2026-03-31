#pragma once

#include "common/types.hpp"
#include <vector>
#include <algorithm>
#include <cstdint>

namespace n_m3u8dl {

// Stream quality ranking utilities
class StreamQuality {
public:
    // Calculate quality score for a stream (higher = better)
    static int64_t calculate_score(const StreamSpec& stream);

    // Sort streams by quality (best first)
    static void sort_by_quality(std::vector<StreamSpec>& streams);

    // Get the best quality stream from a list
    static int get_best_stream_index(const std::vector<StreamSpec>& streams);

private:
    // Parse resolution string like "1920x1080" to pixel count
    static int64_t parse_resolution(const std::string& resolution);

    // Get codec quality score
    static int get_codec_score(const std::string& codec);

    // Get video range score (HDR > SDR)
    static int get_video_range_score(const std::string& video_range);
};

} // namespace n_m3u8dl
