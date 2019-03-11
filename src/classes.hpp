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

using json = nlohmann::json;


namespace beast = boost::beast;         // from <boost/beast.hpp>
namespace http = beast::http;           // from <boost/beast/http.hpp>
namespace websocket = beast::websocket; // from <boost/beast/websocket.hpp>
namespace net = boost::asio;            // from <boost/asio.hpp>
using tcp = boost::asio::ip::tcp;       // from <boost/asio/ip/tcp.hpp>


struct WSCB_Error {
    std::string message = "";
    beast::error_code error;
};

struct WSCB_Callbacks {
    WSCB_Error error;
    std::function<void()> onListening = NULL ;
    std::function<void(void*)> onOpen;
    
    std::function<void(std::string*, const void*)> preOnMessage;
    std::function<void(const json, const void*)> onUnexpectedMessage;
    
    std::function<void(WSCB_Error)> onError;
    std::function<void()> onClose;
    
    void triggerOnOpen(void* conn = NULL){ if (this->onOpen != 0) this->onOpen(conn); }
    
    void triggerOnListening(){ if (this->onListening != 0) this->onListening(); }
    
    void triggerPreOnMessage(std::string* msg, void* conn){ if (this->preOnMessage != 0) this->preOnMessage(msg, conn); }
    void triggerOnUnexpectedMessage(const json json_, const void* conn){ if (this->onUnexpectedMessage != 0) this->onUnexpectedMessage(json_, conn); }
    
    
    void triggerOnError(std::string message){
        if (this->onError != 0){
            this->error.message = message;
            this->onError(this->error);
        }
    }
    
    void triggerOnClose(){if (this->onClose != 0) this->onClose(); }
    
};

struct WSCB_Options {
    bool asClient = false;
    //bool verbose = false;
    std::string address = "127.0.0.1";
    int port = 8081;
    int threads = 1;
    WSCB_Callbacks callbacks;
    
    std::vector<void*>* wsClientSessions;
    void* clientSession = NULL;
};





struct WSCB_Message {
    std::string cmd = "";
    std::string puid = "";
};


/**********/

struct WSCB_Trigger{
    std::string puid;
    std::string command;
    std::function<void(const json, std::function<void(const json)>)> doHandle;
    std::function<void(const json)> onResponse;
    std::function<void(const json)> onProgress;
};

class WSCB_Triggers{
public:
    std::vector<WSCB_Trigger*> elements;
    WSCB_Trigger* add(
             std::string cmd,
             std::function<void(const json, std::function<void(const json)>)> doHandle = NULL,
             std::function<void(const json)> onResponse = NULL,
             std::function<void(const json)> onProgress = NULL){
        
        WSCB_Trigger* trigger = new WSCB_Trigger();
        trigger->command = cmd;
        
        if (doHandle   != 0) trigger->doHandle   = doHandle;
        if (onResponse != 0) trigger->onResponse = onResponse;
        if (onProgress != 0) trigger->onProgress = onProgress;
        
        elements.push_back(trigger);
        
        return trigger;
    }
    
    int indexOfPUID(std::string puid){
        for (int i = 0; i < elements.size(); i++)
            if (elements[i]->puid == puid)
                return i;
        
        return -1;
    }
    
    WSCB_Trigger* getByPUID(std::string id){
        for (int i = 0; i < elements.size(); i++)
            if (elements[i]->puid == id)
                return elements[i];
        
        return NULL;
    }
    
    int indexOfCommand(std::string cmd){
        for (int i = 0; i < elements.size(); i++)
            if (elements[i]->command == cmd)
                return i;
        
        return -1;
    }
    
    WSCB_Trigger* getByCommand(std::string cmd){
        for (int i = 0; i < elements.size(); i++)
            if (elements[i]->command == cmd)
                return elements[i];
        
        return NULL;
    }
};
