#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>      //Define tipos utilizados por llamadas al sistema
#include <sys/ipc.h>        //Proporciona las claves y constantes para comunicacion entre procesos
#include <sys/shm.h>        //Declara las funciones de memoria compartida

#include <sys/mman.h>   // mmap() y munmap()
#include <sys/stat.h>   // stat()
#include <fcntl.h>      // open(), O_RDONLY y O_RDWR
#include <unistd.h>     // close()
#include <sys/wait.h>   // waitpid()

#include <errno.h>

#include "memoria.h"
#include "listaMemoria.h"


static ListaMemoria lista;  //static hace que lista solo sea accesible desde memoria.c


//Variables globales con enlace externo para memory -vars
int externa1 = 10;
int externa2 = 20;
int externa3;

//Inicializamos la variable lista que hemos creado
void inicializarMemoria(void){

    inicializarListaMemoria(&lista);    // Utilizamos la funcion de listaMemoria.c
}



//Funcion que crea un segmento de memoria compartida y lo conecta al proceso de la shell
static void *crearMemoriaCompartida(key_t clave, size_t tamano){   //clave es el identificador para localizar la memoria compartida y tamano la cantidad de bytes que queremos reservar

    int id;             //Identificador del segmento de memoria compartida
    void *direccion;    //Direccion donde se conectara la memoria

    id = shmget(clave, tamano, IPC_CREAT | IPC_EXCL | 0666);    //shmget solicita al S.O. crear un segmento de memoria compartida

    if (id == -1){                                              //Mostramos un error si no se pudo crear el segmento
        perror("Error al crear la memoria compartida");
        return NULL;
    }


    direccion = shmat(id,NULL,0);                               //shmat conecta el segmento de memoria compartida al espacio de direcciones de nuestro proceso


    //Comprobamos si la conexion falla
    if (direccion == (void *) -1) {

        perror("Error al conectar memoria compartida");
        
        shmctl(id, IPC_RMID, NULL);                             //shmctl elimina el segmento recien creado al haber fallado la conexion
        return NULL;
    }

    //Si la conexion ha sido efectiva, devolvemos la direccion de memoria compartida que acabamos de conectar
    return direccion;



}




//Funcion que conecta la shell a un segmento de memoria compartida que ya existe
static void *conectarMemoriaCompartida(key_t clave, size_t *tamano){        //la funcion recibe *tamano en este caso ya que queremos que pueda modificar la variable tamano que le pasamos

    int id;                 //Identificador del segmento
    void *direccion;        //Direccion a donde se conectara la memoria
    struct shmid_ds datos;  //Estructura que almacena la informacion del segmento, como su tamano y sus permisos

    
    id = shmget(clave,0,0666);

    if (id == -1){
        perror("Erro al obtener la memoria compartida");
        return NULL;
    }


    //Conectamos el segmento a la shell
    direccion = shmat(id,NULL,0);   // 0 = conexion normal, con lectura y escritura


    //Si falla la conexion, mostramos un error
    if (direccion == (void *) -1){
        perror("Error al conectar memoria compartida");
        return NULL;
    }


    //Consultamos la informacion del segmento
    if (shmctl(id,IPC_STAT,&datos) ==  -1){             //con IPC_STAT solicitamos la informacion del segmento, y &datos es la direccion de la estructura donde se guardara la informacion

        perror("Error al consultar la memoria compartida"); //Si falla mostramos un error y desconectamos la memoria que habiamos conectado
        shmdt(direccion);
        return NULL;
    }

    //datos.shm_segsz contiene el tamano real del segmento en bytes, y lo guardamos en la variable que recibio la funcion por puntero
    *tamano = datos.shm_segsz;


    //Devolvemos la direccion donde se ha conectado el segmento de memoria compartida
    return direccion;



}



//Funcion que mapea un fichero de memoria. (permite acceder al contenido de un archivo como si estuviera en la RAM)
static void *mapearFichero(char *fichero, int proteccion, int *descriptor, size_t *tamano){

    int fd;                 //Descriptor del fichero
    int modo = O_RDONLY;    //Modo de apertura, inicialmente solo lectura
    struct stat datos;      //Estructura que almacena informacion del fichero
    void *direccion;        //Puntero a la memoria mapeada

    //Comprobamos si se ha solicitado permiso de escritura
    if (proteccion & PROT_WRITE){
        modo = O_RDWR;
    }


    //Consultamos informacion del fichero, si falla devovvlemos error
    if (stat(fichero, &datos) == -1){
        perror("Error al consultar el fichero");
        return NULL;
    }


    //Comprobamos que el fichero no este vacio, si lo esta(0 bytes) mandamos un mensaje de error
    if (datos.st_size == 0) {

        printf("No se puede mapear un fichero vacio\n");
        return NULL;
    }

    //Abrimos el fichero utilizando los permisos establecidos anteriormente
    fd = open(fichero,modo);


    //Si no consigue abrirlo lanza un mensaje de error
    if (fd == -1){
        perror("Error al abrir el fichero");
        return NULL;
    }


    //Creamos un mapeo del contenido del fichero en el espacio de memoria del proceso
    direccion = mmap(NULL, datos.st_size, proteccion,MAP_PRIVATE,fd,0);         //MAP_PRIVATE significa que si modificamos la memoria mapeada, los cambios no se guardaran en el archivp original


    //Si mmap falla, devolvvemos un error y cerramos el descriptor
    if (direccion == MAP_FAILED){
        perror("Error al mapear el fichero");
        close(fd);
        return NULL;
    }

    //Guardamos el descriptor y el tamano
    *descriptor = fd;
    *tamano = (size_t) datos.st_size;


    //Devolvvemos la direccion de la memoria mapeada
    return direccion;

}



//Funcion que lee bytes de un fichero y los guarda en direccion de memoria
static ssize_t leerFichero(char *fichero, void *direccion, size_t cantidad){


    //Declaramos variables
    struct stat datos;
    ssize_t leidos;
    int fd;
    int error;


    //Consultamos la informacion del fichero
    if(stat(fichero,&datos) == -1){
        return -1;
    }

    //Abrimos el fichero en modo lectura
    fd = open(fichero,O_RDONLY);

    if(fd == -1){
        return -1;
    }


    //Si no se indica cantidad, leeremos el fichero completo
    if (cantidad == (size_t) -1) {

        cantidad = (size_t) datos.st_size;
    }

    //Leemos los bytes y los guardamos en la direccion indicada
    leidos = read(fd, direccion, cantidad);

    if (leidos == -1) {
        error = errno;
        close(fd);
        errno = error;
        return -1;
    }

    //Cerramos el fichero
    close(fd);

    return leidos;
}


//Funcion que escribe bytes de memoria en un fichero
static ssize_t escribirFichero(char *fichero, void *direccion,size_t cantidad, int sobrescribir) {

    int fd;
    int flags;
    ssize_t escritos;
    int error;


    //Creamos el fichero solo si no existe
    flags = O_WRONLY | O_CREAT | O_EXCL;

    //Con -o permitimos sobrescribir un fichero existente
    if (sobrescribir) {
        flags = O_WRONLY | O_CREAT | O_TRUNC;
    }

    //Abrimos el fichero con los permisos indicados
    fd = open(fichero, flags, 0666);


    if (fd == -1) {

        return -1;
    }


    //Escribimos los bytes de memoria en el fichero
    escritos = write(fd, direccion, cantidad);

    if (escritos == -1) {
        error = errno;
        close(fd);
        errno = error;
        return -1;
    }

    //Cerramos el fichero
    if (close(fd) == -1) {

        return -1;
    }

    return escritos;
}



//Comando i-o
    //Permite leer o escribir datos entre ficheros y memoria

    void Cmd_io(char *trozos[]) {

    
        if (trozos[1] != NULL && strcmp(trozos[1], "read") == 0) {

            //Declaramos variables
            char *fichero;
            void *direccion = NULL;
            size_t cantidad = (size_t) -1;
            ssize_t leidos;

            
            if (trozos[2] == NULL || trozos[3] == NULL) {

                printf("Uso: i-o read fichero direccion [cantidad]\n");
                return;
            }

           fichero = trozos[2];

            //Convertimos la direccion escrita a un puntero

            if (sscanf(trozos[3], "%p", &direccion) != 1) {

                printf("Direccion de memoria no valida\n");
                return;
            }


            //Si el usuario indica cantidad, la convertimos a numero
            if (trozos[4] != NULL) {

                cantidad = strtoull(trozos[4], NULL, 10);
            }


            //Leemos el fichero utilizando la funcion
            leidos = leerFichero(fichero, direccion, cantidad);


            if (leidos == -1) {

                perror("Error al leer el fichero");
                return;
            }

            printf("Leidos %zd bytes de %s en %p\n",leidos, fichero, direccion);
            return;
    }

    //i-o write
        //Escribe en un fichero los bytes almacenados en memoria

        if (trozos[1] != NULL && strcmp(trozos[1], "write") == 0) {

        char *fichero;
        void *direccion = NULL;
        size_t cantidad;
        ssize_t escritos;
        int sobrescribir = 0;
        int posicion = 2;

        //Comprobamos si el usuario ha indicado la opcion -o
        if (trozos[2] != NULL && strcmp(trozos[2], "-o") == 0) {

            sobrescribir = 1;
            posicion = 3;
        }

        //Comprobamos que se hayan indicado todos los parametros
        if (trozos[posicion] == NULL || trozos[posicion + 1] == NULL || trozos[posicion + 2] == NULL) {

            printf("Uso: i-o write [-o] fichero direccion cantidad\n");
            return;
        }


        fichero = trozos[posicion];

        //Convertimos la direccion escrita por el usuario a un puntero
        if (sscanf(trozos[posicion + 1], "%p", &direccion) != 1) {

            printf("Direccion de memoria no valida\n");
            return;
        }


        //Convertimos la cantidad de bytes a un numero
        cantidad = strtoull(trozos[posicion+2], NULL, 10);


        //Escribimos los datos en el fichero 
        escritos = escribirFichero(fichero, direccion, cantidad, sobrescribir);

        if (escritos == -1) {

            perror("Error al escribir el fichero");
            return;
        }

        printf("Escritos %zd bytes en %s desde %p\n",escritos, fichero, direccion);

        return;
        }

    printf("Opcion de i-o no reconocida\n");
}



    //Comando read
        //Lee bytes de un descriptor abierto y los guarda en memoria

        void Cmd_read(char *trozos[]) {

            int descriptor;
            void *direccion = NULL;
            size_t cantidad;
            ssize_t leidos;
            char *fin;

            
            if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] == NULL) {

                printf("Uso: read descriptor direccion cantidad\n");
                return;
            }

            //Convertimos el descriptor a un numero entero
            descriptor = (int) strtol(trozos[1], &fin, 10);

            if (*fin != '\0' || descriptor < 0) {

                printf("Descriptor no valido\n");
                return;
            }

            //Convertimos la direccion de texto a un puntero
            if (sscanf(trozos[2], "%p", &direccion) != 1) {

                printf("Direccion de memoria no valida\n");
                return;


            }

            
            cantidad = strtoull(trozos[3], NULL, 10);


            //Leemos desde el descriptor directamente a memoria
            leidos = read(descriptor, direccion, cantidad);


            if (leidos == -1) {

                perror("Error al leer del descriptor");
                return;
            }

    printf("Leidos %zd bytes del descriptor %d en %p\n",leidos, descriptor, direccion);

    
}







//Se encarga de reservar memoria
void Cmd_allocate(char *trozos[]){

    //Si el usuario escribe allocate sin argumentos, mostramos los bloques de memoria
    if(trozos[1] == NULL){
        mostrarBloquesMemoria(&lista);
        return;
    }



    //allocate -malloc:

        size_t tamano;      // Cant. de bytes que queremos reservar
        void *direccion;    // Puntero que guarda la direccion de memoria reservada
        BloqueMemoria bloque = {0};     // Inicializamos todos los campos de la estructura a 0


        // Comprobamos que existe un parametro y que es malloc
        if (trozos[1] != NULL && strcmp(trozos[1], "-malloc") == 0 ){


             //Comprobamos si se indico un tamaño o no
            if (trozos[2] == NULL){
                printf("Uso: allocate -malloc tamano\n");       // Si el usuario solo escribe (allocate -malloc) mostramos un mensaje de error
                return;
            }

       
            tamano = strtoull(trozos[2],NULL,10);           // Convertimos la cadena de caracteres que se ha introducido en un numero entero. Ej: "1024"->1024
        
            
            //Si el usuario quiere resevar 0 bytes devolvemos un mensaje de error
            if (tamano == 0){
                printf("\nEl nº de bytes a reservar debe ser mayor que 0\n");
                return;
            }

            direccion = malloc(tamano); // Guardamos en direccion un puntero a la memoria reservada


            // Si malloc falla devolvemos NULL
            if (direccion == NULL) {
                perror("Error al reservar memoria");
                return;
            }


            //Guardamos la informacion del bloque
            bloque.direccion = direccion;
            bloque.tamano = tamano;
            bloque.fecha = time(NULL);
            strcpy(bloque.tipo, "malloc");



            // Si la funcion falla al insertar el bloque de memoria en la lista, liberamos la memoria reservada
            if (!insertarBloqueMemoria(&lista,bloque)){
                free(direccion);
                printf("Error al registrar el bloque de memoria \n");
                return;
            }
        

            printf("Asignados %zu bytes en %p \n", tamano, direccion);
        }


    
    //allocate -createshared:
    
        if (trozos[1] != NULL && strcmp(trozos[1], "-createshared") == 0){


            key_t clave;
            size_t tamano;
            void *direccion;
            BloqueMemoria bloque = {0};


            //Comprobamos que el usario ha introducido la clave y el tamano
            if (trozos[2] == NULL || trozos[3] == NULL){
                printf("Uso: allocate -createshared clave tamano\n");
                return;

            }

            clave = (key_t) strtol(trozos[2],NULL,10);      //Convertimos la cadena de la clave a un numero entero. (El 10 significa que utilizamos base decimal)
            tamano = strtoull(trozos[3],NULL,10);            //Convertimos el tamano a un entero sin signo


            if (clave == IPC_PRIVATE || tamano == 0){   //Rechazamos que la clave sea la clave especial IPC_PRIVATE(usualmente 0) y que el tamano sea de 0 bytes
                printf("Clave o tamano no valido \n");
                return;
            }


            //Creamos la memoria compartida llamando a la funcion
            direccion = crearMemoriaCompartida(clave,tamano);

            if (direccion == NULL){
                return;
            }

            
            bloque.direccion = direccion;
            bloque.tamano = tamano;
            bloque.fecha = time(NULL);
            strcpy(bloque.tipo,"shared");
            bloque.clave = clave;

            if (!insertarBloqueMemoria(&lista, bloque)) {
                int id = shmget(clave, 0, 0666);    //Recuperamos el identificador del segmento que acabamos de crear.

               shmdt(direccion);                    //Desconectamos el segmento de nuestra shell

                if (id != -1) {
                    if (shmctl(id, IPC_RMID, NULL) == -1) {     //Identificamos el segmento y lo marcamos para eliminarlo. Si falla el borrado mandamos un error
                        perror("Error al eliminar el segmento compartido");
                    }
                }   
                    else {                                      // Si ni siquiera lo da indentificado mandamos otro error
                        perror("Error al obtener el segmento compartido");
                    }

                printf("Error al registrar el bloque\n");
                return;
            }


            printf("Memoria compartida de %zu bytes en %p (clave %d)\n",tamano, direccion, (int) clave);
            return;
        }

    //allocate -shared
        //(Comando que permite conectar nuestra shell a un segmento de memoria compartida)

        if (trozos[1] != NULL && strcmp(trozos[1], "-shared") == 0){

            key_t clave;
            size_t tamano;
            void *direccion;
            BloqueMemoria bloque = {0};


            if (trozos[2] == NULL){
                printf("Uso: allocate -shared clave\n");
                return;
            }

            clave = (key_t) strtol(trozos[2],NULL,10);

            if (clave == IPC_PRIVATE){
                printf("Clave no valida\n");
                return;
            }

            //Conectamos nuestra shell al segmento de memoria compartida existente
            direccion = conectarMemoriaCompartida(clave, &tamano);

            if (direccion == NULL){
                return;
            }

            //Guardamos la informacion del bloque
            bloque.direccion = direccion;
            bloque.tamano = tamano;
            bloque.fecha = time(NULL);
            strcpy(bloque.tipo, "shared");
            bloque.clave = clave;

            if (!insertarBloqueMemoria(&lista,bloque)){
                
                shmdt(direccion);
                printf("Error al registrar el bloque de memoria\n");
                return;
            }

            printf("Memoria compartida de clave %d conectada en %p (%zu bytes)\n", (int) clave, direccion, tamano);


            return;



        }


    ///allocate -mmap
        //Mapea un fichero en memoria con los permisos indicados por el usuario

        if (trozos[1] != NULL && strcmp(trozos[1], "-mmap") == 0) {

            char *fichero;                  //Nombre del archivo que queremos mapear
            char *permisos;
            int proteccion = 0;             //Almacena los permisos de memoria
            int descriptor;
            size_t tamano;
            void *direccion;                //Direccion donde se mapeara el fichero
            BloqueMemoria bloque = {0};

    
            //Si el usuario no indico un fichero, mostramos un mensaje de error
            if (trozos[2] == NULL) {
                printf("Uso: allocate -mmap fichero permisos\n");
                return;
            }

            //Obtenemos el nombre del fichero y los permisos del mismo, y los guardamos
            fichero = trozos[2];
            permisos = trozos[3];

            //Interpretamos los permisos indicados por el usuario
            if (permisos != NULL && strlen(permisos) < 4) {

                if (strchr(permisos, 'r') != NULL) {        //Busca el caracter r y si lo encuentra añade permisos de lectura

                    proteccion |= PROT_READ;
                }

                if (strchr(permisos, 'w') != NULL) {        //Busca el caracter w y si lo encuentra añade permisos de escritura
                    proteccion |= PROT_WRITE;
                }

                if (strchr(permisos, 'x') != NULL) {        //Busca el caracter x y si lo encuentra añade permisos de ejecucion
                    proteccion |= PROT_EXEC;
                }
            }       

            //Mapeamos el fichero utilizando la funcion 
            direccion = mapearFichero(fichero, proteccion, &descriptor, &tamano);


            if (direccion == NULL) {
                
                return;
            }

            //Guardamos la informacion del bloque
            bloque.direccion = direccion;
            bloque.tamano = tamano;
            bloque.fecha = time(NULL);
            strcpy(bloque.tipo, "mmap");
            bloque.descriptor = descriptor;

            strncpy(bloque.fichero, fichero, sizeof(bloque.fichero) - 1);   //Copiamos el nombre del fichero a la estructura


            if (!insertarBloqueMemoria(&lista, bloque)) {

            //Si falla la insercion, deshacemos el mapeo de memoria y cerramos el descriptor del fichero
            munmap(direccion, tamano);
            close(descriptor);

            printf("Error al registrar el bloque de memoria\n");
            return;
            }


    printf("Fichero %s mapeado en %p (%zu bytes)\n",fichero, direccion, tamano);
    return;
}
        
        




}


//Se encarga de liberar memoria
void Cmd_deallocate(char *trozos[]){


    //Si el usuario escribe deallocate sin ningun parametro, mostramos los bloques de memoria
    if (trozos[1] == NULL){
        mostrarBloquesMemoria(&lista);
        return;
    }



    //deallocate -malloc
        size_t tamano;
        NodoMemoria *actual;

        if (trozos[1] != NULL && strcmp(trozos[1], "-malloc") == 0) {

            if (trozos[2] == NULL) {
                printf("Uso: deallocate -malloc tamano\n");
                return;
            }
        

            tamano = strtoull(trozos[2], NULL, 10);

            actual = lista.primero;    //Hacemos que actual apunte al primer nodo de la lista


            while (actual != NULL){

                //Buscamos el bloque correcto, comprobando que sea de tipo malloc y que tenga el tamaño solicitado
                if (strcmp(actual->bloque.tipo, "malloc") == 0 && actual->bloque.tamano == tamano){

                    //Guardamos en direccion la direccion de memoria del bloque encontrado
                    void *direccion = actual->bloque.direccion;


                    //Liberamos la memoria y eliminamos el nodo de la lista
                    free(direccion);
                    eliminarBloqueMemoria(&lista,direccion);

                    printf("Liberados %zu bytes de memoria\n",tamano);
                    return;
                }

            actual = actual->siguiente;
            }

         printf("No se encontro un bloque malloc de %zu bytes\n", tamano);
         return;
        }




    //deallocate -shared
        //(Comando que desconecta un segmento de memoria compartida de nuestra shell mediante su clave y elimina su informacion de la lista)
    
        if (trozos[1] != NULL && strcmp(trozos[1], "-shared") == 0){
            key_t clave;
            NodoMemoria *actual;

            //Mensaje de error si el usuario no introduce una clave
            if (trozos[2] == NULL){
                printf("Uso: deallocate -shared clave\n");
                return;
            }

            clave = (key_t) strtol(trozos[2],NULL,10);  //Convertimos la clave de texto a numero, y despues ese numero al tipo key_t

            actual = lista.primero;

            while(actual != NULL){              //Recorremos el bucle comprabando que el bloque sea de tipo shared y que la clave coincida

                if (strcmp(actual->bloque.tipo, "shared") == 0 && actual->bloque.clave == clave){

                    void *direccion = actual->bloque.direccion; //Obtenemos la direccion de memoria del bloque encontrado

                    //Si fallamos al desconectar la memoria compartida mandamos un mensaje de error
                    if (shmdt(direccion) == -1){
                        perror("Error al desconectar la memoria compartida");
                        return;
                    }

                    //ELiminamos el bloque de la lista
                    eliminarBloqueMemoria(&lista,direccion);

                    printf("Memoria compartida de clave %d desconectada\n",(int) clave);
                    return;

            


                }

                actual = actual -> siguiente;
            }

            printf("No hay bloque de esa clave mapeado en el proceso\n");
            return;

        
        } 
    


    
        
    //deallocate -delkey
        //(Comando que elimina un segmento de memoria compartida del sistema mediante su clave)
        
        if (trozos[1] != NULL && strcmp(trozos[1],"-delkey") == 0){
            key_t clave;
            int id;

            if (trozos[2] == NULL){
                printf("Uso: deallocate -delkey clave\n");
                return;
            }

            clave = (key_t) strtol(trozos[2],NULL,10);

            //Comprobamos que la clave es valida
            if (clave == IPC_PRIVATE){
                printf("Clave no valida\n");
                return;
            }


            //Obtenemos el identifcador del segmento
            id = shmget(clave,0,0666);    


            //Si shmget falla devolvemos un mensaje de error
            if (id == -1){
                perror("Error al obtener memoria compartida");
                return;
            }

            if(shmctl(id,IPC_RMID,NULL) == -1){         //shmtctl permite realizar operaciones sobre un segmento de memoria compartida. id es el identificador del segmento que queremos eliminar e IPC_RMID marca el segmento para su eliminacion
            
                perror("Error al eliminar memoria compartida");
                return;

            }

            printf("Clave %d eliminada del sistema\n", (int) clave);
            return;


        }


    //deallocate -mmap
        //Desmapea un fichero y elimina su informacion de la lista
        
        if (trozos[1] != NULL && strcmp(trozos[1],"-mmap") == 0){

            char *fichero;
            NodoMemoria *actual;

            if(trozos[2] == NULL){
                printf("Uso: deallocate -mmap fichero\n");
                return;
            }

            fichero = trozos[2];
            actual = lista.primero;

            while (actual != NULL){

                //Recorremos la lista comprobando que el bloque sea de tipo mmap y que le nombre del fichero coincida
                if (strcmp(actual->bloque.tipo,"mmap") == 0 && strcmp(actual->bloque.fichero,fichero) == 0){

                    //Obtenemos la informacion del bloque y guardamos los datos
                    void *direccion = actual->bloque.direccion;
                    size_t tamano = actual->bloque.tamano;      //Tamano del mapeo en bytes
                    int descriptor = actual->bloque.descriptor;


                    //Desmapeamos el fichero mediante la direccion donde comienza el mapeo y el numero de bytes que queremos desmapear. Si falla el desmapeo mandamos un error
                    if (munmap(direccion,tamano) == -1){
                        perror("Error al desmapear el fichero");
                        return;
                    }

                    
                    //Cerramos el descriptor
                    if(close(descriptor) == -1){
                        perror("Error al cerrar el fichero");
                    }

                    eliminarBloqueMemoria(&lista,direccion);

                    printf("Fichero %s desmapeado correctamente\n",fichero);
                    return;

                }

                actual = actual->siguiente;
            }

            printf("No se encontro el fichero %s mapeado en memoria\n",fichero);
            return;

        }


    //deallocate direccion
        //Comando que permite liberar un bloque de memoria indicando directamente su direccion
        
        if (trozos[1] != NULL && trozos[1][0] != '-'){  //COmprobamos que existe un argumento y que el primer caracter de este no sea un guion

            void *direccion = NULL;
            NodoMemoria *actual;

        
            //Convertimos el texto en una direccion
            if(sscanf(trozos[1], "%p",&direccion) != 1){        //sscanf lee datos de una cadena y los convierte a otro tipo. %p es que queremos leer un puntero
                printf("Direccion de memoria no valida\n");
                return;
            }

            //Buscamos el bloque correspondiente en la lista
            actual = buscarBloqueMemoria(&lista,direccion);

            //Si no se encuentra el bloque mandamos un error
            if (actual == NULL){
                printf("No se encontro ningun bloque en esa direccion\n");
                return;
            }

            //Comprobamos si es memoria malloc
            if(strcmp(actual->bloque.tipo,"malloc") == 0){
                free(direccion);                 //Liberamos la memoria reservada
            }

            //Comprobamos si es memoria compartida
            else if(strcmp(actual->bloque.tipo,"shared") == 0){

                if(shmdt(direccion) == -1){     //Desconectamos la memoria compartida del proceso
                    perror("Error al desconectar la memoria compartida");
                    return;
                }
            }

            //Comprobamos si es memoria mmap
            else if (strcmp(actual->bloque.tipo,"mmap") ==  0){

                if(munmap(direccion, actual->bloque.tamano) == -1){     //Deshacemos el mapeo del fichero de memoria
                    perror("Error al desmapear el fichero");
                    return;
                }

                if(close(actual->bloque.descriptor) == -1){             //Cerramos el descriptor del fichero
                    perror("Error al cerrar el fichero");
                }
            }


            else{
                printf("Tipo de memoria desconocido\n");
                return;
            }


            eliminarBloqueMemoria(&lista,direccion);

            printf("Bloque de memoria en %p liberado\n",direccion);
            return;
        }

        
}

//Muestra información sobre la memoria
void Cmd_memory(char *trozos[]){

    //memory -all
        //Comando que muestra los bloques las funciones y las variables

        if (trozos[1] != NULL && strcmp(trozos[1],"-all") == 0){


            char *argsBlocks[] = {"memory", "-blocks",NULL};
            char *argsFuncs[] = {"memory", "-funcs",NULL};
            char *argsVars[] = {"memory", "-vars",NULL};



            printf("\n---- BLOQUES DE MEMORIA -----\n");
            Cmd_memory(argsBlocks);

            printf("\n---- FUNCIONES-----\n");
            Cmd_memory(argsFuncs);

            printf("\n---- VARIABLES -----\n");
            Cmd_memory(argsVars);



            return;
        }


    //memory -pmap
        //Muestra el mapa de la memoria de nuestro proceso
        
        if (trozos[1] != NULL && strcmp(trozos[1], "-pmap") == 0) {

            pid_t pid;      //Variable donde guardamos el identificador del proceso hijo
            char elpid[32]; //Cadena de caractere donde guardamos el PID de nuestra shell

            //Obtenemos el PID de nuestra shell
            snprintf(elpid,sizeof(elpid),"%d", (int) getpid());

            //Creamos un proceso hijo
            pid = fork();           //fork crea un proceso hijo

            if (pid == -1){
                perror("Error al crear el proceso");
                return;
            }

            if (pid == 0){


                //Intentamos ejecutar pmap
                char *argsPmap[] = {"pmap",elpid,NULL};
                execvp(argsPmap[0],argsPmap);                   //execvp ejecuta un programa externo


                perror("No se pudo ejecutar ninguna herramienta del mapa de memoria");
                _exit(1);

            }

            //El proceso padre espera a que termine el hijo
            waitpid(pid,NULL,0);


        }

    //memory -blocks

        if (trozos[1] == NULL || strcmp(trozos[1], "-blocks") == 0) {
        
            mostrarBloquesMemoria(&lista);
            return; 
        }
        

    //memory -funcs
        //Comando que muestra las direcciones de 3 funciones de la shell y 3 de la biblioteca
        
        if (trozos[1] != NULL && strcmp(trozos[1],"-funcs") == 0){

            printf("Funciones de la shell:\n"); //Identificamos cada funcion  y convertimos su direccion al tipo de puntero que espera %p en nuestro entorno

            printf("Cmd_allocate:   %p\n", (void *) Cmd_allocate);
            printf("Cmd_deallocate: %p\n", (void *) Cmd_deallocate);
            printf("Cmd_memory:     %p\n", (void *) Cmd_memory);


            printf("\nFunciones de la biblioteca C:\n");

            printf("printf: %p\n", (void *) printf);
            printf("malloc: %p\n", (void *) malloc);
            printf("free:   %p\n", (void *) free);


            return;


        }


    //memory -vars    
        //Comando que muestra la direccion de variables automaticas, estaticas y externas

        if (trozos[1] != NULL && strcmp(trozos[1], "-vars") == 0){


            //Variables autmaticas inicializadas
            int automatica1 = 1;
            int automatica2 = 2;
            int automatica3 = 3;


            //Variables automaticas sin inicializar
            int automatica4;
            int automatica5;
            int automatica6;


            //Variables estaticas inicializadas
            static int estatica1 = 10;
            static int estatica2 = 20;
            static int estatica3 = 30;


            //Variables estaticas sin inicializar
            static int estatica4;
            static int estatica5;
            static int estatica6;


            printf("Variables automaticas inicializadas:\n");

            printf("automatica1: %p\n", (void *) &automatica1);
            printf("automatica2: %p\n", (void *) &automatica2);
            printf("automatica3: %p\n", (void *) &automatica3);


    
            printf("\nVariables automaticas sin inicializar:\n");

            printf("automatica4: %p\n", (void *) &automatica4);
            printf("automatica5: %p\n", (void *) &automatica5);
            printf("automatica6: %p\n", (void *) &automatica6);



            printf("\nVariables estaticas inicializadas:\n");

            printf("estatica1: %p\n", (void *) &estatica1);
            printf("estatica2: %p\n", (void *) &estatica2);
            printf("estatica3: %p\n", (void *) &estatica3);



            printf("\nVariables estaticas sin inicializar:\n");

            printf("estatica4: %p\n", (void *) &estatica4);
            printf("estatica5: %p\n", (void *) &estatica5);
            printf("estatica6: %p\n", (void *) &estatica6);



            printf("\nVariables externas:\n");

            printf("externa1: %p\n", (void *) &externa1);
            printf("externa2: %p\n", (void *) &externa2);
            printf("externa3: %p\n", (void *) &externa3);

            return;

        }




}