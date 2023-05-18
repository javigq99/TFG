const path = require('path');
const express = require('express');
const app = express();

const http = require('http');
const server = http.createServer(app);
const {Server} = require('socket.io');
const io = new Server(server);

const { Socket } = require('net');
const socket = new Socket();
socket.connect({host: '192.168.11.91', port: 3001}); //Socket del dispositivo

let conn = null;


socket.on("data",(data) => { //Escuchamos al evento asociado a los datos que envia el dispositivo

        io.emit('data',data.toString()); // Enviamos los datos al websocket
}
);

socket.on("disconnect",() => {
    socket.close();
})

app.use(express.static(path.join(__dirname, 'public')));


app.get('/',(req,res) => {
    res.sendFile(__dirname + '/public/index.html');
});


io.on('connection',(socket)=>{
    conn = socket;
    console.log('Conectado ' + socket.id);
    socket.emit('data','0,1.00');
});

server.listen(3000,()=>{
    console.log('listening on port 3000');
})
