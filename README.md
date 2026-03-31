# N_m3u8DL-RE (C++ port)

A C++20 port of the cross-platform DASH/HLS/MSS downloader [N_m3u8DL-RE](https://github.com/nilaoda/N_m3u8DL-RE).
Implementation done with Claude Code.

## Features

- **HLS Support**: Download HTTP Live Streaming (M3U8) content
- **DASH Support**: Download MPEG-DASH (MPD) content
- **Encryption**: AES-128 CBC/ECB and ChaCha20 decryption
- **Parallel Downloads**: Multi-threaded segment downloading
- **Interactive Stream Selection**: Choose quality, language, and codec
- **FFmpeg Integration**: Automatic MP4 muxing
- **Cross-Platform**: Linux and macOS support

## Requirements

- C++20 compatible compiler (GCC 10+, Clang 10+ or Apple Clang 13+)
- CMake 3.20+

## Dependencies

- libcurl - HTTP client
- OpenSSL - Cryptography
- pugixml - XML parsing (DASH manifests)
- CLI11 - Command-line parsing
- spdlog - Logging (requires fmt as transitive dependency on MacPorts)

## Usage

### Basic Usage

```bash
# Download HLS stream (interactive quality selection)
N_m3u8DL-RE "https://example.com/playlist.m3u8"

# Download DASH stream
N_m3u8DL-RE "https://example.com/manifest.mpd"

# Select specific stream by index
N_m3u8DL-RE "URL" --select-stream 0

# Auto-select first stream (non-interactive)
N_m3u8DL-RE "URL" --auto-select

# Download and mux to MP4 automatically
N_m3u8DL-RE "URL" --mux-to-mp4
```

### Advanced Options

```bash
# Specify output directory and name
N_m3u8DL-RE "URL" --save-dir ./downloads --save-name video

# Custom headers
N_m3u8DL-RE "URL" -H "User-Agent: Custom" -H "Cookie: session=xyz"

# Set thread count
N_m3u8DL-RE "URL" --thread-count 16

# Use proxy
N_m3u8DL-RE "URL" --custom-proxy http://127.0.0.1:8080

# Adjust timeout and retry
N_m3u8DL-RE "URL" --http-request-timeout 120 --download-retry-count 5

# Enable debug logging
N_m3u8DL-RE "URL" --log-level DEBUG --log-file-path ./download.log
```

### Command-Line Options

```
Options:
  input                         Input URL or file (required)
  --tmp-dir DIR                 Temporary file directory
  --save-dir DIR                Output directory
  --save-name NAME              Output filename
  --log-file-path PATH          Log file path
  --base-url URL                Base URL for relative paths
  --thread-count N              Download thread count (0 = auto, default: 0)
  --download-retry-count N      Retry count per segment (default: 3)
  --http-request-timeout SEC    HTTP timeout in seconds (default: 100)
  --binary-merge                Binary merge mode
  --skip-merge                  Skip merging segments
  --skip-download               Skip download (parse only)
  --del-after-done              Delete temp files after completion (default: true)
  --no-log                      Disable logging
  --log-level LEVEL             Log level: DEBUG|INFO|WARN|ERROR|OFF (default: INFO)
  -H, --header HEADER           Custom HTTP headers (can be used multiple times)
  --custom-proxy URL            Custom proxy URL
  --use-system-proxy            Use system proxy (default: true)
  --select-stream N             Select stream by index (0-based)
  --auto-select                 Automatically select first stream (no prompt)
  --mux-to-mp4                  Mux output to MP4 using FFmpeg
  --ffmpeg-binary-path PATH     Custom FFmpeg binary path (alias: --ffmpeg-path)
```
