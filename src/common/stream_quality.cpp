#include "common/stream_quality.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <cctype>

namespace n_m3u8dl {

int64_t StreamQuality::parse_resolution(const std::string& resolution) {
    // Parse "1920x1080" or "1920*1080" format
    if (resolution.empty()) {
        return 0;
    }

    size_t sep_pos = resolution.find('x');
    if (sep_pos == std::string::npos) {
        sep_pos = resolution.find('*');
    }
    if (sep_pos == std::string::npos) {
        return 0;
    }

    try {
        int64_t width = std::stoll(resolution.substr(0, sep_pos));
        int64_t height = std::stoll(resolution.substr(sep_pos + 1));
        return width * height;  // Total pixels
    } catch (...) {
        return 0;
    }
}

int StreamQuality::get_codec_score(const std::string& codec) {
    // Convert to lowercase for comparison
    std::string codec_lower = codec;
    std::transform(codec_lower.begin(), codec_lower.end(), codec_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    // Prioritize codecs by DECODE efficiency (less CPU = better)
    // Perfect for older hardware like macOS 10.6, embedded devices, etc.

    // H.264/AVC codecs (BEST: lightest decode, universal hardware support)
    if (codec_lower.find("avc") != std::string::npos ||  // avc1, avc3
        codec_lower.find("h264") != std::string::npos) {
        return 100;
    }

    // H.265/HEVC codecs (GOOD: moderate decode load, common hardware support)
    if (codec_lower.find("hev") != std::string::npos ||  // hev1, hvc1
        codec_lower.find("h265") != std::string::npos) {
        return 90;
    }

    // VP9 (MODERATE: heavier decode, limited hardware support)
    if (codec_lower.find("vp9") != std::string::npos ||
        codec_lower.find("vp09") != std::string::npos) {
        return 70;
    }

    // VP8 (LIGHT: older but efficient)
    if (codec_lower.find("vp8") != std::string::npos ||
        codec_lower.find("vp08") != std::string::npos) {
        return 85;
    }

    // AV1 (FALLBACK: heaviest decode, newest, limited hardware support)
    if (codec_lower.find("av01") != std::string::npos ||
        codec_lower.find("av1") != std::string::npos) {
        return 60;
    }

    // AAC audio (if this is audio stream)
    if (codec_lower.find("mp4a") != std::string::npos) {
        return 80;
    }

    // Unknown codec
    return 50;
}

int StreamQuality::get_video_range_score(const std::string& video_range) {
    std::string range_lower = video_range;
    std::transform(range_lower.begin(), range_lower.end(), range_lower.begin(),
                   [](unsigned char c) { return std::tolower(c); });

    if (range_lower.find("hdr10+") != std::string::npos ||
        range_lower.find("hdr10plus") != std::string::npos) {
        return 30;
    }

    if (range_lower.find("hdr") != std::string::npos ||
        range_lower.find("pq") != std::string::npos ||
        range_lower.find("hlg") != std::string::npos) {
        return 20;
    }

    // SDR or unspecified
    return 10;
}

int64_t StreamQuality::calculate_score(const StreamSpec& stream) {
    int64_t score = 0;

    // 1. Resolution (most important - millions of pixels)
    //    4K (3840x2160) = 8,294,400 pixels
    //    1080p (1920x1080) = 2,073,600 pixels
    //    720p (1280x720) = 921,600 pixels
    if (stream.resolution) {
        int64_t pixels = parse_resolution(*stream.resolution);
        score += pixels * 1000;  // Weight: 1000 per pixel
    }

    // 2. Bandwidth (important - indicates bitrate quality)
    //    Higher bitrate = better quality at same resolution
    //    Typical: 10Mbps = 10,000,000 bps
    score += stream.bandwidth / 1000;  // Weight: divide by 1000 to not overwhelm resolution

    // 3. Frame rate (moderate importance)
    //    60fps > 30fps > 24fps
    if (stream.frame_rate) {
        score += static_cast<int64_t>(*stream.frame_rate * 100000);  // Weight: 100k per fps
    }

    // 4. Codec (moderate importance)
    //    H.265 > VP9 > AV1 > H.264
    if (stream.codecs) {
        score += get_codec_score(*stream.codecs) * 10000;  // Weight: 10k per codec score point
    }

    // 5. Video range (less important but nice to have)
    //    HDR10+ > HDR > SDR
    if (stream.video_range) {
        score += get_video_range_score(*stream.video_range) * 5000;  // Weight: 5k per range score point
    }

    return score;
}

void StreamQuality::sort_by_quality(std::vector<StreamSpec>& streams) {
    std::sort(streams.begin(), streams.end(),
              [](const StreamSpec& a, const StreamSpec& b) {
                  return calculate_score(a) > calculate_score(b);  // Descending order
              });
}

int StreamQuality::get_best_stream_index(const std::vector<StreamSpec>& streams) {
    if (streams.empty()) {
        return -1;
    }

    int64_t best_score = calculate_score(streams[0]);
    int best_index = 0;

    for (size_t i = 1; i < streams.size(); ++i) {
        int64_t score = calculate_score(streams[i]);
        if (score > best_score) {
            best_score = score;
            best_index = static_cast<int>(i);
        }
    }

    return best_index;
}

} // namespace n_m3u8dl
