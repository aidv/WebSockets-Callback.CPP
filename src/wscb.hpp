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

#include "rubberarray/RubberArray.h"

/*#include "rapidjson/document.h"
#include "rapidjson/writer.h"
#include "rapidjson/stringbuffer.h"*/

//#include "../../libs/libs/lockfree/include/boost/lockfree/spsc_queue.hpp"

#include <nlohmann/json.hpp>

// for convenience
using json = nlohmann::json;


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>
//using namespace rapidjson;



// Report a failure
/*void;//fail(beast::error_code ec, char const* what){
    std::cerr << what << ": " << ec.message() << "\n";
    DLOG_F(INFO, "%s %s %s %s ", "WS Fail: ", what, ": ", ec.message().c_str());
}*/


struct thread_guard{
    std::string strToSend;
    bool strChanged = false;
    bool strSent = false;
};

struct WebSockets_Callback_Error {
    std::string message = "";
    beast::error_code error;
};

struct WebSockets_Callback_onEvents {
    WebSockets_Callback_Error error;
    std::function<void()> onListening = NULL ;
    std::function<void()> onOpen;
    
    std::function<void(const std::string, const void* conn)> preOnMessage;
    std::function<void(const std::string)> onResponselessMessage;
    std::function<void(const std::string)> onMessage;
    
    std::function<void(WebSockets_Callback_Error)> onError;
    std::function<void()> onClose;
    
    void triggerOnOpen(){ if (this->onOpen != 0) this->onOpen(); }
    
    void triggerOnListening(){ if (this->onListening != 0) this->onListening(); }
    
    void triggerPreOnMessage(std::string msg, void* conn){ if (this->preOnMessage != 0) this->preOnMessage(msg, conn); }
    void triggerOnResponselessMessage(std::string msg){ if (this->onResponselessMessage != 0) this->onResponselessMessage(msg); }
    
    
    void triggerOnError(){if (this->onError != 0) this->onError(this->error); }
    
    void triggerOnError(std::string msg){
        this->error.message = msg;
        triggerOnError();
    }
    
    void triggerOnClose(){ this->onClose(); }
    
};

struct WebSockets_Callback_Options {
    bool server = true;
    std::string address = "127.0.0.1";
    int port = 8081;
    int threads = 1;
    std::list<int> *MIDIEvents;
    WebSockets_Callback_onEvents callbacks;
};





struct WebSockets_Callback_Message {
    std::string cmd = "";
    std::string puid = "";
};


struct WebSockets_Callback_Trigger{
    std::string puid;
    std::string command;
    std::function<void(const std::string, std::function<void(const std::string)>)> doHandle;
};




// Echoes back all received WebSocket messages
class ws_session : public std::enable_shared_from_this<ws_session>{
    websocket::stream<beast::tcp_stream> ws_;
    beast::multi_buffer buffer_;
public:
    WebSockets_Callback_Options options;
    
    // Take ownership of the socket
    explicit ws_session(tcp::socket&& socket) : ws_(std::move(socket)){}
    
    // Start the asynchronous operation
    void run(WebSockets_Callback_Options options){
        cout << "%s" << "ws_session.run()";
        this->options = options;
        
        // Set suggested timeout settings for the websocket
        ws_.set_option(websocket::stream_base::suggested_settings(websocket::role_type::server));
        
        // Set a decorator to change the Server of the handshake
        ws_.set_option(websocket::stream_base::decorator(
                                                         [](websocket::response_type& res){
                                                             res.set(http::field::server, std::string(BOOST_BEAST_VERSION_STRING) + " websocket-server-async");
                                                         }
                                                         ));
        
        // Accept the websocket handshake
        ws_.async_accept(beast::bind_front_handler(&ws_session::on_accept, shared_from_this()));
        
    }
    
    void on_accept(beast::error_code ec){
        options.callbacks.error.error = ec;
        
        
        cout << "%s" << "on_accept";
        if(ec){
            options.callbacks.triggerOnError("accept");
            return ;//fail(ec, "accept");
        }
        
        options.callbacks.triggerOnOpen();
        // Read a message
        do_read();
    }
    
    void do_read(){
        // Read a message into our buffer
        cout << "%s" << "Reading message...";
        ws_.async_read(buffer_, beast::bind_front_handler( &ws_session::on_read, shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred);
        
        options.callbacks.error.error = ec;
        
        // This indicates that the session was closed
        if(ec == websocket::error::closed){
            options.callbacks.triggerOnClose();
            return;
        }
        
        if(ec){
           ;//fail(ec, "read");
            options.callbacks.triggerOnError("read");
        }
        
        // Echo the message
        ws_.text(ws_.got_text());
        //ws_.async_write(buffer_.data(), beast::bind_front_handler(&ws_session::on_write, shared_from_this()));
        
        std::string msg = beast::buffers_to_string(buffer_.data());
        
        options.callbacks.triggerPreOnMessage(msg, this);
    }
    
    void on_write( beast::error_code ec, std::size_t bytes_transferred){
        boost::ignore_unused(bytes_transferred);
        
        options.callbacks.error.error = ec;
        
        if(ec){
            options.callbacks.triggerOnError("write");
            return;//fail(ec, "write");
        }
        
        // Clear the buffer
        buffer_.consume(buffer_.size());
        
        // Do another read
        do_read();
    }
    
    
    void send_str(std::string str){
        
        cout << "[WS] Sending string: " << str.c_str();
        
        this->ws_.async_write(
                              net::buffer(str),
                              beast::bind_front_handler(
                                                        &ws_session::on_write,
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
    WebSockets_Callback_Options options;
    
    
    ws_listener(net::io_context& ioc, tcp::endpoint endpoint) : ioc_(ioc), acceptor_(ioc)
    {
        // Open the acceptor
        cout << "%s" << "Open the acceptor";
        acceptor_.open(endpoint.protocol(), options.callbacks.error.error);
        if(options.callbacks.error.error){
            options.callbacks.triggerOnError("open");
            return;
        }
        
        
        // Allow address reuse
        cout << "%s" <<"Allow address reuse";
        acceptor_.set_option(net::socket_base::reuse_address(true), options.callbacks.error.error);
        if(options.callbacks.error.error){
            options.callbacks.triggerOnError("set_option");
            return;
        }
        
        // Bind to the server address
        cout << "%s" << "Bind to the server address";
        acceptor_.bind(endpoint, options.callbacks.error.error);
        if(options.callbacks.error.error){
            options.callbacks.triggerOnError("bind");
            return;
        }
        
        // Start listening for connections
        cout << "%s" << "Start listening for connections";
        acceptor_.listen(net::socket_base::max_listen_connections, options.callbacks.error.error);
        if(options.callbacks.error.error){
            options.callbacks.triggerOnError("listen");
            return;
        }
        options.callbacks.triggerOnListening();
    }
    
    
    
    // Start accepting incoming connections
    void run(WebSockets_Callback_Options options){
        
        this->options = options;
        
        cout << "%s " << "ws run...";
        
        if(! acceptor_.is_open()){
            cout << "%s" << "already open!";
            options.callbacks.triggerOnError("already open");
            return;
        }
        do_accept();
    }
    
    void do_accept(){
        cout << "%s" << "do_accept()";
        
        // The new connection gets its own strand
        acceptor_.async_accept(
                               beast::make_strand(ioc_),
                               beast::bind_front_handler(&ws_listener::on_accept, shared_from_this())
                               );
    }
    
    void on_accept(beast::error_code ec, tcp::socket socket)
    {
        options.callbacks.error.error = ec;
        
        
        cout << "%s" << "on_accept";
        if(ec)
        {
            cout << "%s" << "on_accept: error";
           ;//fail(ec, "accept");
            options.callbacks.triggerOnError("accept");
        }
        else
        {
            cout << "%s" << "on_accept: ok";
            // Create the session and run it
            std::make_shared<ws_session>(std::move(socket))->run(options);
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
    WebSockets_Callback_Options options;
    
    // Resolver and socket require an io_context
    explicit
    ws_client_session(net::io_context& ioc)
    : resolver_(beast::make_strand(ioc))
    , ws_(beast::make_strand(ioc))
    {
    }
    
    
    
    // Start the asynchronous operation
    void run(WebSockets_Callback_Options options){
        this->options = options;
        
        
        const char* host = options.address.c_str();
        const char* port = std::to_string(options.port).c_str();
        
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
        cout << "%s" << "======== on_resolve()";
        
        options.callbacks.error.error = ec;
        if(ec){
            options.callbacks.triggerOnError("resolve");
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
        cout << "%s" << "======== on_connect()";
        
        if(ec){
            options.callbacks.triggerOnError("connect");
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
        ws_.async_handshake(options.address, "/", beast::bind_front_handler( &ws_client_session::on_handshake, shared_from_this()));
        
        
    }
    
    void on_handshake(beast::error_code ec){
        cout << "%s" << "======== on_handshake()";
        
        options.callbacks.error.error = ec;
        
        if(ec){
            options.callbacks.triggerOnError("handshake");
            return;//fail(ec, "handshake");
        }
        
        options.callbacks.onOpen();
        
        this->do_read();
    }
    
    void on_write(beast::error_code ec, std::size_t bytes_transferred){
        cout << "%s" << "======== on_write()";
        
        boost::ignore_unused(bytes_transferred);
        
        options.callbacks.error.error = ec;
        if(ec){
            options.callbacks.triggerOnError("write");
            return;//fail(ec, "write");
        }
        
        
    }
    
    
    void do_read(){
        
        
        cout << "%s" << "Reading message...";
        ws_.async_read(
                      buffer_,
                      beast::bind_front_handler(
                                                &ws_client_session::on_read,
                                                shared_from_this()));
    }
    
    void on_read(beast::error_code ec, std::size_t bytes_transferred){
        cout << "%s" << "======= on_read()";
        
        
        boost::ignore_unused(bytes_transferred);
        
        options.callbacks.error.error = ec;
        if(ec){
            options.callbacks.triggerOnError("read");
            return;//fail(ec, "read");
        }
        
        cout << "%s" << "Message receveived! Calling preOnMessage()...";
        std::string msg = beast::buffers_to_string(buffer_.data());
        options.callbacks.triggerPreOnMessage(msg, this);
        
        
        
        cout << "%s" << "Clearing buffer...";
        buffer_.consume(buffer_.size());
        this->do_read();
    }
    
    void on_close(beast::error_code ec){
        cout << "%s" << "======= on_close()";
        options.callbacks.error.error = ec;
        if(ec){
            options.callbacks.triggerOnError("close");
            return;//fail(ec, "close");
        }
        
        options.callbacks.triggerOnClose();
    }
    
    void send_str(std::string str){
        cout << "%s %s" << "[WS CLIENT] Sending: ", str.c_str();
        
        this->ws_.async_write(
            net::buffer(str),
            beast::bind_front_handler(
                &ws_client_session::on_write,
                shared_from_this()
            )
        );
    }
};





class WebSockets_Callback_Triggers{
    RubberArray<WebSockets_Callback_Trigger*> triggers;
    
public:
    void add(std::string cmd, std::function<void(const std::string, std::function<void(const std::string)>)> doHandle = NULL){
    
        WebSockets_Callback_Trigger* trigger;
        trigger = new WebSockets_Callback_Trigger();
        trigger->command = cmd;
        
        
        if (doHandle != 0){
            
            //get current time
            //trigger->puid = new std::string(//type expector + datetime);
            
            trigger->doHandle = doHandle;
        }
        
        
        triggers.append(trigger);
    }
    
    int indexOf(std::string id){
        return 0;
    }
};

class WebSockets_Callback {
    std::thread thread;
    std::condition_variable cv;
    WebSockets_Callback_Triggers triggers;
public:
    WebSockets_Callback_Options options;
    thread_guard* tg;
    
    std::mutex m;
private:
    std::string ws_types [2]; //0 = "#s", 1 = "#c"
    
    void server_thread(WebSockets_Callback_Options options){
        cout << "%s" << "WebSockets_Callback_Options::server_thread()";
        
        net::io_context ioc{options.threads};
        
        // Create and launch a listening port
        std::make_shared<ws_listener>(ioc, boost::asio::ip::tcp::endpoint{
            net::ip::make_address(options.address.c_str()),
            static_cast<unsigned short>(options.port)
        })->run(options);
        
        // Run the I/O service on the requested number of threads
        std::vector<std::thread> v;
        v.reserve(options.threads - 1);
        
        for(auto i = options.threads - 1; i > 0; --i)
            v.emplace_back([&ioc]{ioc.run();});
        
        ioc.run();
    }
    
    void client_thread(WebSockets_Callback_Options options){
        cout << "%s" << "WebSockets_Callback_Options::client_thread()";
        
        net::io_context ioc;
        auto wsc = std::make_shared<ws_client_session>(ioc);
        wsc->run(options);
        ioc.run();
        
        for(;;){
            if (tg->strToSend.length() > 0){
                cout << "%s %s" << "[WS] Sending message: " << tg->strToSend.c_str();
                wsc->send_str(tg->strToSend);
                tg->strToSend = "";
            }
        }
    }
    
    void server_start(){
        cout << "%s %s %s %i %s %i %s" << "Attempting to start WebSocket server @ " << options.address.c_str() << ":" << options.port << " using " << options.threads, " threads...";
        
        ws_types[0] = "#s";
        ws_types[1] = "#c";

        //thread.reset( new std::thread([this]() {this->server_thread(this->options);}) );
        
        
        thread = std::thread([this]() {this->server_thread(this->options);});
        
        thread.detach();
    }
    
    void client_connect(){
        cout << "%s %s %s %i" << "Attempting to connect to WebSocket server @ " << options.address.c_str() << ":" << options.port;
        
        ws_types[0] = "#c";
        ws_types[1] = "#s";
            
        thread = std::thread([this]() {this->client_thread(this->options);});
        thread.detach();
    }

    
    
    void preOnMessage(const std::string msg, const void* conn){
        
        json json_;  //crashes here
        
        try
        {
            json_ = json::parse(msg.c_str());
        }
        catch (std::invalid_argument & invalid)
        {
            cout << "%s %s" << "Error reading JSON " << invalid.what();
        }
        
        if (json_["puid"] != NULL){
            cout << "%s %s" << "PUID is " << json_["puid"].get<std::string>().c_str();
            this->handleMessage(json_, conn);
        } else {
            cout << "%s" << "PUID is NULL";
            //options.onUnexpectedMessage(conn, json);
        }
        
       /* if (true){
            this->handleMessage(json, conn);
        } else {
            if (options.onResponselessMessage != 0) options.onResponselessMessage(msg);
        }*/
        
        
    }
    
    
    void handleMessage(json json_, const void* conn){
        std::string sender = json_["puid"].get<std::string>().substr(0,2);
        if (sender == this->ws_types[0]){
            //t.handleResponse(t, json, conn)
            cout << "%s" << "===============   Message requires a response";
        } else  if (sender == this->ws_types[1]){
            //t.handleExpectation(t, json, conn)
            cout << "%s" << "===============   Handling expectation";
        }
    }
    /*
    void handleResponse(t, json, conn = undefined){
        //server responded to a message that we expected to have a response
        if (t.expectations[json.puid] != undefined){
            if (json.progress == undefined || json.progress == 100){
                t.expectations[json.puid].onResponse(json);
                delete t.expectations[json.puid];
            } else {
                t.expectations[json.puid].onProgress(json);
            }
        }
    }
    
    void handleExpectation(t, json, conn = undefined){
        t.triggers[json.cmd].doHandle(json, function(response){
            if (response.puid == undefined) response.puid = json.puid;
            t.send(response, undefined, undefined, conn);
            if (this->onResponselessMessage != 0) this->onResponselessMessage();
        })
    }*/
public:
    WebSockets_Callback(){
        
    }
    
    ~WebSockets_Callback(){
         cout << "%s" << "Destroying WebSockets_Callback object...";
    }
    
    void start(){
        cout << "%s" << "Setting preOnMessage()";
        options.callbacks.preOnMessage = [this](const std::string msg, const void* conn) -> void{
            this->preOnMessage(msg, conn);
        };
        
        
        if (options.callbacks.onListening != NULL){
            this->server_start();
        } else {
            this->client_connect();
        }
    }
    
    void stop(){
        cout << "%s" << "Stopping WebSocket...";
    }
    
    void send(std::string command, std::string message, thread_guard& tgAddr, std::function<void()> onResponse = NULL, std::function<void()> onProgress = NULL){
        tg->strToSend = *new std::string("{\"cmd\": \"" + command + "\", \"msg\": \"" + message + "\"}");
    }
    
    void simple(std::string command, std::string message, thread_guard& tgAddr, std::function<void()> onResponse = NULL){
        this->send(command, message, tgAddr, onResponse);
    }
    
    void on(std::string command, std::function<void(const std::string, std::function<void(const std::string)>)> doHandle){
        triggers.add(command, doHandle);
    }
};
