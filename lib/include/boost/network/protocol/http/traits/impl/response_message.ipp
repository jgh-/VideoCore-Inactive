
//          Copyright Dean Michael Berris 2008.
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_MESSAGE_IPP
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_MESSAGE_IPP

#include <boost/network/tags.hpp>

namespace boost { namespace network { namespace http {

    template <>
        struct response_message<tags::http_default_8bit_tcp_resolve> {
            static char const * ok() {
                static char const * const OK = "OK";
                return OK;
            };

            static char const * created() {
                static char const * const CREATED = "Created";
                return CREATED;
            };

            static char const * no_content() {
                static char const * const NO_CONTENT = "NO Content";
                return NO_CONTENT;
            };

            static char const * unauthorized() {
                static char const * const UNAUTHORIZED = "Unauthorized";
                return UNAUTHORIZED;
            };

            static char const * forbidden() {
                static char const * const FORBIDDEN = "Fobidden";
                return FORBIDDEN;
            };

            static char const * not_found() {
                static char const * const NOT_FOUND = "Not Found";
                return NOT_FOUND;
            };

            static char const * method_not_allowed() {
                static char const * const METHOD_NOT_ALLOWED = "Method Not Allowed";
                return METHOD_NOT_ALLOWED;
            };

            static char const * not_modified() {
                static char const * const NOT_MODIFIED = "Not Modified";
                return NOT_MODIFIED;
            };

            static char const * bad_request() {
                static char const * const BAD_REQUEST = "Bad Request";
                return BAD_REQUEST;
            };

            static char const * server_error() {
                static char const * const SERVER_ERROR = "Server Error";
                return SERVER_ERROR;
            };

            static char const * not_implemented() {
                static char const * const NOT_IMPLEMENTED = "Not Implemented";
                return NOT_IMPLEMENTED;
            };

        };

} // namespace http

} // namespace network

} // namespace boost

#endif // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_TRAITS_RESPONSE_MESSAGE_IPP

