#include "listaMemoria.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

void inicializarListaMemoria(ListaMemoria *lista){      //La funcion recibe un puntero a una lista de memoria para modificarla

    lista->primero = NULL;                       //Indicamos que la lista empieza vacia, ya que el puntero que apunta al primer nodo de la lista lo igualamos a NULL

}



int insertarBloqueMemoria(ListaMemoria *lista, BloqueMemoria bloque){       //La funcion recibe un puntero a la lista que queremos modificar y el bloque de memoria que queremos insertar en ella

    NodoMemoria *nuevo = malloc(sizeof(NodoMemoria));  //Reservamos memoria para un nuevo nodo

    if (nuevo == NULL){             //Si malloc falla devolvemos 0
        return 0;

    }

    nuevo->bloque = bloque;                 //Guardamos una copia del bloque dentro del nuevo nodo
    nuevo->siguiente = lista->primero;      //Hacemos que el nuevo nodo apunte al que antes era el primero
    lista->primero = nuevo;                 //El nuevo nodo pasa a ser el primero de la lista

    return 1;                               //Indica que la insercion se hizo correctamente


    /*El ultimo bloque que se ha insertado siemore sera el primero de la lista*/

}


//Funcion que busca un bloque de memoria en la lista mediante su direccion y devuelve el nodo si la encuentra
NodoMemoria *buscarBloqueMemoria(ListaMemoria *lista, void *direccion){

    NodoMemoria *actual = lista->primero;       //Creamos un puntero llamdado actual que apunte al primer nodo de la lista

    while (actual != NULL){         // Recorremos la lista

        if (actual->bloque.direccion == direccion){ // Cuando la direccion buscada coincida devolvemos el nodo de esa posicion 
            return actual;
        }

        actual = actual->siguiente;     // Hacemos que actual apunte al siguiente nodo de la lista para avanzar en la lista

    }

    return NULL;                    // Devolvemos NULL si no se encuentra la direccion


}


//Funcion que busca un bloque de memoria en la lista mediante su direccion y lo elimina
int eliminarBloqueMemoria(ListaMemoria *lista, void *direccion){

    NodoMemoria *actual = lista->primero;       // "actual" apunta al primer nodo de la lista y "anterior" apunta al nodo anterior a actual
    NodoMemoria *anterior = NULL;

    while (actual != NULL){

        if (actual->bloque.direccion == direccion){



            // Si el nodo que queremos borrar es el primero, hacemos que el segundo nodo pase a ser el primero de la lista
            if (anterior == NULL){

                lista->primero = actual->siguiente;
            }

            else {

                // So el nodo que queremos eliminar no es el primero, hacemos que el nodo anterior apunte directamente al siguiente
                anterior->siguiente = actual->siguiente;
            }


            free(actual);   //Liberamos la memoria del nodo eliminado
            return 1;

        }

        //Avanzamos cada variable al siguiente nodo
        anterior = actual;
        actual = actual->siguiente;
    }

    return 0;  // Si no se encuentra la direccion devolvemos 0


}

//Funcion que elimina todos los nodos de la lista
void vaciarListaMemoria(ListaMemoria *lista){


    NodoMemoria *actual = lista->primero;
    NodoMemoria *siguiente;

    while (actual != NULL){ //Recorremos la lista para ir eliminando todos los nodos


        siguiente = actual->siguiente;      // Guardamos la direccion del siguiente nodo
        free(actual);                       // Liberamos la memoria del nodo actual
        actual = siguiente;                 // Avanzamos al siguiente nodo


    }


    lista->primero = NULL;          //Indicamos la lista como vacia
}


//Funcion que recorre la lista y muestra la información de todos los bloques registrados
void mostrarBloquesMemoria(ListaMemoria *lista){
    

    NodoMemoria *actual = lista->primero;   

    while (actual != NULL){

        //Obtenemos el bloque de memoria almacenado en el nodo actual
        BloqueMemoria *bloque = &actual->bloque;    //Usamos & porque queremos acceder directamente al bloque original

        char *fecha = ctime(&bloque->fecha);    //Convertimos la fecha en texto, encontramos la posicion del salto de linea, sustituimos ese salto por el final de la cadena y mostramos todo en una sola linea
        fecha[strcspn(fecha, "\n")] = '\0';

        //Mostramos la información del bloque de memoria(Su direccion de memoria, el tamano del bloque en bytes,la fecha en la que se reservo la memoria para ese bloque y el tipo de memoria)
        printf("%p %zu bytes %s %s\n", bloque->direccion, bloque->tamano, fecha,bloque->tipo);

        actual = actual->siguiente;
    }
}






