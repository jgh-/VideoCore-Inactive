#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_IPP_20110819
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_IPP_20110819

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2011 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/client/connection/ssl_delegate.hpp>
#include <boost/bind.hpp>

boost::network::http::impl::ssl_delegate::ssl_delegate(asio::io_service & service,
                                         optional<std::string> certificate_filename,
                                         optional<std::string> verify_path) :
  service_(service),
  certificate_filename_(certificate_filename),
  verify_path_(verify_path) {}

void boost::network::http::impl::ssl_delegate::connect(
    asio::ip::tcp::endpoint & endpoint,
    function<void(system::error_code const &)> handler) {
  context_.reset(new asio::ssl::context(
      service_,
      asio::ssl::context::sslv23_client));
  if (certificate_filename_ || verify_path_) {
    context_->set_verify_mode(asio::ssl::context::verify_peer);
    if (certificate_filename_) context_->load_verify_file(*certificate_filename_);
    if (verify_path_) context_->add_verify_path(*verify_path_);
  } else {
    context_->set_verify_mode(asio::ssl::context::verify_none);
  }
  socket_.reset(new asio::ssl::stream<asio::ip::tcp::socket>(service_, *context_));
  socket_->lowest_layer().async_connect(
      endpoint,
      ::boost::bind(&boost::network::http::impl::ssl_delegate::handle_connected,
           boost::network::http::impl::ssl_delegate::shared_from_this(),
           asio::placeholders::error,
           handler));
}

void boost::network::http::impl::ssl_delegate::handle_connected(system::error_code const & ec,
                                           function<void(system::error_code const &)> handler) {
  if (!ec) {
    socket_->async_handshake(asio::ssl::stream_base::client, handler);
  } else {
    handler(ec);
  }
}

void boost::network::http::impl::ssl_delegate::write(
    asio::streambuf & command_streambuf,
    function<void(system::error_code const &, size_t)> handler) {
  asio::async_write(*socket_, command_streambuf, handler);
}

void boost::network::http::impl::ssl_delegate::read_some(
    asio::mutable_buffers_1 const & read_buffer,
    function<void(system::error_code const &, size_t)> handler) {
  socket_->async_read_some(read_buffer, handler);
}

boost::network::http::impl::ssl_delegate::~ssl_delegate() {}

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_SSL_DELEGATE_IPP_20110819 */
