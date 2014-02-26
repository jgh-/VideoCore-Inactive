#ifndef BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_STATUS_MESSAGE_HPP_20100603
#define BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPERS_STATUS_MESSAGE_HPP_20100603

// Copyright 2010 (c) Dean Michael Berris
// Copyright 2010 (c) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

namespace boost { namespace network { namespace http {

    template <class Tag>
    struct basic_response;

    namespace impl {

        template <class Tag>
        struct status_message_wrapper {

            typedef typename string<Tag>::type string_type;

            basic_response<Tag> const & response_;

            explicit status_message_wrapper(basic_response<Tag> const & response)
                : response_(response) {}

            status_message_wrapper(status_message_wrapper const & other)
                : response_(other.response_) {}

            operator string_type () {
                return response_.status_message();
            }

        };

    } // namespace impl

    template <class Tag>
    inline 
    impl::status_message_wrapper<Tag>
    status_message(basic_response<Tag> const & response) {
        return impl::status_message_wrapper<Tag>(response);
    }

} // namespace http

} // namespace network

} // namespace boost

#endif // BOOST_NETWORK_PROTOCOL_HTTP_MESSAGE_WRAPPER_STATUS_MESSAGE_HPP_20100603
