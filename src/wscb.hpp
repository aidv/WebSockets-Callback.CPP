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

#include <nlohmann/json.hpp>

#include "src/ws.hpp"
#include "src/classes.hpp"


using json = nlohmann::json;

namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


class WebSockets_Callback {
    std::thread thread;
    std::condition_variable cv;
    WebSockets_Callback_Triggers expectations;
    WebSockets_Callback_Triggers triggers;
public:
    WebSockets_Callback_Options options;
    std::mutex m;
private:
    std::string ws_types [2]; //0 = "#s", 1 = "#c"
    
    void server_thread(WebSockets_Callback_Options options){
        //cout << "\n" <<  "WebSockets_Callback_Options::server_thread()";
        
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
        //cout << "\n" <<  "WebSockets_Callback_Options::client_thread()";
        
        net::io_context ioc;
        auto wsc = std::make_shared<ws_client_session>(ioc);
        wsc->run(options);
        ioc.run();
        
        /*for(;;){
            if (options.tg->strToSend.length() > 0){
                //cout << "\n" << "[WS] Sending message: " << options.tg->strToSend.c_str();
                wsc->send_str(options.tg->strToSend);
                options.tg->strToSend = "";
            }
        }*/
    }
    
    void server_start(){
        std::cout << "\n" << "Attempting to start WebSocket server @ " << options.address.c_str() << ":" << options.port << " using " << options.threads << " threads...";
        
        ws_types[0] = "#s";
        ws_types[1] = "#c";

        //thread.reset( new std::thread([this]() {this->server_thread(this->options);}) );
        
        
        thread = std::thread([this]() {this->server_thread(this->options);});
        
        thread.detach();
    }
    
    void client_connect(){
        //cout << "\n" << "%s %s %s %i" << "Attempting to connect to WebSocket server @ " << options.address.c_str() << ":" << options.port;
        
        ws_types[0] = "#c";
        ws_types[1] = "#s";
            
        thread = std::thread([this]() {this->client_thread(this->options);});
        thread.detach();
    }

    
    
    void preOnMessage(const std::string msg, const void* conn){
        json json_;  //crashes here
        
        try{ json_ = json::parse(msg.c_str());
        } catch (std::invalid_argument & invalid){
            std::cout << "\n" << "Error reading JSON " << invalid.what();
            return;
        }
        
        if (json_["puid"] != NULL){
            this->handleMessage(json_, conn);
        } else {
            options.callbacks.onUnexpectedMessage(json_, conn);
        }
    }
    
    
    void handleMessage(json json_, const void* conn){
        std::string sender = json_["puid"].get<std::string>().substr(0,2);
        if (sender == this->ws_types[0]){
            this->handleResponse(json_, conn);
        } else  if (sender == this->ws_types[1]){
            this->handleExpectation(json_, conn);
        }
    }
    
    void handleResponse(json json_, const void* conn){
        //server responded to a message that we expected to have a response
        
        int expectationIndex = expectations.indexOfPUID(json_["puid"].get<std::string>());
        WebSockets_Callback_Trigger* expectation = expectations.elements[expectationIndex];
        
        if (expectation != NULL){
            if (json_["progress"] == NULL || json_["progress"].get<std::string>() == "100"){
                expectation->onResponse(json_);
                expectations.elements.erase(expectations.elements.begin() + expectationIndex);
            } else {
                expectation->onProgress(json_);
            }
        }
    }
    
    void handleExpectation(json json_, const void* conn){
        WebSockets_Callback_Trigger* trigger = triggers.getByCommand(json_["cmd"].get<std::string>());
        
        if (trigger == NULL) return;
        
        trigger->doHandle(json_, [this, json_](json response) -> void{
            this->send(response);
        });
    }
public:
    WebSockets_Callback(){
        options.strQueue = new std::vector<std::string>();
    }
    
    ~WebSockets_Callback(){
         //cout << "\n" <<  "Destroying WebSockets_Callback object...";
    }
    
    void start(){
        //cout << "\n" <<  "Setting preOnMessage()";
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
        std::cout << "\n" <<  "Stopping WebSocket...";
    }
    
    void send(json json_, std::function<void(json)> onResponse = NULL, std::function<void(json)> onProgress = NULL){
        options.strQueue->push_back(json_.dump());
    }
    
    void simple(std::string command, std::function<void(json)> onResponse = NULL){
        options.strQueue->push_back("{\"cmd\": \"" + command + "\"}");
    }
    
    void on(std::string command, std::function<void(const json, std::function<void(const json)>)> doHandle){
        triggers.add(command, doHandle);
    }
};
