#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_ASYNC_IMPL_HPP_20100623
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_ASYNC_IMPL_HPP_20100623

// Copyright Dean Michael Berris 2010.
// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/io_service.hpp>
#include <boost/asio/strand.hpp>
#include <boost/thread/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/bind.hpp>

namespace boost { namespace network { namespace http {

  template <class Tag, unsigned version_major, unsigned version_minor>
  struct basic_client_impl;

  namespace impl {
    template <class Tag, unsigned version_major, unsigned version_minor>
    struct async_client :
      connection_policy<Tag,version_major,version_minor>::type
    {
      typedef
        typename connection_policy<Tag,version_major,version_minor>::type
        connection_base;
      typedef
        typename resolver<Tag>::type
        resolver_type;
      typedef
        typename string<Tag>::type
        string_type;

      typedef
        function<void(boost::iterator_range<char const *> const &, system::error_code const &)>
        body_callback_function_type;

      async_client(bool cache_resolved, bool follow_redirect, boost::shared_ptr<boost::asio::io_service> service, optional<string_type> const & certificate_filename, optional<string_type> const & verify_path)
        : connection_base(cache_resolved, follow_redirect),
        service_ptr(service.get() ? service : boost::make_shared<boost::asio::io_service>()),
        service_(*service_ptr),
        resolver_(service_),
        sentinel_(new boost::asio::io_service::work(service_)),
        certificate_filename_(certificate_filename),
        verify_path_(verify_path)
      {
        connection_base::resolver_strand_.reset(new
          boost::asio::io_service::strand(service_));
        lifetime_thread_.reset(new boost::thread(
          boost::bind(
            &boost::asio::io_service::run,
            &service_
            )));
      }

      ~async_client() throw ()
      {
        sentinel_.reset();
        if (lifetime_thread_.get()) {
          lifetime_thread_->join();
          lifetime_thread_.reset();
        }
      }

      basic_response<Tag> const request_skeleton(
        basic_request<Tag> const & request_,
        string_type const & method,
        bool get_body,
        body_callback_function_type callback
        )
      {
        typename connection_base::connection_ptr connection_;
        connection_ = connection_base::get_connection(resolver_, request_, certificate_filename_, verify_path_);
        return connection_->send_request(method, request_, get_body, callback);
      }

      boost::shared_ptr<boost::asio::io_service> service_ptr;
      boost::asio::io_service & service_;
      resolver_type resolver_;
      boost::shared_ptr<boost::asio::io_service::work> sentinel_;
      boost::shared_ptr<boost::thread> lifetime_thread_;
      optional<string_type> certificate_filename_, verify_path_;
    };
  } // namespace impl

} // namespace http

} // namespace network

} // namespace boost

#endif // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_ASYNC_IMPL_HPP_20100623
