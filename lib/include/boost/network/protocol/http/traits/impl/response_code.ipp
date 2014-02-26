
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_CODE_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_CODE_IPP

#include <boost/network/tags.hpp>
#include <boost/cstdint.hpp>

namespace boost { namespace network { namespace http {

    /* This glob doesn't have a specialization on the tags::http_default_8bit_tcp_resolve
     * yet because it doesn't need to define different behaviour/types 
     * on different message tags -- for example, it doesn't need to
     * determine the type or change the values of the data no matter
     * what the tag type is provided.
     */
    template <class Tag>
        struct response_code {
            static boost::uint16_t const OK = 200u;
            static boost::uint16_t const CREATED = 201u;
            static boost::uint16_t const NO_CONTENT = 204u;
            static boost::uint16_t const UNAUTHORIZED = 401u;
            static boost::uint16_t const FORBIDDEN = 403u;
            static boost::uint16_t const NOT_FOUND = 404u;
            static boost::uint16_t const METHOD_NOT_ALLOWED = 405u;
            static boost::uint16_t const NOT_MODIFIED = 304u;
            static boost::uint16_t const BAD_REQUEST = 400u;
            static boost::uint16_t const SERVER_ERROR = 500u;
            static boost::uint16_t const NOT_IMPLEMENTED = 501u;
        };

} // namespace http

} // namespace network

} // namespace boost

#endif // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_CODE_IPP
