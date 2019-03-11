#include <thread>
#include <chrono>

#include "src/wscb.hpp"

WebSockets_Callback* wscb = new WebSockets_Callback;

void timerFunc(){
    int i = 0;
    for (;;){
        wscb->simple("Test " + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::seconds(1));
        i++;
    }
}



int main(int argc, char** argv)
{
    
    
    wscb->options->asClient = true;
    wscb->options->address = "192.168.1.5";
    
    wscb->options->callbacks.onOpen = [](void* conn) -> void{
        std::cout << "[WS] Connection opened!" << std::endl;
        
        wscb->simple("testing", [](json json_) -> void{
            std::cout << "Response: " << json_.dump() << std::endl;
        }, conn);
        
        //start a timer thread that sends a message to the server/clients every second.
        std::thread thread = std::thread([]() {timerFunc();});
        thread.detach();
    };
    
    //uncomment "onListening" to start a WebSocket server instead
    /*wscb->options.callbacks.onListening = []() -> void{
        std::cout << "[WS] Server listening! \n";
    };*/
    
    wscb->options->callbacks.onError = [](WSCB_Error error) -> void{
        std::cout << "[WS] Error: " << error.message<< ": " << error.error.message() << std::endl;
    };
    
    wscb->options->callbacks.onClose = []() -> void{
        std::cout << "[WS] Session closed: <reason>" << std::endl;
    };
    
    
    
    wscb->options->callbacks.onUnexpectedMessage = [](const json json_, const void* conn) -> void{
        std::cout << "[WS] Unexpected message: " << json_.dump() << std::endl;
        
        //Unexpected message = Server sent a message and is NOT expecting a response
    };
    
    wscb->on(
        "test", //if the command "test" is received, the lambda below will be executed.
        [](json json_, std::function<void(const json)> respondWith) -> void{
            std::cout << "[WS] Responding to expectation 'testing' with it's own message." << std::endl;
            respondWith(json_);
        }
    );
    
    wscb->on(
        "progress", //if the command "test" is received, the lambda below will be executed.
        [](json json_, std::function<void(const json)> respondWith) -> void{
            std::cout << "[WS] Testing progress..." << std::endl;
            
            json json__ = json::parse(json_.dump());
            
            for (int i = 0; i <= 10; i++){
                json__["progress"] = std::to_string(i);
                respondWith(json__);
            }
        }
    );
    
    wscb->start();
    
    
    
    
    std::cout << "Press any key to continue..." << std::endl;
    std::cin.get();
    
    
    return EXIT_SUCCESS;
}
