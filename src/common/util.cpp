#include "common/util.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <filesystem>
#include <cctype>

namespace n_m3u8dl {

std::string Util::combine_url(const std::string& base, const std::string& relative) {
    if (is_absolute_url(relative)) {
        return relative;
    }

    if (relative.empty()) {
        return base;
    }

    if (relative[0] == '/') {
        // Absolute path
        size_t scheme_end = base.find("://");
        if (scheme_end != std::string::npos) {
            size_t host_end = base.find('/', scheme_end + 3);
            if (host_end != std::string::npos) {
                return base.substr(0, host_end) + relative;
            } else {
                return base + relative;
            }
        }
    }

    // Relative path
    std::string result = base;
    if (!result.empty() && result.back() != '/') {
        size_t last_slash = result.find_last_of('/');
        if (last_slash != std::string::npos) {
            result = result.substr(0, last_slash + 1);
        }
    }

    return result + relative;
}

std::string Util::get_valid_filename(const std::string& name, const std::string& replacement) {
    std::string result = name;
    const std::string invalid_chars = R"(<>:"/\|?*)";

    for (char& c : result) {
        if (invalid_chars.find(c) != std::string::npos) {
            c = replacement[0];
        }
    }

    return result;
}

bool Util::is_absolute_url(const std::string& url) {
    return url.find("://") != std::string::npos;
}

std::vector<uint8_t> Util::hex_to_bytes(const std::string& hex) {
    std::vector<uint8_t> bytes;
    std::string clean_hex = hex;

    // Remove "0x" prefix if present
    if (clean_hex.size() >= 2 && clean_hex[0] == '0' && (clean_hex[1] == 'x' || clean_hex[1] == 'X')) {
        clean_hex = clean_hex.substr(2);
    }

    for (size_t i = 0; i < clean_hex.length(); i += 2) {
        std::string byte_str = clean_hex.substr(i, 2);
        uint8_t byte = static_cast<uint8_t>(std::stoi(byte_str, nullptr, 16));
        bytes.push_back(byte);
    }

    return bytes;
}

std::string Util::bytes_to_hex(std::span<const uint8_t> bytes) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');

    for (uint8_t byte : bytes) {
        oss << std::setw(2) << static_cast<int>(byte);
    }

    return oss.str();
}

std::vector<uint8_t> Util::base64_decode(const std::string& encoded) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::vector<uint8_t> decoded;
    std::vector<int> T(256, -1);

    for (int i = 0; i < 64; i++) {
        T[base64_chars[i]] = i;
    }

    int val = 0, valb = -8;
    for (unsigned char c : encoded) {
        if (T[c] == -1) break;
        val = (val << 6) + T[c];
        valb += 6;
        if (valb >= 0) {
            decoded.push_back(static_cast<uint8_t>((val >> valb) & 0xFF));
            valb -= 8;
        }
    }

    return decoded;
}

std::string Util::base64_encode(std::span<const uint8_t> data) {
    static const std::string base64_chars =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    int val = 0, valb = -6;

    for (uint8_t c : data) {
        val = (val << 8) + c;
        valb += 8;
        while (valb >= 0) {
            encoded.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }

    if (valb > -6) {
        encoded.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    }

    while (encoded.size() % 4) {
        encoded.push_back('=');
    }

    return encoded;
}

bool Util::file_exists(const std::string& path) {
    return std::filesystem::exists(path);
}

void Util::create_directories(const std::string& path) {
    std::filesystem::create_directories(path);
}

std::optional<std::vector<uint8_t>> Util::read_file_bytes(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        return std::nullopt;
    }

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<uint8_t> buffer(size);
    if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) {
        return std::nullopt;
    }

    return buffer;
}

bool Util::write_file_bytes(const std::string& path, std::span<const uint8_t> data) {
    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

bool Util::is_mpeg_ts_buffer(std::span<const uint8_t> buffer) {
    if (buffer.size() < 188) {
        return false;
    }

    // Check for 0x47 sync byte at expected positions
    for (size_t i = 0; i < std::min(buffer.size(), size_t(188 * 3)); i += 188) {
        if (buffer[i] != 0x47) {
            return false;
        }
    }

    return true;
}

} // namespace n_m3u8dl
