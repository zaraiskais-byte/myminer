#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace caesar {

class BinaryWriter {
public:
    void write_u8(std::uint8_t value) {
        data_.push_back(value);
    }

    void write_u32(std::uint32_t value) {
        for (int i = 0; i < 4; ++i)
            data_.push_back(
                static_cast<std::uint8_t>(
                    (value >> (i * 8)) & 0xff));
    }

    void write_u64(std::uint64_t value) {
        for (int i = 0; i < 8; ++i)
            data_.push_back(
                static_cast<std::uint8_t>(
                    (value >> (i * 8)) & 0xff));
    }

    void write_bytes(
        const std::vector<std::uint8_t>& bytes) {

        data_.insert(
            data_.end(),
            bytes.begin(),
            bytes.end());
    }

    void write_string(const std::string& value) {
        if (value.size() >
            static_cast<std::size_t>(UINT32_MAX)) {

            throw std::runtime_error(
                "serialized string too large");
        }

        write_u32(
            static_cast<std::uint32_t>(
                value.size()));

        data_.insert(
            data_.end(),
            value.begin(),
            value.end());
    }

    const std::vector<std::uint8_t>& data() const {
        return data_;
    }

private:
    std::vector<std::uint8_t> data_;
};

class BinaryReader {
public:
    explicit BinaryReader(
        const std::vector<std::uint8_t>& data)
        : data_(data) {}

    std::uint8_t read_u8() {
        require(1);

        return data_[position_++];
    }

    std::uint32_t read_u32() {
        require(4);

        std::uint32_t value = 0;

        for (int i = 0; i < 4; ++i) {
            value |=
                static_cast<std::uint32_t>(
                    data_[position_++])
                << (i * 8);
        }

        return value;
    }

    std::uint64_t read_u64() {
        require(8);

        std::uint64_t value = 0;

        for (int i = 0; i < 8; ++i) {
            value |=
                static_cast<std::uint64_t>(
                    data_[position_++])
                << (i * 8);
        }

        return value;
    }

    std::vector<std::uint8_t> read_bytes(
        std::size_t size) {

        require(size);

        std::vector<std::uint8_t> result(
            data_.begin() + position_,
            data_.begin() + position_ + size);

        position_ += size;

        return result;
    }

    std::string read_string() {
        const std::uint32_t size = read_u32();

        if (size >
            data_.size() - position_) {

            throw std::runtime_error(
                "serialized string exceeds input");
        }

        std::string result(
            reinterpret_cast<const char*>(
                data_.data() + position_),
            size);

        position_ += size;

        return result;
    }

    bool empty() const {
        return position_ == data_.size();
    }

    std::size_t remaining() const {
        return data_.size() - position_;
    }

private:
    void require(std::size_t size) const {
        if (size > data_.size() - position_)
            throw std::runtime_error(
                "truncated binary data");
    }

    const std::vector<std::uint8_t>& data_;
    std::size_t position_{0};
};

inline std::string bytes_to_binary_string(
    const std::vector<std::uint8_t>& bytes) {

    return std::string(
        reinterpret_cast<const char*>(bytes.data()),
        bytes.size());
}

} // namespace caesar
