// include/lowlat/itch/parser.hpp
//
// File reader + streaming loop for ITCH 5.0 over SoupBinTCP.
//
// mmaps the file (zero-copy view) and walks through it message by message,
// stripping the SoupBinTCP frame wrapper and calling dispatch() on each
// ITCH message. The handler is a template parameter so dispatch + handler
// inline together.
//
// Wire layout per record in the file:
//   [2 bytes big-endian length N] [1 byte packet type] [N-1 bytes ITCH msg]
//
// Most packets have type 'S' (sequenced data). Heartbeats and other framing
// packets are skipped.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <lowlat/itch/dispatcher.hpp>
#include <lowlat/itch/messages.hpp>
#include <lowlat/core/compiler.hpp>

namespace lowlat::itch {

struct ParseStats {
    std::uint64_t total_packets   = 0;
    std::uint64_t messages      = 0;   // handled by our dispatcher
    std::uint64_t bytes_consumed  = 0;
};

// RAII wrapper for an mmap'd file.
class MappedFile {
public:
    explicit MappedFile(const std::string& path) {
        fd_ = ::open(path.c_str(), O_RDONLY);
        if (fd_ < 0) {
            throw std::runtime_error("open failed: " + path);
        }
        struct stat st{};
        if (::fstat(fd_, &st) < 0) {
            ::close(fd_);
            throw std::runtime_error("fstat failed: " + path);
        }
        size_ = static_cast<std::size_t>(st.st_size);
        void* p = ::mmap(nullptr, size_, PROT_READ, MAP_PRIVATE, fd_, 0);
        if (p == MAP_FAILED) {
            ::close(fd_);
            throw std::runtime_error("mmap failed: " + path);
        }
        data_ = static_cast<const std::byte*>(p);
        // Hint that we'll read sequentially — kernel can prefetch.
        ::madvise(p, size_, MADV_SEQUENTIAL);
    }

    ~MappedFile() {
        if (data_) ::munmap(const_cast<std::byte*>(data_), size_);
        if (fd_ >= 0) ::close(fd_);
    }

    MappedFile(const MappedFile&)            = delete;
    MappedFile& operator=(const MappedFile&) = delete;

    [[nodiscard]] const std::byte* data() const noexcept { return data_; }
    [[nodiscard]] std::size_t      size() const noexcept { return size_; }

private:
    int                 fd_   = -1;
    const std::byte*    data_ = nullptr;
    std::size_t         size_ = 0;
};

// Read a 2-byte big-endian length prefix from a buffer position.
[[nodiscard]] LOWLAT_ALWAYS_INLINE
std::uint16_t read_be16(const std::byte* p) noexcept {
    std::uint16_t raw;
    std::memcpy(&raw, p, sizeof(raw));
    if constexpr (std::endian::native == std::endian::little) {
        return std::byteswap(raw);
    }
    return raw;
}

// Main parsing loop. Walks the file and calls dispatch() on every
// ITCH message wrapped in a SoupBinTCP 'S' packet.
template <typename Handler>
ParseStats parse_file(const std::string& path, Handler&& handler) {
    MappedFile file(path);
    const std::byte* p    = file.data();
    const std::byte* end  = p + file.size();
    ParseStats stats;

    while (p + 3 <= end) {
        // SoupBinTCP framing: [2-byte BE length][1-byte packet type][payload]
        const std::uint16_t frame_len  = read_be16(p);
        const char          packet_type = static_cast<char>(p[2]);
        const std::byte*    payload     = p + 3;
        const std::size_t   payload_len = frame_len - 1; // length includes packet_type byte

        if (payload + payload_len > end) break;  // truncated trailing frame

        ++stats.total_packets;

        if (packet_type == 'S') {
            const std::size_t consumed = dispatch(payload, handler);
            if (consumed == 0) break;   // unknown type byte — stop
            ++stats.messages;
        }
        // Other packet types (heartbeats, login, etc.) are skipped silently.

        p = payload + payload_len;
    }
    stats.bytes_consumed = static_cast<std::uint64_t>(p - file.data());
    return stats;
}

}  // namespace lowlat::itch