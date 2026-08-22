//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#include "http_server.h"
#include <fstream>
#include <iostream>
#include <strings.h>

namespace status_strings {

const std::string ok =
		"HTTP/1.1 200 OK\r\n";
const std::string created =
		"HTTP/1.1 201 Created\r\n";
const std::string accepted =
		"HTTP/1.1 202 Accepted\r\n";
const std::string no_content =
		"HTTP/1.1 204 No Content\r\n";
const std::string multiple_choices =
		"HTTP/1.1 300 Multiple Choices\r\n";
const std::string moved_permanently =
		"HTTP/1.1 301 Moved Permanently\r\n";
const std::string moved_temporarily =
		"HTTP/1.1 302 Moved Temporarily\r\n";
const std::string not_modified =
		"HTTP/1.1 304 Not Modified\r\n";
const std::string bad_request =
		"HTTP/1.1 400 Bad Request\r\n";
const std::string unauthorized =
		"HTTP/1.1 401 Unauthorized\r\n";
const std::string forbidden =
		"HTTP/1.1 403 Forbidden\r\n";
const std::string not_found =
		"HTTP/1.1 404 Not Found\r\n";
const std::string method_not_allowed =
		"HTTP/1.1 405 Method Not Allowed\r\n";
const std::string internal_server_error =
		"HTTP/1.1 500 Internal Server Error\r\n";
const std::string not_implemented =
		"HTTP/1.1 501 Not Implemented\r\n";
const std::string bad_gateway =
		"HTTP/1.1 502 Bad Gateway\r\n";
const std::string service_unavailable =
		"HTTP/1.1 503 Service Unavailable\r\n";

asio::const_buffer toBuffer(Reply::status_type status)
{
	switch (status)
	{
	case Reply::ok:
		return asio::buffer(ok);
	case Reply::created:
		return asio::buffer(created);
	case Reply::accepted:
		return asio::buffer(accepted);
	case Reply::no_content:
		return asio::buffer(no_content);
	case Reply::multiple_choices:
		return asio::buffer(multiple_choices);
	case Reply::moved_permanently:
		return asio::buffer(moved_permanently);
	case Reply::moved_temporarily:
		return asio::buffer(moved_temporarily);
	case Reply::not_modified:
		return asio::buffer(not_modified);
	case Reply::bad_request:
		return asio::buffer(bad_request);
	case Reply::unauthorized:
		return asio::buffer(unauthorized);
	case Reply::forbidden:
		return asio::buffer(forbidden);
	case Reply::not_found:
		return asio::buffer(not_found);
	case Reply::method_not_allowed:
		return asio::buffer(method_not_allowed);
	case Reply::internal_server_error:
		return asio::buffer(internal_server_error);
	case Reply::not_implemented:
		return asio::buffer(not_implemented);
	case Reply::bad_gateway:
		return asio::buffer(bad_gateway);
	case Reply::service_unavailable:
		return asio::buffer(service_unavailable);
	default:
		return asio::buffer(internal_server_error);
	}
}

} // namespace status_strings

namespace misc_strings {

const char name_value_separator[] = { ':', ' ' };
const char crlf[] = { '\r', '\n' };

} // namespace misc_strings

std::vector<asio::const_buffer> Reply::toBuffers()
{
	std::vector<asio::const_buffer> buffers;
	buffers.push_back(status_strings::toBuffer(status));
	for (std::size_t i = 0; i < headers.size(); ++i)
	{
		Header& h = headers[i];
		buffers.push_back(asio::buffer(h.name));
		buffers.push_back(asio::buffer(misc_strings::name_value_separator));
		buffers.push_back(asio::buffer(h.value));
		buffers.push_back(asio::buffer(misc_strings::crlf));
	}
	buffers.push_back(asio::buffer(misc_strings::crlf));
	buffers.push_back(asio::buffer(content));
	return buffers;
}

std::string Reply::to_string()
{
	std::string s;
	std::vector<asio::const_buffer> buffers = toBuffers();
	for (const asio::const_buffer& buffer : buffers)
	{
		size_t sz = asio::buffer_size(buffer);
		const char *p = asio::buffer_cast<const char*>(buffer);
		s += std::string(p, p + sz);
	}
	return s;
}

namespace stock_replies {

const char ok[] = "";
const char created[] =
		"<html>"
		"<head><title>Created</title></head>"
		"<body><h1>201 Created</h1></body>"
		"</html>";
const char accepted[] =
		"<html>"
		"<head><title>Accepted</title></head>"
		"<body><h1>202 Accepted</h1></body>"
		"</html>";
const char no_content[] =
		"<html>"
		"<head><title>No Content</title></head>"
		"<body><h1>204 Content</h1></body>"
		"</html>";
const char multiple_choices[] =
		"<html>"
		"<head><title>Multiple Choices</title></head>"
		"<body><h1>300 Multiple Choices</h1></body>"
		"</html>";
const char moved_permanently[] =
		"<html>"
		"<head><title>Moved Permanently</title></head>"
		"<body><h1>301 Moved Permanently</h1></body>"
		"</html>";
const char moved_temporarily[] =
		"<html>"
		"<head><title>Moved Temporarily</title></head>"
		"<body><h1>302 Moved Temporarily</h1></body>"
		"</html>";
const char not_modified[] =
		"<html>"
		"<head><title>Not Modified</title></head>"
		"<body><h1>304 Not Modified</h1></body>"
		"</html>";
const char bad_request[] =
		"<html>"
		"<head><title>Bad Request</title></head>"
		"<body><h1>400 Bad Request</h1></body>"
		"</html>";
const char unauthorized[] =
		"<html>"
		"<head><title>Unauthorized</title></head>"
		"<body><h1>401 Unauthorized</h1></body>"
		"</html>";
const char forbidden[] =
		"<html>"
		"<head><title>Forbidden</title></head>"
		"<body><h1>403 Forbidden</h1></body>"
		"</html>";
const char not_found[] =
		"<html>"
		"<head><title>Not Found</title></head>"
		"<body><h1>404 Not Found</h1></body>"
		"</html>";
const char method_not_allowed[] =
		"<html>"
		"<head><title>Method Not Allowed</title></head>"
		"<body><h1>405 Method Not Allowed</h1></body>"
		"</html>";
const char internal_server_error[] =
		"<html>"
		"<head><title>Internal Server Error</title></head>"
		"<body><h1>500 Internal Server Error</h1></body>"
		"</html>";
const char not_implemented[] =
		"<html>"
		"<head><title>Not Implemented</title></head>"
		"<body><h1>501 Not Implemented</h1></body>"
		"</html>";
const char bad_gateway[] =
		"<html>"
		"<head><title>Bad Gateway</title></head>"
		"<body><h1>502 Bad Gateway</h1></body>"
		"</html>";
const char service_unavailable[] =
		"<html>"
		"<head><title>Service Unavailable</title></head>"
		"<body><h1>503 Service Unavailable</h1></body>"
		"</html>";

std::string to_string(Reply::status_type status)
{
	switch (status)
	{
	case Reply::ok:
		return ok;
	case Reply::created:
		return created;
	case Reply::accepted:
		return accepted;
	case Reply::no_content:
		return no_content;
	case Reply::multiple_choices:
		return multiple_choices;
	case Reply::moved_permanently:
		return moved_permanently;
	case Reply::moved_temporarily:
		return moved_temporarily;
	case Reply::not_modified:
		return not_modified;
	case Reply::bad_request:
		return bad_request;
	case Reply::unauthorized:
		return unauthorized;
	case Reply::forbidden:
		return forbidden;
	case Reply::not_found:
		return not_found;
	case Reply::method_not_allowed:
		return method_not_allowed;
	case Reply::internal_server_error:
		return internal_server_error;
	case Reply::not_implemented:
		return not_implemented;
	case Reply::bad_gateway:
		return bad_gateway;
	case Reply::service_unavailable:
		return service_unavailable;
	default:
		return internal_server_error;
	}
}

} // namespace stock_replies

Reply Reply::stockReply(Reply::status_type status)
{
	Reply rep;
	rep.setContent(stock_replies::to_string(status), "text/html");
	rep.status = status;
	return rep;
}

void Reply::setContent(const std::string& content, const std::string& mimeType)
{
	this->content = content;
	addHeader("Content-Length", std::to_string(content.size()));
	addHeader("Content-Type", mimeType);
	status = ok;
}

void RequestHandler::handleRequest(const Request& req, Reply& rep)
{
	// Decode url to path.
	std::string request_path;
	if (!urlDecode(req.uri, request_path))
	{
		rep = Reply::stockReply(Reply::bad_request);
		return;
	}

	// Request path must be absolute and not contain "..".
	if (request_path.empty() || request_path[0] != '/'
			|| request_path.find("..") != std::string::npos)
	{
		rep = Reply::stockReply(Reply::bad_request);
		return;
	}

	// Call the corresponding handler
	std::size_t qm_pos = request_path.find('?');
	if (qm_pos == std::string::npos)
		qm_pos = request_path.length();
	// try perfect match first
	auto it = handlers.find(request_path.substr(0, qm_pos));
	if (it != handlers.end()) {
		it->second(req, rep);
		return;
	}
	// try prefix match
	for (const auto& [path, handler] : handlers)
	{
		if (request_path.substr(0, path.size()) == path) {
			handler(req, rep);
			return;
		}
	}

	rep = Reply::stockReply(Reply::not_found);
	return;
}

bool RequestHandler::urlDecode(const std::string& in, std::string& out)
{
	out.clear();
	out.reserve(in.size());
	for (std::size_t i = 0; i < in.size(); ++i)
	{
		if (in[i] == '%')
		{
			if (i + 3 <= in.size())
			{
				int value = 0;
				std::istringstream is(in.substr(i + 1, 2));
				if (is >> std::hex >> value)
				{
					out += static_cast<char>(value);
					i += 2;
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}
		else if (in[i] == '+')
		{
			out += ' ';
		}
		else
		{
			out += in[i];
		}
	}
	return true;
}

RequestParser::result_type RequestParser::consume(Request& req, char input)
{
	if (state != reading_content && ++headLength > MAX_HEAD_LEN)
		return bad;
	switch (state)
	{
	case method_start:
		if (!isChar(input) || isCtl(input) || isTSpecial(input))
		{
			return bad;
		}
		else
		{
			state = method;
			req.method.push_back(input);
			return indeterminate;
		}
	case method:
		if (input == ' ')
		{
			state = uri;
			return indeterminate;
		}
		else if (!isChar(input) || isCtl(input) || isTSpecial(input))
		{
			return bad;
		}
		else
		{
			req.method.push_back(input);
			return indeterminate;
		}
	case uri:
		if (input == ' ')
		{
			state = http_version_h;
			return indeterminate;
		}
		else if (isCtl(input))
		{
			return bad;
		}
		else
		{
			req.uri.push_back(input);
			return indeterminate;
		}
	case http_version_h:
		if (input == 'H')
		{
			state = http_version_t_1;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_t_1:
		if (input == 'T')
		{
			state = http_version_t_2;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_t_2:
		if (input == 'T')
		{
			state = http_version_p;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_p:
		if (input == 'P')
		{
			state = http_version_slash;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_slash:
		if (input == '/')
		{
			req.http_version_minor = 0;
			state = http_version_major_start;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_major_start:
		if (isDigit(input))
		{
			if (input != '1')
				return bad;
			state = http_version_major;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_major:
		if (input == '.')
		{
			state = http_version_minor_start;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_minor_start:
		if (isDigit(input))
		{
			req.http_version_minor = input - '0';
			state = http_version_minor;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case http_version_minor:
		if (input == '\r')
		{
			state = expecting_newline_1;
			return indeterminate;
		}
		else {
			return bad;
		}
	case expecting_newline_1:
		if (input == '\n')
		{
			state = header_line_start;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case header_line_start:
		if (input == '\r')
		{
			state = expecting_newline_3;
			return indeterminate;
		}
		else if (!req.headers.empty() && (input == ' ' || input == '\t'))
		{
			state = header_lws;
			return indeterminate;
		}
		else if (!isChar(input) || isCtl(input) || isTSpecial(input))
		{
			return bad;
		}
		else
		{
			req.headers.push_back(Header());
			req.headers.back().name.push_back(input);
			state = header_name;
			return indeterminate;
		}
	case header_lws:
		if (input == '\r')
		{
			state = expecting_newline_2;
			return indeterminate;
		}
		else if (input == ' ' || input == '\t')
		{
			return indeterminate;
		}
		else if (isCtl(input))
		{
			return bad;
		}
		else
		{
			state = header_value;
			req.headers.back().value.push_back(input);
			return indeterminate;
		}
	case header_name:
		if (input == ':')
		{
			state = space_before_header_value;
			return indeterminate;
		}
		else if (!isChar(input) || isCtl(input) || isTSpecial(input))
		{
			return bad;
		}
		else
		{
			req.headers.back().name.push_back(input);
			return indeterminate;
		}
	case space_before_header_value:
		if (input == ' ')
		{
			state = header_value;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case header_value:
		if (input == '\r')
		{
			state = expecting_newline_2;
			return indeterminate;
		}
		else if (isCtl(input))
		{
			return bad;
		}
		else
		{
			req.headers.back().value.push_back(input);
			return indeterminate;
		}
	case expecting_newline_2:
		if (input == '\n')
		{
			state = header_line_start;
			return indeterminate;
		}
		else
		{
			return bad;
		}
	case expecting_newline_3:
		if (input == '\n')
		{
			if (req.method == "POST")
			{
				state = reading_content;
				contentLength = 0;
				for (const Header& header : req.headers)
				{
					if (!strcasecmp(header.name.c_str(), "Content-Length"))
					{
						contentLength = std::atol(header.value.c_str());
						if (contentLength > MAX_BODY_LEN)
							return bad;
						break;
					}
				}

				return indeterminate;
			}
			else {
				return good;
			}
		}
		else {
			return bad;
		}
	case reading_content:
		req.content.push_back(input);
		if (req.content.size() > MAX_BODY_LEN)
			return bad;
		else if (req.content.size() == contentLength)
			return good;
		else
			return indeterminate;
	default:
		return bad;
	}
}

bool RequestParser::isTSpecial(int c)
{
	switch (c)
	{
	case '(': case ')': case '<': case '>': case '@':
	case ',': case ';': case ':': case '\\': case '"':
	case '/': case '[': case ']': case '?': case '=':
	case '{': case '}': case ' ': case '\t':
		return true;
	default:
		return false;
	}
}

Connection::Connection(asio::ip::tcp::socket socket,
		ConnectionManager& manager, RequestHandler& handler)
	: socket(std::move(socket)), connectionManager(manager), requestHandler(handler),
	  timeoutTimer(socket.get_executor())
{
}

void Connection::doRead()
{
	auto self(shared_from_this());
	socket.async_read_some(asio::buffer(buffer),
		[this, self](std::error_code ec, std::size_t bytes_transferred)
		{
			if (!ec)
			{
				RequestParser::result_type result;
				std::tie(result, std::ignore) = requestParser.parse(
						request, buffer.data(), buffer.data() + bytes_transferred);

				if (result == RequestParser::good)
				{
					requestHandler.handleRequest(request, reply);
					reply.addHeader("Connection", "close");
					doWrite();
				}
				else if (result == RequestParser::bad)
				{
					reply = Reply::stockReply(Reply::bad_request);
					reply.addHeader("Connection", "close");
					doWrite();
				}
				else {
					doRead();
				}
			}
			else if (ec != asio::error::operation_aborted) {
				connectionManager.stop(shared_from_this());
			}
		});
}

void Connection::doWrite()
{
	auto self(shared_from_this());
	asio::async_write(socket, reply.toBuffers(),
		[this, self](std::error_code ec, std::size_t)
		{
			if (!ec)
			{
				if (!keepAlive)
				{
					// Initiate graceful connection closure.
					std::error_code ignored_ec;
					socket.shutdown(asio::ip::tcp::socket::shutdown_both,
							ignored_ec);
				}
				else
				{
					keepAlive = false;
					request = {};
					reply = {};
					startTimer();
					doRead();
				}
			}

			if (ec != asio::error::operation_aborted)
				connectionManager.stop(shared_from_this());
		});
}

void Connection::startTimer()
{
	timeoutTimer.expires_after(asio::chrono::seconds(30));
	timeoutTimer.async_wait([self = shared_from_this()](const std::error_code& ec) {
		if (!ec)
		{
			std::error_code ignored;
			self->socket.shutdown(asio::ip::tcp::socket::shutdown_both, ignored);
			self->connectionManager.stop(self);
		}
	});

}

void ConnectionManager::start(Connection::Ptr c) {
	connections.insert(c);
	c->start();
}

void ConnectionManager::stop(Connection::Ptr c) {
	connections.erase(c);
	c->stop();
}

void ConnectionManager::stopAll()
{
	for (auto c: connections)
		c->stop();
	connections.clear();
}

HttpServer::HttpServer(asio::io_context& io_context, const std::string& address, uint16_t port)
  : io_context(io_context),
    acceptor(io_context),
    connectionManager(),
    requestHandler()
{
	// Open the acceptor with the option to reuse the address (i.e. SO_REUSEADDR).
	asio::ip::tcp::resolver resolver(io_context);
	asio::ip::tcp::endpoint endpoint = *resolver.resolve(address, std::to_string(port)).begin();
	acceptor.open(endpoint.protocol());
	acceptor.set_option(asio::ip::tcp::acceptor::reuse_address(true));
	acceptor.bind(endpoint);
	acceptor.listen();

	doAccept();
}

void HttpServer::doAccept()
{
	acceptor.async_accept(
		[this](std::error_code ec, asio::ip::tcp::socket socket)
		{
			// Check whether the server was stopped by a signal before this
			// completion handler had a chance to run.
			if (!acceptor.is_open())
			  return;

			if (!ec) {
			  connectionManager.start(std::make_shared<Connection>(
				  std::move(socket), connectionManager, requestHandler));
			}

			doAccept();
		});
}
