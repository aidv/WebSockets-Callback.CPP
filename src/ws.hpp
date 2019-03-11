#pragma once

#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <boost/asio.hpp>
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <vector>
#include <list>

#include <fstream>

#include <thread>
#include <mutex>

#include "src/classes.hpp"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


// Echoes back all received WebSocket messages
class server_session : public std::enable_shared_from_this<server_session>{
    websocket::stream<beast::tcp_stream> ws_;
    beast::multi_buffer buffer_;
public:
    WSCB_Options* options;
    uint64_t sessionID;
    std::vector<std::string>* outputMessageQueue;
    bool writing = false;
    bool waiting = false;
    
    // Take ownership of the socket
    explicit server_session(tcp::socket&& socket) : ws_(std::move(socket)){}
    
    // Start the asynchronous operation
    void run(WSCB_Options* options){
        sessionID = (uint64_t)std::time(nullptr);
        
        //cout << "\n" << "ws_session.run()";
        this->options = options;
        
        outputMessageQueue = new std::vector<std::string>();
        
        // Set suggested timeout settings for the websocket
        ws_.set_option(websocket::stream_base::suggested_settings(websocket::role_type::server));
        
        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
                                                         [](websocket::response_type& res){
                                                             res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
                                                         }
                                                         ));
        
        // Accept the websocket handshake
        ws_.async_accept(beast::bind_front_handler(&server_session::on_accept, shared_from_this()));
        
        
    }
    
    void on_accept(beast::error_code ec){
        options->callbacks.error.error = ec;
        
        
        //cout << "\n" << "on_accept";
        if(ec){
            options->callbacks.triggerOnError("accept");
            return ;//fail(ec, "accept");
        }
        
        sessionID = (uint64_t)std::time(nullptr);
        options->wsClientSessions->push_back(this);
        
        options->callbacks.triggerOnOpen(this);
        
        // Read a message
        this->reset();
    }
    
    void wait_for_read(){
        waiting = true;
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &server_session::on_read,
                shared_from_this()
            )
        );
    }
    
    void reset(){
        if (outputMessageQueue->size() > 0 && writing == false){
            std::vector<std::string> q = *outputMessageQueue;
            this->send(q[0]);
        }
        
        if (waiting == false) this->wait_for_read();
    }
    
    void on_read(beast::error_code ec, std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred);
        
        options->callbacks.error.error = ec;
        
        if(ec == websocket::error::closed){
            options->callbacks.triggerOnClose();
            
            for (int i = 0; i < options->wsClientSessions->size(); i++){
                std::vector<void*> sessions = *options->wsClientSessions;
                server_session* conn_ = ((server_session*)sessions[i]);
                if (conn_->sessionID == this->sessionID){
                    options->wsClientSessions->erase(options->wsClientSessions->begin() + i);
                    break;
                }
            }
            
            std::cout << "wsClientSessions->size(): " << options->wsClientSessions->size() << std::endl;
            
            
            return;
        }
        
        if(ec){
            options->callbacks.triggerOnError("read");
        }
        
        std::string* msg = new std::string(beast::buffers_to_string(buffer_.data()));
        if (msg->length() > 0) options->callbacks.triggerPreOnMessage(msg, this);
        
        buffer_.consume(buffer_.size());
        waiting = false;
        this->reset();
    }
    
    void on_write( beast::error_code ec, std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred);
        
        options->callbacks.error.error = ec;
        
        if(ec){
            options->callbacks.triggerOnError("write");
            return;//fail(ec, "write");
        }
        
        if (outputMessageQueue->size() > 0 && writing == true){
            outputMessageQueue->erase(outputMessageQueue->begin() + 0);
            std::vector<std::string> q = *outputMessageQueue;
            writing = false;
            if (outputMessageQueue->size() > 0) this->send(q[0]);
        } else {
            writing = false;
        }
    }
    
    
    void send(std::string msg){
        if (writing == true) return;
        writing = true;
        
        this->ws_.async_write(
            net::buffer(msg),
            beast::bind_front_handler(
                &server_session::on_write,
                shared_from_this()
            )
        );
    }
};

//------------------------------------------------------------------------------

// Accepts incoming connections and launches the sessions
class ws_listener : public std::enable_shared_from_this<ws_listener>
{
    net::io_context& ioc_;
    tcp::acceptor acceptor_;
    
    
public:
    WSCB_Options* options;
    
    
    ws_listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(ioc)
    {
        // Open the acceptor
        //cout << "\n" <<  "Open the acceptor";
        acceptor_.open(endpoint.protocol(), options->callbacks.error.error);
        if(options->callbacks.error.error){
            options->callbacks.triggerOnError("open");
            return;
        }
        
        
        // Allow address reuse
        //cout << "\n" << "Allow address reuse";
        acceptor_.set_option(net::socket_base::reuse_address(true), options->callbacks.error.error);
        if(options->callbacks.error.error){
            options->callbacks.triggerOnError("set_option");
            return;
        }
        
        // Bind to the server address
        //cout << "\n" <<  "Bind to the server address";
        acceptor_.bind(endpoint, options->callbacks.error.error);
        if(options->callbacks.error.error){
            options->callbacks.triggerOnError("bind");
            return;
        }
        
        // Start listening for connections
        //cout << "\n" <<  "Start listening for connections";
        acceptor_.listen(net::socket_base::max_listen_connections, options->callbacks.error.error);
        if(options->callbacks.error.error){
            options->callbacks.triggerOnError("listen");
            return;
        }
        options->callbacks.triggerOnListening();
    }
    
    
    
    // Start accepting incoming connections
    void run(WSCB_Options* options){
        
        this->options = options;
        
        //cout << "\n" << "%s " << "ws run...";
        
        if(! acceptor_.is_open()){
            //cout << "\n" <<  "already open!";
            options->callbacks.triggerOnError("already open");
            return;
        }
        do_accept();
    }
    
    void do_accept(){
        //cout << "\n" <<  "do_accept()";
        
        // The new connection gets its own strand
        acceptor_.async_accept(
                               beast::make_strand(ioc_),
                               beast::bind_front_handler(&ws_listener::on_accept, shared_from_this())
                               );
    }
    
    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        options->callbacks.error.error = ec;
        
        
        //cout << "\n" <<  "on_accept";
        if(ec)
        {
            //cout << "\n" <<  "on_accept: error";
           ;//fail(ec, "accept");
            options->callbacks.triggerOnError("accept");
        }
        else
        {
            //cout << "\n" <<  "on_accept: ok";
            // Create the session and run it
            std::make_shared<server_session>(std::move(socket))->run(options);
            
        }
        
        // Accept another connection
        do_accept();
    }
};



// Sends a WebSocket message and prints the response
class ws_client_session : public std::enable_shared_from_this<ws_client_session>
{
    tcp::resolver resolver_;
    websocket::stream<beast::tcp_stream> ws_;
    beast::multi_buffer buffer_;
public:
    WSCB_Options* options;
    std::vector<std::string>* outputMessageQueue;
    bool writing = false;
    bool waiting = false;
    
    // Resolver and socket require an io_context
    explicit
    ws_client_session(net::io_context& ioc)
    : resolver_(beast::make_strand(ioc))
    , ws_(beast::make_strand(ioc))
    {
    }
    
    
    
    // Start the asynchronous operation
    void run(WSCB_Options* options){
        this->options = options;
        outputMessageQueue = new std::vector<std::string>();
        options->clientSession = this;
        
        
        
        const char* host = options->address.c_str();
        const char* port = std::to_string(options->port).c_str();
        
        resolver_.async_resolve(
            host,
            port,
            beast::bind_front_handler(
                &ws_client_session::on_resolve,
                shared_from_this()
            )
        );
        
    }
    
    void on_resolve(beast::error_code ec, tcp::resolver::results_type results){
        //cout << "\n" <<  "======== on_resolve()";
        
        options->callbacks.error.error = ec;
        if(ec){
            options->callbacks.triggerOnError("resolve");
            return;//fail(ec, "resolve");
        }
        
        // Set the timeout for the operation
        beast::get_lowest_layer(ws_).expires_after(std::chrono::seconds(30));
        
        // Make the connection on the IP address we get from a lookup
        beast::get_lowest_layer(ws_).async_connect(
            results,
            beast::bind_front_handler(
                &ws_client_session::on_connect,
                                      shared_from_this()
            )
        );
    }
    
    void on_connect(beast::error_code ec, tcp::resolver::results_type::endpoint_type){
        //cout << "\n" <<  "======== on_connect()";
        
        if(ec){
            options->callbacks.triggerOnError("connect");
            return;//fail(ec, "connect");
        }
        
        // Turn off the timeout on the tcp_stream, because
        // the websocket stream has its own timeout system.
        beast::get_lowest_layer(ws_).expires_never();
        
        // Set suggested timeout settings for the websocket
        ws_.set_option(websocket::stream_base::suggested_settings(boost::beast::websocket::role_type::client));
        
        // Set a decorator to change the User-Agent of the handshake
        ws_.set_option(websocket::stream_base::decorator(
                                                         [](websocket::request_type& req)
                                                         {
                                                             req.set(http::field::user_agent,
                                                                     std::string(BOOST_BEAST_VERSION_STRING) +
                                                                     " websocket-client-async");
                                                         }));
        
        // Perform the websocket handshake
        ws_.async_handshake(options->address, "/", beast::bind_front_handler( &ws_client_session::on_handshake, shared_from_this()));
        
        
    }
    
    void on_handshake(beast::error_code ec){
        //cout << "\n" <<  "======== on_handshake()";
        
        options->callbacks.error.error = ec;
        
        if(ec){
            options->callbacks.triggerOnError("handshake");
            return;//fail(ec, "handshake");
        }
        
        options->callbacks.triggerOnOpen();
        
        this->reset();
    }
    
    
    
    
    void wait_for_read(){
        waiting = true;
        ws_.async_read(
            buffer_,
            beast::bind_front_handler(
                &ws_client_session::on_read,
                shared_from_this()
            )
        );
    }
    
    void reset(){
        if (outputMessageQueue->size() > 0 && writing == false){
            std::vector<std::string> q = *outputMessageQueue;
            this->send(q[0]);
        }
        
        if (waiting == false) this->wait_for_read();
    }
    
    void on_read(beast::error_code ec, std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred);
        
        options->callbacks.error.error = ec;
        
        if(ec){
            options->callbacks.triggerOnError("read");
            return;//fail(ec, "read");
        }
        
        std::string* msg = new std::string(beast::buffers_to_string(buffer_.data()));
        options->callbacks.triggerPreOnMessage(msg, NULL);
        
        buffer_.consume(buffer_.size());
        waiting = false;
        this->reset();
    }
    
    void on_close(beast::error_code ec){
        //cout << "\n" <<  "======= on_close()";
        options->callbacks.error.error = ec;
        if(ec){
            options->callbacks.triggerOnError("close");
            return;//fail(ec, "close");
        }
        
        options->callbacks.triggerOnClose();
    }
    
    
    void on_write(beast::error_code ec, std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred);
        
        options->callbacks.error.error = ec;
        
        if(ec){
            options->callbacks.triggerOnError("write");
            return;
        }
        
        if (outputMessageQueue->size() > 0 && writing == true){
            outputMessageQueue->erase(outputMessageQueue->begin() + 0);
            std::vector<std::string> q = *outputMessageQueue;
            writing = false;
            if (outputMessageQueue->size() > 0) this->send(q[0]);
        } else {
            writing = false;
        }
    }
    
    void send(std::string msg){
        if (writing == true) return;
        writing = true;
        
        this->ws_.async_write(
            net::buffer(msg),
            beast::bind_front_handler(
                &ws_client_session::on_write,
                shared_from_this()
            )
        );
    }
};
