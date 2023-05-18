
const socket = io.connect('http://localhost:3000');// Nos conectamos al servidor de node

const oldclass = "imagen"
const newclass = "imagenseleccionada"

let oldid = 1;

const accId = document.getElementById('acc');

function replaceClass(data){ //Cambiar la clase de normal a seleccionada para que aparezca resaltada en el navegador
    
    const parseData = data.split(",");
    
    const id = parseData[0];
    const acc = (Number.parseFloat(parseData[1]) * 100).toFixed(2).toString();

    accId.innerHTML = acc + "%";  
        
    if(id != oldid){
        
        document.getElementById(id).classList.replace(oldclass,newclass);
    
        document.getElementById(oldid).classList.replace(newclass,oldclass);
    
        oldid = id;
    }
    
}

socket.on('data', function (data) { // Escuchamos al evento con los datos recibidos
    console.log(data);
    replaceClass(data);
});


