#include "src/wscb.hpp"

int main(int argc, char** argv)
{
    bool connected = true;
    
    WebSockets_Callback* wscb = new WebSockets_Callback;
    
    
    //wscb->options.MIDIEvents = &fParams.MIDIEvents;
    wscb->options.address = "192.168.1.5";
    
    wscb->options.tg = new thread_guard();
    
    wscb->options.callbacks.onOpen = [wscb]() -> void{
        cout << "\n[WS] Connection opened!";
        wscb->simple("testing", "1234", *wscb->options.tg);
    };
    
    /*wscb.options.callbacks.onListening = []() -> void{
     DLOG_F(INFO, "%s", "[WS] Server listening!");
     };*/
    
    wscb->options.callbacks.onMessage = [](const std::string msg) -> bool{
        cout << "\n[WS] Message: " << msg.c_str();
        return true;
    };
    
    wscb->options.callbacks.onError = [](WebSockets_Callback_Error error) -> void{
        cout << "\n[WS] Error: " << error.message.c_str();
    };
    
    wscb->options.callbacks.onClose = [&connected]() -> void{
        cout << "\n[WS] Session closed: <reason>";
        connected = false;
    };
    
    
    wscb->on(
        "Test",
        [](json json_, std::function<void(const json)> respondWith) -> void{
            cout << "\n[WS] RESPONDING TO EXPECTATION!";
            respondWith(json_);
        }
    );
    
    wscb->start();
    
    while (connected){}
    
    
    return EXIT_SUCCESS;
}
