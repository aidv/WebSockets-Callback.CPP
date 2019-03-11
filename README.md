# websockets-callback.cpp
A C++ port of my JS library [WebSockets-Callback](https://github.com/aidv/websockets-callback).

### IMPORTANT
This C++ library most likely infested with TONS of bugs.
Help me kill the bugs 🕷

### DEPENDENCIES
[Boost](https://github.com/boostorg/boost)

[Beast](https://github.com/boostorg/beast/)

[Nlohmann JSON](https://github.com/nlohmann/json)

### Introduction
This library aims at simplifying the integration of WebSockets to your C++ project
by exposing simple functions that do specific jobs and that do them very well.

### Advantages
- Non-blocking multithreading
- Easy to use

### KNOWN ISSUES
- Server session async_read reports "Operation canceled" when all clients have disconnected. To be fixed.

### Usage

### XCode
Check the XCode demo project.

```js
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
    
    
    //wscb->options.asClient = true;
    wscb->options.address = "192.168.1.5";
    
    wscb->options.callbacks.onOpen = [](void* conn) -> void{
        std::cout << "[WS] Connection opened!" << std::endl;
        
        wscb->simple("testing", [](json json_) -> void{
            std::cout << "Response: " << json_.dump() << std::endl;
        }, conn);
    };
    
    //uncomment "onListening" to start a WebSocket server instead
    /*wscb->options.callbacks.onListening = []() -> void{
        std::cout << "[WS] Server listening! \n";
    };*/
    
    wscb->options.callbacks.onError = [](WSCB_Error error) -> void{
        std::cout << "[WS] Error: " << error.message<< ": " << error.error.message() << std::endl;
    };
    
    wscb->options.callbacks.onClose = []() -> void{
        std::cout << "[WS] Session closed: <reason>" << std::endl;
    };
    
    
    
    wscb->options.callbacks.onUnexpectedMessage = [](const json json_, const void* conn) -> void{
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
    
    
    std::thread thread = std::thread([]() {timerFunc();});
    thread.detach();
    
    std::cout << "Press any key to continue...";
    std::cin.get();
    
    
    return EXIT_SUCCESS;
}
```

### CONTRIBUTE
Let's help each other make this library the best WebSocket library for C++.

You can contribute in any way you want;
- Fork, code, commit
- Open new issues and report bugs or suggest improvements
- Ask questions so I can add information to the README file

### CREDITS

- [gmbeard](https://github.com/gmbeard) - Suggested to replace rubberArray with std::vector.
- [vinniefalco](https://github.com/vinniefalco/) - Gave some recognition. I'll use it as a "stamp of approval" since this library is heavily relying on his Boost library.
- You. For using/contributing/sharing my library.

Star this repository on github, please. Thank you.