#include "sfg_sbs/error.hpp"

namespace sfg_sbs
{
    Error error_from_bus_error(int bus_error)
    {
        switch (bus_error)
        {
            case 1:
                return Error::BusDataTooLong;
            case 2:
                return Error::BusAddressNack;
            case 3:
                return Error::BusDataNack;
            default:
                return Error::BusOtherError;
        }
    }
}