#ifndef MEMORIA_H
#define MEMORIA_H   // Para evitar duplicaciones, comprobamos si la cabecera ya se ha incluido y despues la definimos

// Declaramos las tres nuevas funciones que despues implementaremos en memoria.c

void Cmd_allocate(char *trozos[]);
void Cmd_deallocate(char *trozos[]);
void Cmd_memory(char *trozos[]);


void inicializarMemoria(void);
void Cmd_io(char *trozos[]);
void Cmd_read(char *trozos[]);

#endif