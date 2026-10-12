#pragma once

#include <cstddef>
#include <cstdint>
#include <etl/span.h>
#include <etl/string.h>
#include <etl/string_view.h>
#include <etl/vector.h>

namespace sfg_sbs
{
    /// @brief Stores the raw wire block.
    ///
    /// @note The count is not stored as an explicit member, it is part of the data. The transport adds the
    ///       count byte when emitting a block and strips it when consuming one, so a Block holds
    ///       only the data bytes the count byte describes.
    class Block
    {
    public:
        /// @brief Maximum block data size in bytes as specified by the Smart Battery Data Specification
        ///        Rev 1.1, Appendix B.
        static constexpr size_t s_max_data_size = 32;
        /// @brief Max wire size in bytes of a block transfer: 1 count byte + data.
        static constexpr size_t s_max_wire_size = s_max_data_size + 1;

        /// @brief Constructs an empty block.
        Block() = default;

        /// @brief Constructs a block from a span of bytes.
        /// @param data The bytes to copy into a block.
        Block(etl::span<const std::uint8_t> data);

        /// @brief Constructs a block from the bytes of a string view.
        /// @param string The string to copy into a block.
        Block(etl::string_view string);

        /// @brief Number of data bytes held; equal to the block count byte on the wire.
        size_t size() const;

        /// @brief Pointer to the data bytes, in wire order.
        const std::uint8_t *as_data() const;

        /// @brief ASCII interpretation of a block.
        /// @returns Contents as a string, keeping embedded NUL bytes as data.
        etl::string<s_max_data_size> as_string() const;

    private:
        etl::vector<std::uint8_t, s_max_data_size> m_data;
    };
}
