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

#include <nlohmann/json.hpp>

using json = nlohmann::json;


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


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
    std::function<void(const json, const void* conn)> onUnexpectedMessage;
    std::function<void(const std::string)> onMessage;
    
    std::function<void(WebSockets_Callback_Error)> onError;
    std::function<void()> onClose;
    
    void triggerOnOpen(){ if (this->onOpen != 0) this->onOpen(); }
    
    void triggerOnListening(){ if (this->onListening != 0) this->onListening(); }
    
    void triggerPreOnMessage(std::string msg, void* conn){ if (this->preOnMessage != 0) this->preOnMessage(msg, conn); }
    void triggerOnUnexpectedMessage(const json json_, const void* conn){ if (this->onUnexpectedMessage != 0) this->onUnexpectedMessage(json_, conn); }
    
    
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
    
    thread_guard* tg;
};





struct WebSockets_Callback_Message {
    std::string cmd = "";
    std::string puid = "";
};


/**********/

struct WebSockets_Callback_Trigger{
    std::string puid;
    std::string command;
    std::function<void(const json, std::function<void(const json)>)> doHandle;
    std::function<void(const json)> onResponse;
    std::function<void(const json)> onProgress;
};

class WebSockets_Callback_Triggers{
public:
    RubberArray<WebSockets_Callback_Trigger*> elements;
    void add(
             std::string cmd,
             std::function<void(const json, std::function<void(const json)>)> doHandle = NULL,
             std::function<void(const json)> onResponse = NULL,
             std::function<void(const json)> onProgress = NULL){
        
        WebSockets_Callback_Trigger* trigger;
        trigger = new WebSockets_Callback_Trigger();
        trigger->command = cmd;
        
        if (doHandle   != 0) trigger->doHandle   = doHandle;
        if (onResponse != 0) trigger->onResponse = onResponse;
        if (onProgress != 0) trigger->onProgress = onProgress;
        
        elements.append(trigger);
    }
    
    int indexOfPUID(std::string puid){
        for (int i = 0; i < elements.length(); i++)
            if (elements[i]->puid == puid)
                return i;
        
        return -1;
    }
    
    WebSockets_Callback_Trigger* getByPUID(std::string id){
        for (int i = 0; i < elements.length(); i++)
            if (elements[i]->puid == id)
                return elements[i];
        
        return NULL;
    }
    
    int indexOfCommand(std::string cmd){
        for (int i = 0; i < elements.length(); i++)
            if (elements[i]->command == cmd)
                return i;
        
        return -1;
    }
    
    WebSockets_Callback_Trigger* getByCommand(std::string cmd){
        for (int i = 0; i < elements.length(); i++)
            if (elements[i]->command == cmd)
                return elements[i];
        
        return NULL;
    }
    
    
};
