# HTTP/1.1 conformance of HttpHostService

What the server side of [RFC 9112](https://www.rfc-editor.org/rfc/rfc9112) (HTTP/1.1), [RFC 9110](https://www.rfc-editor.org/rfc/rfc9110) (HTTP semantics) and [RFC 9111](https://www.rfc-editor.org/rfc/rfc9111) (caching) require of an origin server, and how Ichor's own `Ichor::v1::HttpHostService` (`ichor/services/network/http/HttpHostService.h`) meets it. The Boost.BEAST `HttpHostService` is a separate implementation and not covered here.

Tests are the `TEST_CASE`s in [test/HttpHostRfcTests.cpp](../test/HttpHostRfcTests.cpp) (named here without the `HttpHostRfcTests: ` prefix), which inject the host's dependencies by hand, and [test/HttpHostSocketTests.cpp](../test/HttpHostSocketTests.cpp), which use real sockets.

Some requirements are on the content of a response, which only a route's handler knows. Those are marked *handler*: the host can't meet them on a handler's behalf, but the table says what it does do.

## RFC 9112: HTTP/1.1

| Section | Requirement | Level | Ichor | Test |
|---|---|---|---|---|
| 2.2 | Parse as octets, a superset of US-ASCII | MUST | Parsed as bytes, field values may contain obs-text | header fields |
| 2.2 | A bare CR is invalid or replaced by SP | MUST | Invalid: 400 | request line |
| 2.2 | A bare LF can be a line terminator | MAY | Not accepted: 400, CRLF only | request line |
| 2.2 | Ignore at least one empty line before the request line | SHOULD | All empty lines are ignored, up to MaxRequestLineSize | request line, unbounded input |
| 2.2 | Whitespace between the start line and the first field line: reject or ignore the line | MUST | Rejected: 400 | header fields |
| 2.2 | Respond to an invalid request with 400 and close | SHOULD | 400, the connection is closed | request line |
| 2.3 | HTTP-version is case-sensitive `HTTP/` DIGIT `.` DIGIT | MUST | Others are 400, major versions other than 1 are 505 | request line |
| 3 | Methods are case-sensitive | MUST | `get` is an unknown method: 501 | request line |
| 3 | A request line longer than the server wants: 414, or 501 for a method longer than any implemented | MUST / SHOULD | MaxRequestLineSize (8 KiB), 414 or 501 | request line, limits from properties, a method longer than any Ichor implements is 501 |
| 3 | Single SP separators, no whitespace in the request target | MUST | Anything else is 400 | request line, request target |
| 3.2 | An invalid request target is 400 | SHOULD | 400, characters and percent-encoding are checked | request target |
| 3.2 | HTTP/1.1 requests without Host, or with more than one or an invalid one, are 400 | MUST | 400, IP literals are checked to be IPv6 addresses or IPvFuture | header fields, IP literals |
| 3.2.2 | Accept absolute-form, its authority replaces Host | MUST | Turned into origin-form, Host replaced | request target |
| 3.2.3 | authority-form is for CONNECT only | MUST | Other combinations are 400 | request target |
| 3.2.4 | asterisk-form is for OPTIONS only | MUST | `OPTIONS *` lists the server's methods, other methods with `*` are 400 | request target, routing |
| 3.2.4 | absolute-form OPTIONS with an empty path and no query is `OPTIONS *` | MUST (proxies) | Handled as `OPTIONS *` | absolute-form OPTIONS without a path is OPTIONS * |
| 4 | Status line, reason phrase can be empty | MUST | `HTTP/1.1 NNN reason`, empty reason for unknown codes | responses |
| 5.1 | No whitespace between a field name and the colon: 400 | MUST | 400 | header fields |
| 5.2 | obs-fold: reject with 400 or replace with SP | MUST | Rejected: 400 | header fields |
| 6.1 | Transfer-Encoding in an HTTP/1.0 request is faulty framing, close after responding | MUST | 400, closed | message body |
| 6.1 | chunked is the final transfer coding, or the request is 400 and the connection closed | MUST | 400, closed. So is `chunked` with parameters, which it doesn't have, or a coding that isn't a token | message body, transfer coding parameters and syntax |
| 6.1 | Don't send Transfer-Encoding to HTTP/1.0 clients, in 1xx, 204 or 2xx to CONNECT | MUST | Never: close-delimited for HTTP/1.0 | streaming responses, a 2xx to CONNECT has no framing and closes |
| 6.2 | Don't send Content-Length in 1xx, 204, 2xx to CONNECT | MUST | Never | responses, upgrade |
| 6.3 | Message length: Transfer-Encoding over Content-Length, both is an error to close after | MUST | Both is 400 and closed | message body |
| 6.3 | An invalid Content-Length is 400 and closed | MUST | 400, closed. A list of identical values is rejected too | invalid Content-Length, message body |
| 6.3 | No Content-Length or Transfer-Encoding in a request: no content | MUST | No content | message body |
| 7.1 | Decode chunked, chunk extensions are ignored | MUST | Decoded, extensions validated and ignored, chunk-size overflow is 400. The framing can be at most the content plus MaxHeaderSize, more is 400 | message body, invalid chunked bodies, chunk framing, unbounded input |
| 7.1.2 | Trailer fields can be discarded | MAY | Validated and discarded | message body |
| 7.2 | TE: transfer codings the client accepts in the response | MAY | Ignored: responses are chunked or not regardless | - |
| 7.3 | A transfer coding the server doesn't understand is 501 | SHOULD | 501, closed | message body |
| 8 | Incomplete messages | | Wait for the rest, until the request timeout: 408. What was received isn't parsed again for every part that follows | timeouts, a request received in parts is parsed once |
| 9.3 | HTTP/1.1 connections are persistent unless `close`, HTTP/1.0 ones are not unless `keep-alive` | MUST | Yes, `Connection: keep-alive` in the response to HTTP/1.0 | persistence |
| 9.3.2 | Responses to pipelined requests in the order they came | MUST | Requests are handled one at a time, in order | pipelining |
| 9.5 | Timeouts, close idle connections gracefully | MAY / SHOULD | IdleTimeoutMs and RequestTimeoutMs, with an ITimerFactory | timeouts, HttpHostSocketTests |
| 9.6 | After a response with `close`, close the connection and process no further requests | MUST | Closed, what follows is dropped | persistence |
| 9.6 | Close in stages: the sending side first, read until the client closes, then close | (described) | Done with an ITimerFactory, up to LingerTimeoutMs. Lingering connections are closed right away when the ITimerFactory goes away | graceful close, lingering connections are closed when the timer factory goes away, closing an upgraded connection in the handler lingers, HttpHostSocketTests |
| 11.1 | Response splitting: a handler can't add CR or LF to a field | (security) | A handler's invalid field is a 500 | responses |
| 11.2 | Request smuggling | (security) | The framing ambiguities above are 400 and close the connection | message body, header fields |

## RFC 9110: HTTP semantics

| Section | Requirement | Level | Ichor | Test |
|---|---|---|---|---|
| 2.5 | Respond with the highest version conformant to, of the request's major version | SHOULD | Always HTTP/1.1, later 1.x minor versions are handled as 1.1 | request line |
| 5.1 | Field names are case-insensitive | MUST | `HttpRequest::headers` ignores case | header fields |
| 5.3 | Field lines with the same name are one list | MUST | Combined with `, ` (`; ` for Cookie) | header fields |
| 5.3 | Don't send a field twice | MUST | A handler's response with names that only differ in case, or `contentType` and a Content-Type field, is a 500 | a handler's response with the same field twice is 500 |
| 5.4 | A field section larger than the server wants is a 4xx | MUST | MaxHeaderSize (64 KiB): 431 | request line, limits from properties |
| 5.5 | Field values with CR, LF or NUL: reject or replace | MUST | Rejected: 400, as are other control characters | header fields |
| 5.6.1.2 | Accept empty list elements | MUST | Ignored in Connection, Transfer-Encoding, Expect, Upgrade, If-Match and If-None-Match | Expect is a list, transfer coding parameters and syntax |
| 5.6.7 | Accept all three HTTP-date formats | MUST | IMF-fixdate, RFC 850 (two-digit years as the RFC says) and asctime | HTTP-date, conditional requests |
| 6.6.1 | An origin server with a clock sends Date | MUST | Every response, unless the handler sets one | responses |
| 7.2 | Host, see RFC 9112 3.2 | MUST | | header fields |
| 7.4 | A request for an https resource over a connection not secured for it is rejected | MUST | 421 for an absolute-form https target: the host's connections are plain TCP | an https target over a plain connection is 421 |
| 7.4 | Requests for another authority can be 421 | MAY | Not done: the host doesn't know its names | - |
| 7.6.1 | Connection options | MUST | `close`, `keep-alive` and `upgrade` are acted on. A handler's Connection field is replaced by the host's, after its `close` is taken into account | persistence, upgrade |
| 7.8 | Upgrade: ignore it in HTTP/1.0, switch only to a protocol the client asked for, send Upgrade and Connection: upgrade with 101 | MUST | Streaming routes can switch, anything else is 500 | upgrade, HttpHostSocketTests |
| 7.8 | A sender of Upgrade also sends `Connection: upgrade` | MUST | Added to every response with an Upgrade field, e.g. a 426 | Upgrade in a response is a connection option |
| 7.8 | Upgrade with Expect: 100-continue: 100 before the 101 | MUST | Yes | 100 Continue before 101 |
| 8.3 | Content-Type with content | SHOULD | *Handler*: `HttpResponse::contentType` | - |
| 8.4 | Content codings | MAY | Not done: responses are identity, which is always acceptable unless a client rules it out; request content codings are the handler's | - |
| 8.6 | Content-Length: not in 1xx, 204, 2xx to CONNECT; 304 and HEAD can have the GET length | MUST | Set by the host, a HEAD or 304 route can set it | responses, routing |
| 8.8 | Validators (ETag, Last-Modified) | SHOULD | *Handler*, used by the conditional requests below | conditional requests |
| 9.1 | An unrecognised method is 501 | SHOULD | 501, the connection stays open | request line |
| 9.1 | A method the resource doesn't allow is 405 | SHOULD | 405 when another method has a route for it | routing |
| 9.1 | GET and HEAD are supported | MUST | HEAD uses the GET route | routing |
| 9.3.2 | HEAD: the header fields of GET, no content | SHOULD | The GET route's response without content | routing |
| 9.3.6 | CONNECT to an empty or invalid port is 400 | MUST | Ports outside 1-65535 are 400 | CONNECT to an invalid port is 400 |
| 9.3.6 | CONNECT: 2xx starts a tunnel without Content-Length or Transfer-Encoding | MUST | Streaming routes can take the tunnel over, other routes' 2xx closes the connection | upgrade, a 2xx to CONNECT has no framing and closes |
| 9.3.7 | OPTIONS: Allow, Content-Length: 0 without content | SHOULD / MUST | Without an OPTIONS route | routing |
| 9.3.8 | TRACE: reflect the request without sensitive fields | SHOULD | Off by default (EnableTrace), as reflecting requests is a known risk | TRACE |
| 10.1.1 | Expect: 100-continue is ignored in HTTP/1.0, otherwise 100 or a final response before reading content, other expectations are 417 | MUST / SHOULD | Yes | Expect, a rejected request doesn't get a 100 Continue |
| 10.2.1 | Allow lists the methods of a resource | | Generated by the host | routing |
| 10.2.4 | Server | MAY | Off by default (SendServerHeader), it tells an attacker what to target | SendServerHeader, responses |
| 11.6.1, 11.7.1 | 401 has WWW-Authenticate, 407 has Proxy-Authenticate | MUST | *Handler*: the host logs an error when it's missing | required fields of a handler's response |
| 12 | Content negotiation | MAY | *Handler* | - |
| 13 | Conditional requests: evaluated in the order of 13.2.2, If-Match compares strongly, If-None-Match weakly, invalid dates are ignored | MUST | The host evaluates them for GET and HEAD routes against the response's ETag and Last-Modified; routes for other methods have to evaluate them before changing anything | conditional requests |
| 13.1.5 | If-Range without Range is ignored | MUST | Range isn't supported, so it is | - |
| 14 | Range requests | MAY | Not done: Range is ignored and the whole content sent, which is allowed | - |
| 15.2 | No 1xx to HTTP/1.0 clients | MUST | 100 Continue and 101 only to HTTP/1.1 | Expect, upgrade |
| 15.3.5 | 204 has no content | MUST | Content and Content-Length are dropped | responses, streaming responses |
| 15.3.6 | 205 has no content | MUST | Content dropped, Content-Length: 0 | 205 has no content |
| 15.4.5 | 304 has no content, keeps the fields a 200 would have | MUST | Content and Content-Type dropped, as is a Content-Length the handler set for content it sent | responses, conditional requests, a 304 doesn't pass on the Content-Length a handler set for its content |
| 15.5, 15.6 | Error responses explain the error and whether it's temporary, a 505 what versions are supported | SHOULD | The errors the host responds with have a text/plain explanation, the content of a handler's error responses is the handler's | the errors the host responds with explain themselves |
| 15.5.6 | 405 has Allow | MUST | Added by the host when the handler's 405 has none | routing, required fields of a handler's response |
| 15.5.9 | 408: close the connection | SHOULD | Closed after the 408 | timeouts, HttpHostSocketTests |
| 15.5.14 | 413: can close the connection | MAY | Closed: the content that's still coming can't be framed | limits from properties |
| 15.5.22 | 426 has Upgrade | MUST | *Handler*: the host logs an error when it's missing | - |

## RFC 9111: caching

An origin server's part in caching is the fields it sends (Cache-Control, Expires, ETag, Last-Modified, Vary), which are the handler's, and answering conditional requests, see RFC 9110 13 above. RFC 9111 has no requirements on origin servers beyond that.

## How this was checked

- Every sentence with a MUST or MUST NOT in RFC 9112 (84), RFC 9110 (200) and RFC 9111 (59) was taken from the RFC text and checked against the
  implementation, leaving out those only for clients, proxies, caches and registrations. That found four requirements that weren't met (duplicate
  fields, empty list elements in Expect, https targets, CONNECT ports), which have their rows and tests now. The same for every SHOULD in RFC 9112 (18)
  and RFC 9110 (88) found that the errors the host responded with had no explanation. The requirements left to a handler are marked *handler* in the
  tables, the deliberate deviations are TRACE being off by default and the unsupported features below.
- curl and Python's http.client, independent HTTP/1.1 implementations, against a running HttpHostService: GET, HEAD, a 2 MB chunked upload, Expect:
  100-continue, chunked and close-delimited streaming, conditional requests, connection reuse, OPTIONS * and 405. Raw requests for pipelining and
  the usual request smuggling variants (Content-Length with Transfer-Encoding, obfuscated or repeated Transfer-Encoding, obs-fold, whitespace before
  the colon, two Content-Lengths, a signed Content-Length, chunk-size overflow), a bare LF, a missing Host, HTTP/2.0, a header section over the
  limit and a body that never completes (408).

## Not supported

All optional in the RFCs:

* content codings (compression) of responses,
* range requests,
* 421 Misdirected Request,
* proactive content negotiation,
* response trailers.
