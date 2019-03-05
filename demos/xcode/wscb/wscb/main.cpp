#include "src/wscb.hpp"


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

int main(int argc, char** argv)
{
    bool connected = true;
    
    WebSockets_Callback* wscb = new WebSockets_Callback;
    
    wscb->options.address = "192.168.1.5";
    
    wscb->options.callbacks.onOpen = [wscb]() -> void{
        cout << "\n[WS] Connection opened!";
    };
    
    //uncomment "onListening" to start a WebSocket server instead
    /*wscb->options.callbacks.onListening = []() -> void{
        cout << "[WS] Server listening! \n";
    };*/
    
    wscb->options.callbacks.onError = [](WebSockets_Callback_Error error) -> void{
        cout << "[WS] Error: " << error.message.c_str() << "\n";
    };
    
    wscb->options.callbacks.onClose = [&connected]() -> void{
        cout << "[WS] Session closed: <reason> \n";
        connected = false;
    };
    
    
    
    wscb->options.callbacks.onUnexpectedMessage = [](const json json_, const void* conn) -> void{
        cout << "[WS] Unexpected message: " << json_.dump() << "\n";
        
        //Unexpected message = Server sent a message and is NOT expecting a response
    };
    
    wscb->on(
        "Test", //if the command "Test" is received, the lambda below will be executed.
        [](json json_, std::function<void(const json)> respondWith) -> void{
            cout << "[WS] Responding to expectation 'TEST' with it's own message. \n";
            respondWith(json_);
        }
    );
    
    wscb->start();
    
    
    while (connected){}
    
    
    return EXIT_SUCCESS;
}
