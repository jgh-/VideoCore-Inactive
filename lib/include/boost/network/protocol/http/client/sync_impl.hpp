#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_SYNC_IMPL_HPP_20100623
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_SYNC_IMPL_HPP_20100623

// Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

namespace boost { namespace network { namespace http {

    template <class Tag, unsigned version_major, unsigned version_minor>
    struct basic_client_impl;

    namespace impl {
        template <class Tag, unsigned version_major, unsigned version_minor>
        struct sync_client  :
            connection_policy<Tag, version_major, version_minor>::type
        {
            typedef typename string<Tag>::type string_type;
            typedef typename connection_policy<Tag,version_major,version_minor>::type connection_base;
            typedef typename resolver<Tag>::type resolver_type;
            typedef function<void(iterator_range<char const *> const &, system::error_code const &)> body_callback_function_type;
            friend struct basic_client_impl<Tag,version_major,version_minor>;

            boost::shared_ptr<boost::asio::io_service> service_ptr;
            boost::asio::io_service & service_;
            resolver_type resolver_;
            optional<string_type> certificate_file, verify_path;

            sync_client(bool cache_resolved, bool follow_redirect
                , boost::shared_ptr<boost::asio::io_service> service
                , optional<string_type> const & certificate_file = optional<string_type>()
                , optional<string_type> const & verify_path = optional<string_type>()
            )
                : connection_base(cache_resolved, follow_redirect),
                service_ptr(service.get() ? service : make_shared<boost::asio::io_service>()),
                service_(*service_ptr),
                resolver_(service_)
                , certificate_file(certificate_file)
                , verify_path(verify_path)
            {}

            ~sync_client() {
                connection_base::cleanup();
                service_ptr.reset();
            }

            basic_response<Tag> const request_skeleton(basic_request<Tag> const & request_, string_type method, bool get_body, body_callback_function_type callback) {
                typename connection_base::connection_ptr connection_;
                connection_ = connection_base::get_connection(resolver_, request_, certificate_file, verify_path);
                return connection_->send_request(method, request_, get_body, callback);
            }

        };
        
    } // namespace impl

} // namespace http

} // namespace network

} // namespace boost

#endif // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_SYNC_IMPL_HPP_20100623
