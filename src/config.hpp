#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>

namespace program
{
    namespace net = boost::asio;
    namespace beast = boost::beast;

    using error_code = boost::system::error_code;
}