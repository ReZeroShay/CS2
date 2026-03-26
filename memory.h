#pragma once

#include <concepts>
#include <span>
#include <optional>
#include <type_traits>
#include <string_view>
template <typename R>
concept BasicMemoryReader = std::movable<R> && requires(R r, std::size_t virtualAddress, std::span<std::uint8_t> dest,
                                                        std::wstring_view module) {
    { r.read_into(virtualAddress, dest) } -> std::same_as<std::size_t>;
    { r.get_module_base(module) } -> std::same_as<std::size_t>;
};

template <class R>
    requires BasicMemoryReader<R>
struct MemoryReaderAdapter {

    template <typename... Args> explicit MemoryReaderAdapter(Args &&...args) : reader(std::forward<Args>(args)...) {}

    [[nodiscard]] auto get_module_base(std::wstring_view name) noexcept -> std::size_t {
        return reader.get_module_base(name);
    }

    [[nodiscard]] auto read_into(std::size_t virtualAddress, std::span<std::uint8_t> dest) noexcept -> std::size_t {
        return reader.read_into(virtualAddress, dest);
    }

    template <typename T>
        requires std::is_trivially_copyable_v<T>
    [[nodiscard]] inline auto read_as(std::size_t virtualAddress) noexcept -> std::optional<T> {
        alignas(T) std::uint8_t buf[sizeof(T)]{};
        auto bytes = reader.read_into(virtualAddress, std::span{buf});
        if (bytes != sizeof(T)) {
            return std::nullopt;
        }
        return std::bit_cast<T>(buf);
    }

    [[nodiscard]] auto underlying() noexcept -> R & { return reader; }
    [[nodiscard]] auto underlying() const noexcept -> const R & { return reader; }
    R reader;
};

template <typename R> using MemoryReader = MemoryReaderAdapter<R>;
