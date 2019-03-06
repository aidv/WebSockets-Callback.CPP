#include "src/wscb.hpp"

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/function.hpp>
#include <boost/chrono.hpp>
#include <boost/chrono/duration.hpp>
#include <boost/function.hpp>

namespace ns {
    struct person {
        std::string name;
        std::string address;
        int age;
    };
    
    void to_json(json& j, const person& p) {
        j = json{{"name", p.name}, {"address", p.address}, {"age", p.age}};
    }
    
    void from_json(const json& j, person& p) {
        j.at("name").get_to(p.name);
        j.at("address").get_to(p.address);
        j.at("age").get_to(p.age);
    }
};

void sendTest(const boost::system::error_code& e, WebSockets_Callback* wscb)
{
    wscb->simple("\"cmd\": \"testing\"");//, [&](json json_) -> void{
    //    std::cout << "Response: " << json_.dump();
    //});}
}


int main(int argc, char** argv)
{
    bool connected = true;
    
    WebSockets_Callback* wscb = new WebSockets_Callback;
    
    wscb->options.address = "192.168.1.5";
    
    wscb->options.callbacks.onOpen = [wscb]() -> void{
        std::cout << "\n[WS] Connection opened!";
    };
    
    //uncomment "onListening" to start a WebSocket server instead
    /*wscb->options.callbacks.onListening = []() -> void{
        std::cout << "[WS] Server listening! \n";
    };*/
    
    wscb->options.callbacks.onError = [](WebSockets_Callback_Error error) -> void{
        std::cout << "[WS] Error: " << error.message<< ": " << error.error.message() << "\n";
    };
    
    wscb->options.callbacks.onClose = [&connected]() -> void{
        std::cout << "[WS] Session closed: <reason> \n";
        connected = false;
    };
    
    
    
    wscb->options.callbacks.onUnexpectedMessage = [](const json json_, const void* conn) -> void{
        std::cout << "[WS] Unexpected message: " << json_.dump() << "\n";
        
        //Unexpected message = Server sent a message and is NOT expecting a response
    };
    
    wscb->on(
        "Test", //if the command "Test" is received, the lambda below will be executed.
        [](json json_, std::function<void(const json)> respondWith) -> void{
            std::cout << "[WS] Responding to expectation 'TEST' with it's own message. \n";
            respondWith(json_);
        }
    );
    
    

    wscb->simple("testing", [](json json_) -> void{
        std::cout << "Response: " << json_.dump();
    });
    
    wscb->start();
    
    
    while (connected){}
    
    
    return EXIT_SUCCESS;
}
