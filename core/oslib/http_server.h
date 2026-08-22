//
// Copyright (c) 2003-2025 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once
#include <asio.hpp>
#include <array>
#include <memory>
#include <set>
#include <string>
#include <tuple>
#include <vector>
#include <unordered_map>
#include <functional>

struct Header
{
	std::string name;
	std::string value;
};

/// A request received from a client.
struct Request
{
	std::string method;
	std::string uri;
	int http_version_minor;
	std::vector<Header> headers;
	std::string content;
};

/// A reply to be sent to a client.
struct Reply
{
	/// The status of the reply.
	enum status_type
	{
		ok = 200,
		created = 201,
		accepted = 202,
		no_content = 204,
		multiple_choices = 300,
		moved_permanently = 301,
		moved_temporarily = 302,
		not_modified = 304,
		bad_request = 400,
		unauthorized = 401,
		forbidden = 403,
		not_found = 404,
		method_not_allowed = 405,
		internal_server_error = 500,
		not_implemented = 501,
		bad_gateway = 502,
		service_unavailable = 503
	} status;

	/// The headers to be included in the reply.
	std::vector<Header> headers;

	/// The content to be sent in the reply.
	std::string content;

	/// Convert the reply into a vector of buffers. The buffers do not own the
	/// underlying memory blocks, therefore the reply object must remain valid and
	/// not be changed until the write operation has completed.
	std::vector<asio::const_buffer> toBuffers();

	/// Get a stock reply.
	static Reply stockReply(status_type status);

	/// Add a header to the reply.
	void addHeader(const std::string& name, const std::string& value) {
		headers.push_back({ name,  value });
	}

	/// Sets the reply content and content type
	void setContent(const std::string& content, const std::string& mimeType = "text/plain");

	/// Converts the reply into a string for logging purposes.
	std::string to_string();
};

/// The common handler for all incoming requests.
class RequestHandler
{
public:
	RequestHandler() = default;
	RequestHandler(const RequestHandler&) = delete;
	RequestHandler& operator=(const RequestHandler&) = delete;

	/// Handle a request and produce a reply.
	void handleRequest(const Request& req, Reply& rep);

	/// Adds a URL handler. The request URL must match the specified path.
	using HttpHandler = std::function<void(const Request&, Reply&)>;
	void addHandler(const std::string& path, HttpHandler handler) {
		handlers[path] = handler;
	}

private:
	std::unordered_map<std::string, HttpHandler> handlers;

	/// Perform URL-decoding on a string. Returns false if the encoding was
	/// invalid.
	static bool urlDecode(const std::string& in, std::string& out);
};

/// Parser for incoming requests.
class RequestParser
{
public:
	/// Construct ready to parse the request method.
	RequestParser() : state(method_start) {}

	/// Reset to initial parser state.
	void reset() {
		state = method_start;
		headLength = 0;
		contentLength = 0;
	}

	/// Result of parse.
	enum result_type { good, bad, indeterminate };

	/// Parse some data. The enum return value is good when a complete request has
	/// been parsed, bad if the data is invalid, indeterminate when more data is
	/// required. The InputIterator return value indicates how much of the input
	/// has been consumed.
	template <typename InputIterator>
	std::tuple<result_type, InputIterator> parse(Request& req,
			InputIterator begin, InputIterator end)
	{
		if (begin == end)
		{
			// end of transmission
			if (state == reading_content)
				return std::make_tuple(good, begin);
			else
				return std::make_tuple(bad, begin);
		}
		while (begin != end)
		{
			result_type result = consume(req, *begin++);
			if (result == good || result == bad)
				return std::make_tuple(result, begin);
		}
		return std::make_tuple(indeterminate, begin);
	}

private:
	/// Handle the next character of input.
	result_type consume(Request& req, char input);

	/// Check if a byte is an HTTP character.
	static bool isChar(int c)  {
		return c >= 0 && c <= 127;
	}

	/// Check if a byte is an HTTP control character.
	static bool isCtl(int c) {
		return (c >= 0 && c <= 31) || (c == 127);
	}

	/// Check if a byte is defined as an HTTP tspecial character.
	static bool isTSpecial(int c);

	/// Check if a byte is a digit.
	static bool isDigit(int c) {
		return c >= '0' && c <= '9';
	}

	/// The current state of the parser.
	enum State
	{
		method_start,
		method,
		uri,
		http_version_h,
		http_version_t_1,
		http_version_t_2,
		http_version_p,
		http_version_slash,
		http_version_major_start,
		http_version_major,
		http_version_minor_start,
		http_version_minor,
		expecting_newline_1,
		header_line_start,
		header_lws,
		header_name,
		space_before_header_value,
		header_value,
		expecting_newline_2,
		expecting_newline_3,
		reading_content
	} state;
	size_t headLength = 0;
	size_t contentLength = 0;

	static constexpr size_t MAX_HEAD_LEN = 2048;
	static constexpr size_t MAX_BODY_LEN = 4096;
};

class ConnectionManager;

/// Represents a single connection from a client.
class Connection
  : public std::enable_shared_from_this<Connection>
{
public:
	Connection(const Connection&) = delete;
	Connection& operator=(const Connection&) = delete;

	/// Construct a connection with the given socket.
	explicit Connection(asio::ip::tcp::socket socket,
		  ConnectionManager& manager, RequestHandler& handler);

	/// Start the first asynchronous operation for the connection.
	void start() {
		startTimer();
		doRead();
	}

	/// Stop all asynchronous operations associated with the connection.
	void stop() {
		socket.close();
	}

	using Ptr = std::shared_ptr<Connection>;

private:
	/// Perform an asynchronous read operation.
	void doRead();

	/// Perform an asynchronous write operation.
	void doWrite();

	void startTimer();

	/// Socket for the connection.
	asio::ip::tcp::socket socket;

	/// The manager for this connection.
	ConnectionManager& connectionManager;

	/// The handler used to process the incoming request.
	RequestHandler& requestHandler;

	/// Buffer for incoming data.
	std::array<char, 8192> buffer;

	/// The incoming request.
	Request request;

	/// The parser for the incoming request.
	RequestParser requestParser;

	/// The reply to be sent back to the client.
	Reply reply;

	bool keepAlive = false;

	asio::steady_timer timeoutTimer;
};

/// Manages open connections so that they may be cleanly stopped when the server
/// needs to shut down.
class ConnectionManager
{
public:
	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;

	/// Construct a connection manager.
	ConnectionManager() {}

	/// Add the specified connection to the manager and start it.
	void start(Connection::Ptr c);

	/// Stop the specified connection.
	void stop(Connection::Ptr c);

	/// Stop all connections.
	void stopAll();

private:
	/// The managed connections.
	std::set<Connection::Ptr> connections;
};

class HttpServer
{
public:
	HttpServer(const HttpServer&) = delete;
	HttpServer& operator=(const HttpServer&) = delete;

	/// Construct the server to listen on the specified TCP address and port.
	explicit HttpServer(asio::io_context& io_context, const std::string& address, uint16_t port);

	/// Adds a URI handler to the request handler. See RequestHandler::addHandler
	void addUriHandler(const std::string& path, RequestHandler::HttpHandler handler) {
		requestHandler.addHandler(path, handler);
	}

private:
	/// Perform an asynchronous accept operation.
	void doAccept();

	/// The io_context used to perform asynchronous operations.
	asio::io_context& io_context;

	/// Acceptor used to listen for incoming connections.
	asio::ip::tcp::acceptor acceptor;

	/// The connection manager which owns all live connections.
	ConnectionManager connectionManager;

	/// The handler for all incoming requests.
	RequestHandler requestHandler;
};
