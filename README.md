# websockets-callback.cpp
A C++ port of my JS library [WebSockets-Callback](https://github.com/aidv/websockets-callback).

### IMPORTANT
This C++ library most likely infested with TONS of bugs.
Help me kill the bugs 🕷

### DEPENDENCIES
[Boost](https://github.com/boostorg/boost)

[Beast](https://github.com/boostorg/beast/)

[Nlohmann JSON](https://github.com/nlohmann/json)

[Nemimccarter rubberArray](https://github.com/nemimccarter/rubberArray)

### Introduction
This library aims at simplifying the integration of WebSockets to your C++ project
by exposing simple functions that do specific jobs and that do them very well.

### Advantages
- Non-blocking multithreading
- Easy to use

### Usage

### XCode
Check the XCode demo project.

```js
#include "src/wscb.hpp"

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
```

### CONTRIBUTE
Let's help each other make this library the best WebSocket library for C++.

You can contribute in any way you want;
- Fork, code, commit
- Open new issues and report bugs or suggest improvements
- Ask questions so I can add information to the README file


Star this repository on github, please. Thank you.