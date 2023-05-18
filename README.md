# TFG
 Red neuronal embebida para clasificación de movimiento.

En el se incluyen:
- Código de la aplicaciones en distintas versiones
- Scripts en python para el entrenamiento de la red y formateo de ficheros.
- El dataset se encuentra en la carpeta data.
- Modelo entrenado y almacenando en formato .h5.
- Proyecto del servidor web con todas las dependencias.

Advertencias:
- Tener instalado la versión 7.0 del framework X-CUBE-AI.
- En caso de entrenar un modelo propio tener la versión 2.5.0 o inferior de Tensorflow.
- Dicho modelo está entrenado con una serie de datos en movimiento, con los sensores ubicados en una zona cercana al tobillo, de no emplearlo de esa forma los resultados pueden ser erróneos.
- Para ejecutar el servidor nos dirigimos a la ruta del proyecto y teclear el comando "node index.js".
- Para la aplicación web, asegurarse de que tanto el dispositivo como el servidor están dentro de la misma red.
