#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memoria.h"
#include "listaMemoria.h"


static ListaMemoria lista;  //static hace que lista solo sea accesible desde memoria.c

//Inicializamos la variable lista que hemos creado
void inicializarMemoria(void){

    inicializarListaMemoria(&lista);    // Utilizamos la funcion de listaMemoria.c
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
}

//Muestra información sobre la memoria
void Cmd_memory(char *trozos[]){




}