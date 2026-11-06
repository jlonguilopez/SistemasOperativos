#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>      //Define tipos utilizados por llamadas al sistema
#include <sys/ipc.h>        //Proporciona las claves y constantes para comunicacion entre procesos
#include <sys/shm.h>        //Declara las funciones de memoria compartida

#include "memoria.h"
#include "listaMemoria.h"


static ListaMemoria lista;  //static hace que lista solo sea accesible desde memoria.c

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







//Se encarga de reservar memoria
void Cmd_allocate(char *trozos[]){

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
        
        




}


//Se encarga de liberar memoria
void Cmd_deallocate(char *trozos[]){

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


        
}

//Muestra información sobre la memoria
void Cmd_memory(char *trozos[]){


    //memory -blocks

        if (trozos[1] == NULL || strcmp(trozos[1], "-blocks") == 0) {
        
            mostrarBloquesMemoria(&lista);
            return; 
        }   




}