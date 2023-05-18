import tensorflow as tf
import numpy as np
import os
import re
import matplotlib.pyplot as plt
import random
import csv
from scipy.fft import fft
from sklearn.model_selection import train_test_split
from time import sleep

trainroute = "./train"
testroute = "./test"
data = "./data"
classification = {"parado":0,"caminar":1,"correr":2,"bici":3}
nMovements = len(classification)

try:
    nSensor = int(input("Introduzca el número de sensores para utilizar como entrada a la red: \n 1 --> Acelerómetro \n 2 --> Acelerómetro y giroscopio \n 3 --> Acelerómetro, giroscopio y magnetómetro \n"))
    if nSensor > 3 or nSensor < 1:
        raise Exception ("Valor entre 1 y 3, repita de nuevo")
    
    for e in os.listdir(trainroute):
        if re.search(e,".csv"):
            print("--> "+ e)
    dataset = input("Introduzca el nombre del fichero para extraer los datos de entrenamiento \n")
    datasetroute = trainroute + "/" + dataset + ".csv"
    
    for e in os.listdir(testroute):
        if re.search(e,".csv"):
            print("--> "+ e)
    datatest = input("Introduzca el nombre del fichero para realizar la validación del modelo \n")
    datatestroute = testroute + "/" + datatest + ".csv"

except ValueError:
    print("Formato incorrecto")


cols = [i for i in range((3 * nSensor))]

Xtrainaux = np.loadtxt(datasetroute,delimiter=',',skiprows=1,usecols=cols, dtype=float)
Ytrainaux = np.loadtxt(datasetroute,delimiter=',',skiprows=1,usecols=9, dtype=int)

Xtestaux = np.loadtxt(datatestroute,delimiter=',',skiprows=1,usecols=cols, dtype=float)
Ytestaux = np.loadtxt(datatestroute,delimiter=',',skiprows=1,usecols=9, dtype=int)

print(Xtrainaux.shape)
print(Xtestaux.shape)


def normalizar(npInput,valor):
    return npInput/valor

def window(matrix, ventana, salto):
    nventanas = ((matrix.shape[0] - ventana) // salto) + 1
    shape = (nventanas, ventana,3 * nSensor)
    strides = ((salto * matrix.strides[0],) + matrix.strides)
    return np.lib.stride_tricks.as_strided(matrix, shape=shape, strides=strides)

def createWindows(windowsize,file):
    directory = data + "/" + file
    ls = [e for e in os.listdir(directory) if re.search(".csv",e)]
    Xaux = []
    Yaux = []
    for e in ls:
        ruta = directory + "\{}".format(e)
        rx = np.loadtxt(ruta,delimiter=',',skiprows=1,usecols=cols, dtype=float)
        rx = rx[:(len(rx)//windowsize) * windowsize]
        yx = np.loadtxt(ruta,delimiter=',',skiprows=1,usecols=9, dtype=int)
        windowaux = window(rx,windowsize,windowsize)
        Xaux.append(windowaux)
        Yaux.append(yx[:windowaux.shape[0]])
    
    Xwindow = []
    Ywindow = []
    for i in range(0,len(Xaux)):
        Xwindow.extend(Xaux[i])
        Ywindow.extend(Yaux[i])
    
    Xwindow = np.array(Xwindow,dtype = float)# Vectores de entrada para el entrenamiento
    Ywindow = np.array(Ywindow, dtype = int)
    return(Xwindow,Ywindow)

def shuffledata(x,y):
    assert(len(x) == len(y))
    randomseq = random.sample(range(len(x)),len(x))
    x = np.array([x[i] for i in randomseq],dtype = float)
    y = np.array([y[i] for i in randomseq],dtype = int)
    return(x,y)

X,Y = createWindows(26,dataset)
#X = np.abs(fft(X))
X = normalizar(X,2000)

Xval,Yval = createWindows(26,datatest)
#Xtest = np.abs(fft(Xtest))
Xval = normalizar(Xval,2000)

X,Y = shuffledata(X,Y)

print(X.shape)
print(Y.shape)

Xval,Yval = shuffledata(Xval,Yval)

Xval,Xtest,Yval,Ytest = train_test_split(Xval,Yval,test_size = 0.25)

print(Xval.shape)
print(Yval.shape)

print(Xtest.shape)
print(Ytest.shape)


modelname = input("Introduzca el nombre del modelo \n")

#tensorboard = TensorBoard(log_dir='logs/' + modelname,write_graph = True)

modelo = tf.keras.models.Sequential([
    tf.keras.layers.Conv1D(16,5, activation='relu', input_shape=(26,3 * nSensor)),
    tf.keras.layers.MaxPooling1D(2),
    
    tf.keras.layers.Conv1D(8,5, activation='relu'),
    tf.keras.layers.MaxPooling1D(2),
    tf.keras.layers.Dropout(0.5),
    
    tf.keras.layers.Flatten(),
    tf.keras.layers.Dense(25, activation='relu'),
    tf.keras.layers.Dense(nMovements, activation="softmax")
])

#Compilación
modelo.compile(optimizer='adam',
              loss='sparse_categorical_crossentropy',
              metrics=['accuracy'])

modelo.summary()

epocas=15
history = modelo.fit(
    X,
    Y,
    epochs=epocas,
    validation_data = (Xtest,Ytest),
    #callbacks = [tensorboard]
)
modelo.summary()

loss,acc = modelo.evaluate(Xtest,Ytest,verbose = 2)

modelo.save(modelname + ".h5")

sleep(10)

converter = tf.lite.TFLiteConverter.from_keras_model(modelo)
tflite_model = converter.convert()

with open(modelname + ".tflite", 'wb') as f:
  f.write(tflite_model)