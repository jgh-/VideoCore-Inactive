#ifndef SERVER_REQUEST_PARSERS_IMPL_UW3PM6V6
#define SERVER_REQUEST_PARSERS_IMPL_UW3PM6V6

#include <boost/spirit/include/qi.hpp>

// Copyright 2010 Dean Michael Berris. 
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/message/header.hpp>
#include <boost/fusion/tuple.hpp>

#ifdef BOOST_NETWORK_NO_LIB
#  ifndef BOOST_NETWORK_INLINE
#    define BOOST_NETWORK_INLINE inline
#  endif
#else
#  define BOOST_NETWORK_INLINE
#endif
#include <vector>

namespace boost { namespace network { namespace http {

    BOOST_NETWORK_INLINE void parse_version(std::string const & partial_parsed, fusion::tuple<uint8_t,uint8_t> & version_pair) {
        using namespace boost::spirit::qi;
        parse(
            partial_parsed.begin(), partial_parsed.end(),
            (
                lit("HTTP/")
                >> ushort_
                >> '.'
                >> ushort_
            )
            , version_pair);
    }

    BOOST_NETWORK_INLINE void parse_headers(std::string const & input, std::vector<request_header_narrow> & container) {
        using namespace boost::spirit::qi;
        parse(
            input.begin(), input.end(),
            *(
                +(alnum|(punct-':'))
                >> lit(": ")
                >> +((alnum|space|punct) - '\r' - '\n')
                >> lit("\r\n")
            )
            >> lit("\r\n")
            , container
            );
    }

} /* http */
    
} /* network */
    
} /* boost */

#endif /* SERVER_REQUEST_PARSERS_IMPL_UW3PM6V6 */
