#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wconversion"
#pragma GCC diagnostic ignored "-Wpedantic"
#include <Arduino.h>
#pragma GCC diagnostic pop

#include <magic_enum.hpp>
#include <Wire.h>

#include "sfg_sbs/sbs.hpp"

// The uC has 3 HW I2Cs on the following pins:
// - I2C1
//     - SDA: PB7, PB9
//     - SCL: PB6, PB8
// - I2C2
//     - SDA: PB11, PB14
//     - SCL: PB10, PB13
// - I2C3
//     - SDA: PC1
//     - SCL: PC0
TwoWire host_wire = TwoWire(PC1, PC0);
sfg_sbs::Host<TwoWire> host = sfg_sbs::Host(host_wire);

void setup()
{
    host_wire.begin();
    Serial.begin(115200);
    delay(1000);
}

void loop()
{
    for (auto command : magic_enum::enum_values<sfg_sbs::Command>())
    {
        const auto traits = sfg_sbs::get_runtime_traits(command).value();

        if (traits.m_optional)
        {
            continue;
        }
        const auto result = host.read(command);

        if (!result.has_value())
        {
            auto error = result.error();
            Serial.printf("- %s: <%s>\n", magic_enum::enum_name(command).data(), magic_enum::enum_name(result.error()).data());

            if (result.error() == sfg_sbs::Error::BusOtherError)
            {
                // If the bus is in a bad state, try to reset it by sending a STOP condition.
                host_wire.begin();
                break;
            }
            continue;
        }

        Serial.printf("- %s:", magic_enum::enum_name(command).data());

        switch (traits.m_protocol)
        {
            case sfg_sbs::SmbusProtocol::Word:
            {
                Serial.printf(" 0x%04X", etl::get<sfg_sbs::Word>(result.value()).as_uint());

                switch (traits.m_data_type)
                {
                    case sfg_sbs::DataType::Int:
                    {
                        Serial.printf(", %d\n", etl::get<sfg_sbs::Word>(result.value()).as_int());
                        break;
                    }
                    case sfg_sbs::DataType::UnsignedInt:
                    case sfg_sbs::DataType::Word:
                    {
                        Serial.printf(", %u\n", etl::get<sfg_sbs::Word>(result.value()).as_uint());
                        break;
                    }
                    case sfg_sbs::DataType::Boolean:
                    {
                        Serial.printf(", %s\n", etl::get<sfg_sbs::Word>(result.value()).as_bool() ? "true" : "false");
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case sfg_sbs::SmbusProtocol::Block:
            {
                Serial.printf(" %s\n", etl::get<sfg_sbs::Block>(result.value()).as_string().data());
                break;
            }
        }
    }
    Serial.printf("\n\n\n");
    delay(3000);
}