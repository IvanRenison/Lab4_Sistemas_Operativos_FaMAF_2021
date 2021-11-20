## Corrección
* Grupo:  27
* Docente: Milagro Teruel
* Commit: 1386b52
* Porcentaje: 100


|  | Concepto | Comentario | Estado |
|---|---|---|---|
| Entrega & Informe | En tiempo: 16/10/21 a las 15:59 |  | ok |
|  | Commit de todos los integrantes |  | ok |
|  | Commit frecuentes |  | ok |
|  | No Entregaron código objeto y/o ejecutables |  | ok |
|  |  |  |  |
| Calidad del Codigo | Comentarios explicativos en el código |  | ok |
|  | Uniformidad Idiomática |  | ok |
|  | Consistencia en los TABs, espacios, identación |  | ok |
|  | Líneas de codigo "cortas" y bloques de codigo "chicos" |  | ok |
|  | Nombres de variables informativos y utilizacion de constantes |  | ok |
|  | ¿Respetan el estilo de código? (`git clang-format --diff --extension c,h <commit con codigo inicial>`) |  | ok |
|  | Codigo redundante |  | ok |
|  | Sencillez y legibilidad |  | ok |
|  | ¿Tocaron solo los archivos pertinentes? (`fat_fuse_ops.c`, `fat_file.c` etc.) |  | ok |
|  | ¿Definieron constantes? ¿Utilizaron las constantes ya dadas en `big_brother.h`? |  | ok |
|  | Modularizacion y encapsulamiento |  | ok |
|  |  |  |  |
| Funcionalidad | Creación del archivo de logs al inicio del sistema |  | ok |
|  | Registro de actividades de lectura y escritura |  | ok |
|  | Se oculta el archivo de logs en la operación `fat_fuse_readdir` |  | ok |
|  | Se oculta el archivo para otros sistemas de archivos |  | ok |
|  | Al eliminar un archivo, se liberan todos sus clusters |  | ok |
|  | Al eliminar un archivo, se escribe su entrada de directorio |  | ok |
|  | Al eliminar un archivo, se lo elimina del árbol de directorios |  | ok |
|  | Al eliminar un archivo, se comprueba que el archivo existe y que no es un directorio |  | ok |
|  | Al escribir un archivo con clusters nuevos, se setea el nuevo EOC |  | ok |
|  | Al escribir un archivo con clusters nuevos, se actualiza el campo `fat_file->file.num_clusters` |  | no |
|  | [Opcional] Al escribir un archivo con clusters nuevos, se contempla el caso en que el offset sea múltiplo del tamaño del cluster |  | ok |
|  |  |  |  |
| Puntos estrellas | Agregar opción al montar el filesystem que permita mostrar/ocultar el archivo de logs |  | ok |
|  | Agregar registro de palabras censuradas |  | ok |
|  | Agregar soporte para eliminar directorios. |  | ok |
|  | Agregar una caché en memoria con cadena de clusters |  | no |
|  | Agregar soporte para nombres de archivo de más de 8 caracteres |  | no |
|  | Agregar un nuevo cluster al directorio cuando este se llena de entradas |  | no |
|  | Utilizar la segunda copia de la FAT |  | no |