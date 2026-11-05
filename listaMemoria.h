//En este archivo definimos la estructura de un bloque de memoria
#ifndef LISTAMEMORIA_H
#define LISTAMEMORIA_H


#include <stddef.h>
#include <time.h>

typedef struct{

    void *direccion;    //Direccion de memoria del bloque
    size_t tamano;      //Tamano del bloque en bytes
    time_t fecha;       //Fecha de asignacion de la memoria
    char tipo[16];      //Tipo de memoria(malloc,shared,mmap)
    int clave;          //Clave de memoria compartida
    int descriptor;     //Descriptor de fichero
    char fichero[256];   //Nombre del fichero 



}BloqueMemoria;

//Creamos el tipo de dato BloqueMemoria para guardar informacion sobre los bloques de memoria de la shell



//Usamos una lista enlazada simple que nos permita agregar y eliminar bloques de memoria sin poner un limite fijo


typedef struct NodoMemoria {     //NodoMemoria representa un elemento de nuestra lista

    BloqueMemoria bloque;               //"bloque" es toda la informacion de una reserva de memoria (tamano,fecha,descriptor...)
    struct NodoMemoria *siguiente;      //"siguiente" es un puntero al siguiente nodo de la lista

}NodoMemoria;



typedef struct {                //ListaMemoria representa la lista completa.

    NodoMemoria *primero;       //"primero apunta al primer nodo de la lista"

}ListaMemoria;




//Prototipos

void inicializarListaMemoria(ListaMemoria *lista);

int insertarBloqueMemoria(ListaMemoria *lista, BloqueMemoria bloque);

NodoMemoria *buscarBloqueMemoria(ListaMemoria *lista, void *direccion);

int eliminarBloqueMemoria(ListaMemoria *lista, void *direccion);

void vaciarListaMemoria(ListaMemoria *lista);

void mostrarBloquesMemoria(ListaMemoria *lista);

#endif