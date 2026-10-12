#pragma once

#include <cstdint>
#include <etl/string.h>

#include "sfg_sbs/block.hpp"
#include "sfg_sbs/data_type.hpp"

namespace sfg_sbs
{
    /// @brief Maps a @ref DataType spelling to the C++ type used by the typed API read and write operations.
    template <DataType Type>
    struct DataTypeToNativeTypeMapper;

    template <>
    struct DataTypeToNativeTypeMapper<DataType::Int>
    {
    public:
        using NativeType = std::int16_t;
    };

    template <>
    struct DataTypeToNativeTypeMapper<DataType::UnsignedInt>
    {
    public:
        using NativeType = std::uint16_t;
    };

    template <>
    struct DataTypeToNativeTypeMapper<DataType::Word>
    {
    public:
        using NativeType = std::uint16_t;
    };

    template <>
    struct DataTypeToNativeTypeMapper<DataType::Boolean>
    {
    public:
        using NativeType = bool;
    };

    template <>
    struct DataTypeToNativeTypeMapper<DataType::String>
    {
    public:
        using NativeType = etl::string<Block::s_max_data_size>;
    };

    template <>
    struct DataTypeToNativeTypeMapper<DataType::Data>
    {
    public:
        using NativeType = Block;
    };
}