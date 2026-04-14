#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

class DrowsinessBridge {
   public:
    // Shared memory layout (32 bytes):
    // [0..3]  : magic "EARS"
    // [4..7]  : version (u32, little-endian)
    // [8..11] : sequence (u32, writer flips odd/even)
    // [12..15]: status  (u32, 0=awake, 1=drowsy, 2=no-face)
    explicit DrowsinessBridge(std::string shm_name) : shm_name_(std::move(shm_name)) {}

    ~DrowsinessBridge() {
        if (map_) {
            munmap(map_, kMapSize);
            map_ = nullptr;
        }
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
    }

    bool poll() {
        if (!ensure_mapping_()) {
            is_drowsy_ = false;
            return false;
        }

        const auto* bytes = static_cast<const uint8_t*>(map_);
        if (!(bytes[0] == 'E' && bytes[1] == 'A' && bytes[2] == 'R' && bytes[3] == 'S')) {
            is_drowsy_ = false;
            return false;
        }

        uint32_t version = read_u32_le_(bytes + 4);
        if (version != 1U) {
            is_drowsy_ = false;
            return false;
        }

        // Retry a few times to avoid torn reads while writer updates.
        for (int i = 0; i < 4; ++i) {
            uint32_t seq_start = read_u32_le_(bytes + 8);
            uint32_t status    = read_u32_le_(bytes + 12);
            uint32_t seq_end   = read_u32_le_(bytes + 8);

            if ((seq_start & 1U) == 0U && seq_start == seq_end) {
                is_drowsy_ = (status == 1U);
                return is_drowsy_;
            }
        }

        // If updates are in progress continuously, keep last stable value.
        return is_drowsy_;
    }

    bool is_drowsy() const { return is_drowsy_; }

   private:
    static uint32_t read_u32_le_(const uint8_t* p) {
        return static_cast<uint32_t>(p[0]) | (static_cast<uint32_t>(p[1]) << 8) |
               (static_cast<uint32_t>(p[2]) << 16) | (static_cast<uint32_t>(p[3]) << 24);
    }

    bool ensure_mapping_() {
        if (map_) {
            return true;
        }

        fd_ = shm_open(shm_name_.c_str(), O_RDONLY, 0);
        if (fd_ < 0) {
            if (errno != ENOENT) {
                std::fprintf(stderr, "[drowsy] WARNING: shm_open(%s) failed: %s\n", shm_name_.c_str(),
                             std::strerror(errno));
            }
            return false;
        }

        map_ = mmap(nullptr, kMapSize, PROT_READ, MAP_SHARED, fd_, 0);
        if (map_ == MAP_FAILED) {
            std::fprintf(stderr, "[drowsy] WARNING: mmap(%s) failed: %s\n", shm_name_.c_str(),
                         std::strerror(errno));
            map_ = nullptr;
            close(fd_);
            fd_ = -1;
            return false;
        }

        return true;
    }

    static constexpr size_t kMapSize = 32;

    std::string shm_name_;
    int         fd_       = -1;
    void*       map_      = nullptr;
    bool        is_drowsy_ = false;
};