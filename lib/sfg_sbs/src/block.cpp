#include "sfg_sbs/block.hpp"

namespace sfg_sbs
{
    Block::Block(etl::span<const std::uint8_t> data)
    {
        for (size_t index = 0; index < data.size() && m_data.size() < m_data.capacity(); index++)
        {
            m_data.push_back(data[index]);
        }
    }

    Block::Block(etl::string_view string)
    {
        for (size_t index = 0; index < string.length() && m_data.size() < m_data.capacity(); index++)
        {
            if (string[index] == '\0')
            {
                break;
            }
            m_data.push_back(static_cast<std::uint8_t>(string[index]));
        }
    }

    size_t Block::size() const
    {
        return m_data.size();
    }

    const std::uint8_t *Block::as_data() const
    {
        return m_data.data();
    }

    etl::string<Block::s_max_data_size> Block::as_string() const
    {
        etl::string<s_max_data_size> result;

        for (size_t index = 0; index < size(); index++)
        {
            result.push_back(static_cast<char>(as_data()[index]));
        }
        return result;
    }
}
