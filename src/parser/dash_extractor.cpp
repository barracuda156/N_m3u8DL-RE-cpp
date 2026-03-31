#include "parser/dash_extractor.hpp"
#include "common/util.hpp"
#include "common/logger.hpp"
#include <pugixml.hpp>
#include <sstream>

namespace n_m3u8dl {

DASHExtractor::DASHExtractor(const std::string& mpd_url, const std::string& base_url)
    : mpd_url_(mpd_url), base_url_(base_url.empty() ? mpd_url : base_url) {}

std::vector<StreamSpec> DASHExtractor::extract_streams(const std::string& mpd_content) {
    std::vector<StreamSpec> streams;

    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_string(mpd_content.c_str());

    if (!result) {
        Logger::error("Failed to parse MPD: " + std::string(result.description()));
        return streams;
    }

    auto mpd_node = doc.child("MPD");
    if (!mpd_node) {
        Logger::error("No MPD root node found");
        return streams;
    }

    bool is_live = std::string(mpd_node.attribute("type").value()) == "dynamic";

    // Iterate through periods
    for (auto period : mpd_node.children("Period")) {
        std::string period_base_url = base_url_;

        // Check for BaseURL in Period
        auto period_base_url_node = period.child("BaseURL");
        if (period_base_url_node) {
            period_base_url = Util::combine_url(period_base_url, period_base_url_node.child_value());
        }

        // Iterate through AdaptationSets
        for (auto adaptation_set : period.children("AdaptationSet")) {
            std::string as_base_url = period_base_url;

            auto as_base_url_node = adaptation_set.child("BaseURL");
            if (as_base_url_node) {
                as_base_url = Util::combine_url(as_base_url, as_base_url_node.child_value());
            }

            std::string content_type = adaptation_set.attribute("contentType").value();
            std::string mime_type = adaptation_set.attribute("mimeType").value();

            MediaType media_type = MediaType::VIDEO;
            if (content_type == "audio" || mime_type.find("audio") != std::string::npos) {
                media_type = MediaType::AUDIO;
            } else if (content_type == "text" || mime_type.find("text") != std::string::npos) {
                media_type = MediaType::SUBTITLES;
            }

            // Iterate through Representations
            for (auto representation : adaptation_set.children("Representation")) {
                StreamSpec spec;
                spec.media_type = media_type;

                std::string repr_base_url = as_base_url;
                auto repr_base_url_node = representation.child("BaseURL");
                if (repr_base_url_node) {
                    repr_base_url = Util::combine_url(repr_base_url, repr_base_url_node.child_value());
                }

                spec.url = repr_base_url;
                spec.bandwidth = representation.attribute("bandwidth").as_llong();
                spec.codecs = representation.attribute("codecs").value();

                auto width = representation.attribute("width").as_int();
                auto height = representation.attribute("height").as_int();
                if (width > 0 && height > 0) {
                    spec.resolution = std::to_string(width) + "x" + std::to_string(height);
                }

                auto frame_rate_attr = representation.attribute("frameRate");
                if (frame_rate_attr) {
                    std::string fr_str = frame_rate_attr.value();
                    size_t slash = fr_str.find('/');
                    if (slash != std::string::npos) {
                        double num = std::stod(fr_str.substr(0, slash));
                        double den = std::stod(fr_str.substr(slash + 1));
                        spec.frame_rate = num / den;
                    } else {
                        spec.frame_rate = std::stod(fr_str);
                    }
                }

                spec.language = adaptation_set.attribute("lang").value();

                // Parse SegmentTemplate or SegmentList for playlist
                auto seg_template = representation.child("SegmentTemplate");
                if (!seg_template) {
                    seg_template = adaptation_set.child("SegmentTemplate");
                }

                if (seg_template) {
                    Playlist playlist;
                    playlist.is_live = is_live;

                    // This is simplified - full implementation would parse timelines
                    auto timeline = seg_template.child("SegmentTimeline");
                    if (timeline) {
                        MediaPart part;
                        int index = 0;
                        for (auto s : timeline.children("S")) {
                            MediaSegment seg;
                            seg.duration = s.attribute("d").as_double() / s.attribute("t").as_double();
                            seg.index = index++;

                            std::string media_template = seg_template.attribute("media").value();
                            // Substitute template variables (simplified)
                            // Full implementation would handle $Number$, $Time$, $RepresentationID$
                            seg.url = Util::combine_url(repr_base_url, media_template);

                            part.segments.push_back(seg);
                            playlist.total_duration += seg.duration;
                        }
                        if (!part.segments.empty()) {
                            playlist.media_parts.push_back(part);
                        }
                    }

                    spec.playlist = playlist;
                    spec.segments_count = spec.playlist->media_parts.size() > 0 ?
                        spec.playlist->media_parts[0].segments.size() : 0;
                }

                streams.push_back(spec);
            }
        }
    }

    return streams;
}

} // namespace n_m3u8dl
