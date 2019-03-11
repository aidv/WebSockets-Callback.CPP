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
    WSCB_Triggers* expectations;
    WSCB_Triggers* triggers;
public:
    WSCB_Options* options;
private:
    std::string ws_types [2]; //0 = expector, 1 = responder
    
    void server_thread(WSCB_Options* options){
        //cout << "\n" <<  "WebSockets_Callback_Options::server_thread()";
        
        net::io_context ioc{options->threads};
        
        // Create and launch a listening port
        std::make_shared<ws_listener>(ioc, boost::asio::ip::tcp::endpoint{
            net::ip::make_address(options->address.c_str()),
            static_cast<unsigned short>(options->port)
        })->run(options);
        
        // Run the I/O service on the requested number of threads
        std::vector<std::thread> v;
        v.reserve(options->threads - 1);
        
        for(auto i = options->threads - 1; i > 0; --i)
            v.emplace_back([&ioc]{ioc.run();});
        
        ioc.run();
    }
    
    void client_thread(WSCB_Options* options){
        net::io_context ioc;
        auto wsc = std::make_shared<ws_client_session>(ioc);
        wsc->run(options);
        ioc.run();
    }
    
    void server_start(){
        std::cout << "\n" << "Attempting to start WebSocket server @ " << options->address.c_str() << ":" << options->port << " using " << options->threads << " threads..." << std::endl;
        
        thread = std::thread([this]() {this->server_thread(this->options);});
        thread.detach();
    }
    
    void client_connect(){
        thread = std::thread([this]() {this->client_thread(this->options);});
        thread.detach();
    }

    
    
    void preOnMessage(std::string msg, const void* conn){
        json json_;
        
        try{
            json_ = json::parse(msg.c_str());
        } catch (std::invalid_argument & invalid){
            std::cout << "\n" << "Error reading JSON " << invalid.what();
            return;
        }
        
        bool hasPUID = (json_.find("puid") != json_.end());
        
        if (hasPUID){
            this->handleMessage(json_, conn);
        } else {
            options->callbacks.onUnexpectedMessage(json_, conn);
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
        
        int expectationIndex = expectations->indexOfPUID(json_["puid"].get<std::string>());
        WSCB_Trigger* expectation = (expectationIndex == -1 ? NULL : expectations->elements[expectationIndex]);
        
        if (expectation != NULL){
            bool hasProgress = (json_.find("progress") != json_.end());
            std::string progress = "";
            if ((json_.find("progress") != json_.end())){
                progress = json_["progress"].get<std::string>();
            }
            
            if (hasProgress == false || progress == "100"){
                expectation->onResponse(json_);
                expectations->elements.erase(expectations->elements.begin() + expectationIndex);
            } else {
                expectation->onProgress(json_);
            }
        }
    }
    
    void handleExpectation(json json_, const void* conn){
        WSCB_Trigger* trigger = triggers->getByCommand(json_["cmd"].get<std::string>());
        
        if (trigger == NULL) return;
        
        trigger->doHandle(json_, [this, json_, conn](json response) -> void{
            this->send(response, NULL, NULL, conn);
        });
    }
public:
    WebSockets_Callback(){
        options = new WSCB_Options;
        expectations = new WSCB_Triggers;
        triggers = new WSCB_Triggers;
        if (options->asClient == false)
            options->wsClientSessions = new std::vector<void*>();
    }
    
    ~WebSockets_Callback(){
         //cout << "\n" <<  "Destroying WebSockets_Callback object...";
    }
    
    void start(){
        
        if (options->asClient == false){
            ws_types[0] = "#s";
            ws_types[1] = "#c";
        } else {
            ws_types[0] = "#c";
            ws_types[1] = "#s";
        }
        
        
        options->callbacks.preOnMessage = [this](std::string* msg, const void* conn) -> void{
            this->preOnMessage(*msg, conn);
        };
        
        
        if (options->callbacks.onListening != NULL || options->asClient == false){
            this->server_start();
        } else {
            this->client_connect();
        }
    }
    
    void stop(){
        std::cout << "\n" <<  "Stopping WebSocket...";
    }
    
    void send(json json_, std::function<void(json)> onResponse = NULL, std::function<void(json)> onProgress = NULL, const void* conn = NULL){
        
        json json__ = json::parse(json_.dump());
        
        if (onResponse != NULL){
            std::string puid = ws_types[0] + std::to_string((uint64_t)std::time(nullptr));
            WSCB_Trigger* expectation = expectations->add("", NULL, onResponse, onProgress);
            expectation->puid = puid;
            json__["puid"] = puid;
        }
        
        
        if (options->asClient == false){
            if (conn != NULL){
                server_session* conn_ = ((server_session*)conn);
                conn_->outputMessageQueue->push_back(json__.dump());
                if (conn_->writing == false) conn_->reset();
            } else {
                for (int i = 0; i < options->wsClientSessions->size(); i++){
                    std::vector<void*> sessions = *options->wsClientSessions;
                    server_session* conn_ = ((server_session*)sessions[i]);
                    conn_->outputMessageQueue->push_back(json__.dump());
                    if (conn_->writing == false) conn_->reset();
                }
            }
        } else {
            if (options->clientSession == NULL) return;
            ws_client_session* conn_ = ((ws_client_session*)options->clientSession);
            conn_->outputMessageQueue->push_back(json__.dump());
            if (conn_->writing == false) conn_->reset();
        }
    }
    
    void simple(std::string command, std::function<void(json)> onResponse = NULL, void* conn = NULL){
        this->send(
            json::parse("{\"cmd\": \"" + command + "\"}"),
            onResponse,
            NULL,
            conn
        );
    }
    
    void on(std::string command, std::function<void(const json, std::function<void(const json)>)> doHandle){
        triggers->add(command, doHandle);
    }
};
