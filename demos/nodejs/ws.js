const WebSockets_Callback = require('wscb');
var wscb = new WebSockets_Callback({
    //asClient: true,
    address: "192.168.1.5",
    onOpen: function(conn){
           console.log('Client connected')
           //Send some tests to client and wait for responses
            

            /*wscb.send({cmd: 'test'},
                function(response){
                },
                undefined, //not expecting any progress
                conn
            )*/

            console.log('Requesting progress...')
            wscb.send({cmd: 'progress'},
                function(response){
                    console.log('Progress test completed!');
                },
                function(response){
                    console.log(response.progress + '% done');
                },
                conn
            )
            
            
            
            var i = 0;
            setInterval(() => {
                var str = 'test';// + i;
                console.log('Sending: ' + str)
                i++;
                
                wscb.send({cmd: str, delay: 1000},
                undefined,/*function(response){
                    console.log('CLIENT RESPONDED WITH: ' + JSON.stringify(response))
                },*/
                undefined, //not expecting any progress
                conn
            )
            }, 1000);
            
            
    },

    onClose: function(event){
        console.log('Client disconnected')
    }

});

wscb.on('testing', function(msg, respondWith){
    console.log('Received TESTING signal!');
    respondWith({cmd: "this is a response"});
})

wscb.on('hello from client :)', function(msg, respondWith){
    console.log('Client said:')
    console.log(msg)
    respondWith({msg: 'hi from server :D'});
})

wscb.on('waitFor', function(msg, respondWith){
    setTimeout(() => {
        respondWith({msg: 'Delayed for ' + msg.delay + ' ms'});
    }, msg.delay);
})

wscb.on('progress', function(msg, respondWith){
    var progress = -1;
    var progressTimer = setInterval(() => {
        progress++;
        if (progress >= 100){
            progress = 100;
            clearInterval(progressTimer);
        }
        respondWith({progress: progress});
    }, 10);
})



wscb.options.onUnexpectedMessage = function(conn, msg){
    console.log('Client sent a responseless message: ' + JSON.stringify(msg))
}
