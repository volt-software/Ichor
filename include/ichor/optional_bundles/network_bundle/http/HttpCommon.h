#pragma once

#include <span>

namespace Ichor {
    // Copied/modified from Boost.BEAST
    enum class HttpMethod {
        /** An unknown method.

            This value indicates that the request method string is not
            one of the recognized verbs. Callers interested in the method
            should use an interface which returns the original string.
        */
        unknown = 0,

        /// The DELETE method deletes the specified resource
        delete_,

        /** The GET method requests a representation of the specified resource.

            Requests using GET should only retrieve data and should have no other effect.
        */
        get,

        /** The HEAD method asks for a response identical to that of a GET request, but without the response body.

            This is useful for retrieving meta-information written in response
            headers, without having to transport the entire content.
        */
        head,

        /** The POST method requests that the server accept the entity enclosed in the request as a new subordinate of the web resource identified by the URI.

            The data POSTed might be, for example, an annotation for existing
            resources; a message for a bulletin board, newsgroup, mailing list,
            or comment thread; a block of data that is the result of submitting
            a web form to a data-handling process; or an item to add to a database
        */
        post,

        /** The PUT method requests that the enclosed entity be stored under the supplied URI.

            If the URI refers to an already existing resource, it is modified;
            if the URI does not point to an existing resource, then the server
            can create the resource with that URI.
        */
        put,

        /** The CONNECT method converts the request connection to a transparent TCP/IP tunnel.

            This is usually to facilitate SSL-encrypted communication (HTTPS)
            through an unencrypted HTTP proxy.
        */
        connect,

        /** The OPTIONS method returns the HTTP methods that the server supports for the specified URL.

            This can be used to check the functionality of a web server by requesting
            '*' instead of a specific resource.
        */
        options,

        /** The TRACE method echoes the received request so that a client can see what (if any) changes or additions have been made by intermediate servers.
        */
        trace,

        // WebDAV

        copy,
        lock,
        mkcol,
        move,
        propfind,
        proppatch,
        search,
        unlock,
        bind,
        rebind,
        unbind,
        acl,

        // subversion

        report,
        mkactivity,
        checkout,
        merge,

        // upnp

        msearch,
        notify,
        subscribe,
        unsubscribe,

        // RFC-5789

        patch,
        purge,

        // CalDAV

        mkcalendar,

        // RFC-2068, section 19.6.1.2

        link,
        unlink
    };

    // Copied/modified from Boost.BEAST
    enum class HttpStatus
    {
        continue_                           = 100,
        switching_protocols                 = 101,

        processing                          = 102,

        ok                                  = 200,
        created                             = 201,
        accepted                            = 202,
        non_authoritative_information       = 203,
        no_content                          = 204,
        reset_content                       = 205,
        partial_content                     = 206,
        multi_status                        = 207,
        already_reported                    = 208,
        im_used                             = 226,

        multiple_choices                    = 300,
        moved_permanently                   = 301,
        found                               = 302,
        see_other                           = 303,
        not_modified                        = 304,
        use_proxy                           = 305,
        temporary_redirect                  = 307,
        permanent_redirect                  = 308,

        bad_request                         = 400,
        unauthorized                        = 401,
        payment_required                    = 402,
        forbidden                           = 403,
        not_found                           = 404,
        method_not_allowed                  = 405,
        not_acceptable                      = 406,
        proxy_authentication_required       = 407,
        request_timeout                     = 408,
        conflict                            = 409,
        gone                                = 410,
        length_required                     = 411,
        precondition_failed                 = 412,
        payload_too_large                   = 413,
        uri_too_long                        = 414,
        unsupported_media_type              = 415,
        range_not_satisfiable               = 416,
        expectation_failed                  = 417,
        misdirected_request                 = 421,
        unprocessable_entity                = 422,
        locked                              = 423,
        failed_dependency                   = 424,
        upgrade_required                    = 426,
        precondition_required               = 428,
        too_many_requests                   = 429,
        request_header_fields_too_large     = 431,
        connection_closed_without_response  = 444,
        unavailable_for_legal_reasons       = 451,
        client_closed_request               = 499,

        internal_server_error               = 500,
        not_implemented                     = 501,
        bad_gateway                         = 502,
        service_unavailable                 = 503,
        gateway_timeout                     = 504,
        http_version_not_supported          = 505,
        variant_also_negotiates             = 506,
        insufficient_storage                = 507,
        loop_detected                       = 508,
        not_extended                        = 510,
        network_authentication_required     = 511,
        network_connect_timeout_error       = 599
    };

    struct HttpHeader {
        std::string_view name;
        std::string_view value;

        HttpHeader() noexcept = default;
        HttpHeader(std::string_view _name, std::string_view _value) noexcept : name(_name), value(_value) {}
    };

    struct HttpRequest {
        std::vector<uint8_t> body;
        HttpMethod method;
        std::string_view route;
        std::vector<HttpHeader> headers;
    };

    struct HttpResponse {
        HttpStatus status;
        std::vector<uint8_t> body;
        std::vector<HttpHeader> headers;
    };
}