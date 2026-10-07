/*
 * AUTORES DE LA P1
 * Julián Longarela López  | julian.longarela@udc.es
 * Manuel Roel Porca       | manuel.roelp@udc.es
 */


//librerias grupo 1:Comandos de información
#include <stdio.h>
#include <string.h>
#include <unistd.h>   // es necesaria para el getpid() y el getppid(), tambien nos valdrá para el chdir
#include <time.h>     //nos vale para el date
#include <sys/utsname.h>  //para uname(), la usa sysinfo
#include <stdlib.h>   //para exit(), la usa Cmd_fin 
#include <errno.h>    //para errno, donde el sistema deja el motivo del ultimo error (lo usamos con strerror)
//librerias grupo 2: ficheros abiertos
#include <fcntl.h>    //para open(), fcntl() y las constantes
//librerias grupo 3: crear y borrar
#include <sys/stat.h> //para mkdir() y lstat(), que nos dice si algo es fichero o directorio
#include <dirent.h>   //para opendir(), readdir() y closedir(), recorrer lo que hay dentro de un directorio
//librerias grupo 4: listar
#include <pwd.h>      //para getpwuid(), pasar el numero de usuario a su nombre
#include <grp.h>      //para getgrgid(), pasar el numero de grupo a su nombre

#define MAX_TEXT 2048
#define MAX_NOMBRE 1024
#define MAX_ABIERTOS 1024   //maximo de descriptores que guardamos en la lista



//avisamos al compilador de que estas funciones existen, porque la tabla las usa antes de que esten escritas mas abajo
//grupo 1
void Cmd_authors (char *trozos[]);
void Cmd_pid (char *trozos[]);
void Cmd_date (char *trozos[]);
void Cmd_chdir (char *trozos[]);
void Cmd_sysinfo (char *trozos[]);
void Cmd_help (char *trozos[]);
void Cmd_fin (char *trozos[]);
//grupo 2
void Cmd_listopen (char *trozos[]);
void Cmd_open (char *trozos[]);
void Cmd_close (char *trozos[]);
void Cmd_dup (char *trozos[]);
void Cmd_lseek (char *trozos[]);
void Cmd_readstr (char *trozos[]);
void Cmd_writestr (char *trozos[]);
//grupo 3
void Cmd_makefile (char *trozos[]);
void Cmd_makedir (char *trozos[]);
void Cmd_delete (char *trozos[]);
void Cmd_deltree (char *trozos[]);
//grupo 4
void Cmd_listfile (char *trozos[]);
void Cmd_list (char *trozos[]);


//cada fila tiene el nombre, la funcion que lo ejecuta y su ayuda para el help
struct COMANDO {
  char *nombre;  //nombre del comando
  void (*funcion)(char **);  //puntero a la funcion que ejecuta el comando
  char *texto;   //lo que explica help sobre ese comando designado 
};

struct COMANDO comandos[] = {   //ponemos cada comando, su funcion y su informacion de ayuda en la tabla
  {"authors", Cmd_authors, "[-l|-n]\tMuestra los nombres y/o logins de los autores"},
  {"pid",     Cmd_pid,     "[-p]\tMuestra el pid del shell o de su proceso padre"},
  {"date",    Cmd_date,    "[-d|-t]\tMuestra la fecha y/o la hora actual"},
  {"chdir",   Cmd_chdir,   "[dir]\tCambia (o muestra) el directorio actual del shell"},
  {"sysinfo", Cmd_sysinfo, "\tMuestra informacion de la maquina donde corre el shell"},
  {"help",    Cmd_help,    "[cmd]\tMuestra ayuda sobre los comandos"},
  {"exit",    Cmd_fin,     "\tTermina la ejecucion del shell"},
  {"bye",     Cmd_fin,     "\tTermina la ejecucion del shell"},
  {"listopen",Cmd_listopen, "\tLista los ficheros abiertos del shell"},
  {"open",    Cmd_open,     "[fich] [cr|ex|ro|wo|rw|ap|tr]\tAbre un fichero y lo pone a la lista (sin argumentos lista los abiertos)"},
  {"close",   Cmd_close,   "df\tCierra el descriptor df y lo quita de la lista"},
  {"dup",     Cmd_dup,     "df\tDuplica el descriptor df y lo anade a la lista"},
  {"lseek",   Cmd_lseek,   "df pos ref\tMueve la posicion de df a pos (ref: SEEK_SET, SEEK_CUR o SEEK_END)"},
  {"readstr", Cmd_readstr, "df cont\tLee cont bytes de df y los muestra"},
  {"writestr",Cmd_writestr,"df str\tEscribe el texto str en df"},
  {"makefile",Cmd_makefile,"nombre\tCrea un fichero vacio"},
  {"makedir", Cmd_makedir, "nombre\tCrea un directorio"},
  {"delete",  Cmd_delete,  "n1 n2 ...\tBorra ficheros o directorios vacios"},
  {"deltree", Cmd_deltree, "n1 n2 ...\tBorra ficheros o directorios con todo lo que tengan dentro"},
  {"listfile",Cmd_listfile,"[-long][-link][-acc] n1 n2 ...\tInformacion de ficheros o directorios"},
  {"list",    Cmd_list,    "[-reca][-recb][-hid][-long][-link][-acc] n1 n2 ...\tLista el contenido de directorios"},
  {NULL, NULL, NULL}  //marcamos el final de la tabla con los NULL para que sepa el bucle donde termina la tabla 
};



//cada fichero abierto del shell con su nombre y su modo de apertura
struct FICHERO {
  char nombre[MAX_NOMBRE];   //nombre del fichero si esta vacio, ese descriptor esta cerrado
  int modo;                  //flags con los que se abre (O_RDONLY, O_CREAT...)
};

//la posicion del array ES el descriptor (abiertos[3] es el df 3),al ser global empieza toda a 0, es decir, con todos los nombres vacios
struct FICHERO abiertos[MAX_ABIERTOS];



//definir la función que trocea esa cadena en tokens, para dividirla en chars simples, esta función la presenta el enunciado tal cual
int TrocearCadena(char *cadena, char *trozos[]){
  int i=1;
  if ((trozos[0] = strtok(cadena, "\n \t")) == NULL )
    return 0;
  while ((trozos[i]=strtok(NULL, "\n \t"))!=NULL)
    i++;
  return i;
}



//al arrancar, metemos en la lista los 3 descriptores que hereda el shell de su padre, me refiero a lo de 0,1,2 respectivamente, teclado pantalla y errores
void InicializarAbiertos (void) {
  strcpy(abiertos[0].nombre, "entrada estandar"); //0:teclado
  strcpy(abiertos[1].nombre, "salida estandar");  //1:pantalla
  strcpy(abiertos[2].nombre, "error estandar"); //2:errores
  abiertos[0].modo = fcntl(0, F_GETFL);   //fcntl con F_GETFL nos dice con que modo esta abierto
  abiertos[1].modo = fcntl(1, F_GETFL);
  abiertos[2].modo = fcntl(2, F_GETFL);
}

//escribe en texto los modos de apertura
void ModoTexto (int modo, char *texto) {
  texto[0] = '\0';   //empezamos con el texto vacio
  switch (modo & O_ACCMODE) {   //O_ACCMODE se queda solo con la parte de lectura/escritura
    case O_RDONLY: strcat(texto, "O_RDONLY"); break;
    case O_WRONLY: strcat(texto, "O_WRONLY"); break;
    case O_RDWR:   strcat(texto, "O_RDWR");   break;
  }
  if (modo & O_CREAT)  strcat(texto, " O_CREAT");    //el & mira si ese modo esta activado para todos
  if (modo & O_EXCL)   strcat(texto, " O_EXCL");
  if (modo & O_APPEND) strcat(texto, " O_APPEND");
  if (modo & O_TRUNC)  strcat(texto, " O_TRUNC");
}


//pasa el texto de un descriptor a numero, si no es un numero valido o se sale del array devuelve -1
int LeerDescriptor (char *texto) {
  int df;

  if (texto[0] < '0' || texto[0] > '9') {   //si no empieza por un numero no vale (asi close patata no cierra el 0)
    printf("Descriptor no valido: %s\n", texto);
    return -1;
  }
  df = atoi(texto);   //atoi pasa el texto a numero
  if (df >= MAX_ABIERTOS) {   //si es muy grande se saldria de nuestro array
    printf("Descriptor no valido: %s\n", texto);
    return -1;
  }
  return df;
}



//escribe el directorio actual (lo usan makefile, makedir, delete, deltree, listfile y list cuando no les damos nombres, como el shell de referencia)
void MostrarDirectorioActual (void) {
  char dir[MAX_NOMBRE];

  if (getcwd(dir, MAX_NOMBRE) == NULL)
    perror("Imposible obtener el directorio actual");
  else
    printf("%s\n", dir);
}


//ponemos la función para gestionar el comando de authors
void Cmd_authors (char *trozos[]){
  if(trozos[1]== NULL) {  //si no hay ni -l ni -n, entonces imprime tanto nombre como logins
    printf("Julian Longarela Lopez: julian.longarela@udc.es \n");
    printf("Manuel Roel Porca: manuel.roelp@udc.es \n");
  } else if (strcmp (trozos[1], "-n") == 0) { //si continua el comando "authors"con -n, imprime solo los nombres
    printf ("Julian Longarela Lopez\nManuel Roel Porca\n");
  } else if (strcmp(trozos[1], "-l") == 0 ) { //si el coando continua con -l, entonces solo pone los logins
    printf("julian.longarela@udc.es\nmanuel.roelp@udc.es\n");
  } else {  //si pone otra cosa, no funciona
    printf("Comando no valido para authors\n");
  }
}

//funcion para gestionar el comando de pid
void Cmd_pid (char *trozos[]) {
  if (trozos [1] != NULL && strcmp(trozos[1], "-p") == 0) {
    printf("Pid del padre del shell: %d\n", getppid());
  } else {
    printf("Pid de shell: %d\n", getpid());
  }
}

//funcion que utilizamos para gestionar el comando date (-d solo nos dará la fecha y -t la hora)
void Cmd_date (char *trozos[]) {
  time_t t= time(NULL);   //hora actual
  struct tm *tm= localtime(&t); //lo pasamos a dia, mes, año
  char fecha[32], hora[32];

  if (tm == NULL){  //salta un error por si el localtime puede fallar
    perror ("No es posible encontrar la fecha");
    return;
  }
  strftime (fecha, sizeof(fecha), "%d/%m/%Y", tm);  // escribimos la fecha en texto
  strftime (hora, sizeof(hora), "%H:%M:%S", tm);  //ponemos la hora en otro

  if (trozos[1] == NULL) {  //si nos pide fecha y hora
    printf ("%s\n%s\n", hora, fecha);   //el shell de referencia saca primero la hora y luego la fecha
  } else if (strcmp(trozos[1], "-d")== 0) { //solo la fecha
    printf("%s\n", fecha );
  } else if (strcmp(trozos[1], "-t")== 0) { //solo la hora
    printf("%s\n", hora );
  } else {
    printf("Opcion no valida para date\n");
  }
}


//funcion para gestionar el comando chdir (mostrar el directorio actual)
void Cmd_chdir (char *trozos[]) {
  char dir[MAX_NOMBRE];
  
  if (trozos[1] == NULL ) {   //mostraremos donde estamos 
    if (getcwd(dir, MAX_NOMBRE) == NULL)
    
      perror("Imposible obtener el directorio actual");      
    else
      printf("%s\n", dir);
      
  } else if (chdir (trozos[1]) == -1){   //si tiene un argumento lo intentamos cambiar
      perror("Imposible cambiar directorio");
    }
}


//funcion para el comando sysinfo, muestra la informacion de la maquina
void Cmd_sysinfo (char *trozos[]) {
  (void) trozos;        //no usamos el argumento, asi no salta un aviso con el Wextra
  struct utsname info;  //aqui uname nos guarda los datos de la maquina

  if (uname(&info) == -1) {  //si falla avisamos al sistema y ponemos el perror
    perror("No es posible obtener la informacion del sistema");
    return;
  }
  //como el shell de referencia: equipo (procesador), OS: sistema-kernel-version
  printf("%s (%s), OS: %s-%s-%s\n", info.nodename, info.machine, info.sysname, info.release, info.version);
}


//funcion para el comando help 
void Cmd_help (char *trozos[]) {
  int i;    //indice para recorrer la tabla 

  if (trozos[1] == NULL) {    //si solo escribe help sin nd mas 
    printf("Comandos disponibles:\n");
    for (i = 0; comandos[i].nombre != NULL; i++)    //recorremos la lista hasta el NULL,NULL
      printf("%s ", comandos[i].nombre);    //imprimimos los nombres de cada comando 
    printf("\n");
    return;
  }

  //si hay argumento , bucar en la tabla
  for (i = 0; comandos[i].nombre != NULL; i++) { //recorremos la tabla    
    if (strcmp(comandos[i].nombre, trozos[1]) == 0) {   //comprobamos si el nombre coincide con el de la fila 
      printf("%s: %s\n", comandos[i].nombre, comandos[i].texto);   //como el shell de referencia: "date: [-d|-t] ..."
      return;    
    }
  }
  //si el comando no esta en la tabla, y no lanza return 
  printf("%s no encontrado\n", trozos[1]);    //si acaba el bucle es que no estaba
}


//funcion para exit y bye, termina el shell
void Cmd_fin (char *trozos[]) {
  (void) trozos;   //no usamos el argumento, asi no salta el aviso con -Wextra, y lo ponemos por la tabla tambien
  exit(0);         // el 0 significa que ha acabado bien
}


//funcion para el comando listopen en el que muestra los ficheros abiertos del shell
void Cmd_listopen (char *trozos[]) {
  (void) trozos;   //no tiene opciones
  int df;
  char modo[128];
  long pos;

  for (df = 0; df < MAX_ABIERTOS; df++) {   //recorremos todas las posiciones
    if (abiertos[df].nombre[0] != '\0') {   //si el nombre no esta vacio, ese df esta abierto
      ModoTexto(abiertos[df].modo, modo);
      pos = lseek(df, 0, SEEK_CUR);   //movernos 0 desde donde estamos = preguntar por donde va el marcapaginas
      if (pos == -1)   //la terminal no tiene posicion, en ese caso lo dejamos en blanco como el shell de referencia
        printf("descriptor: %d, offset: (  )-> %s %s\n", df, abiertos[df].nombre, modo);
      else
        printf("descriptor: %d, offset: (%ld)-> %s %s\n", df, pos, abiertos[df].nombre, modo);
    }
  }
}


//funcion para el comando open(abre un fichero con los modos indicados y lo añade a la lista)
void Cmd_open (char *trozos[]) {
  int i, df, modo = 0;   //modo empieza a 0, que es O_RDONLY si no se indica nada

  if (trozos[1] == NULL) {   //sin argumentos listamos los abiertos
    Cmd_listopen(trozos);
    return;
  }

  //a partir de trozos[2] vienen los modos traducimos cada palabra a su constante y la juntamos con | (poruqe los bits los juntamos con eso )
  for (i = 2; trozos[i] != NULL; i++) {
    if      (strcmp(trozos[i], "cr") == 0) modo |= O_CREAT;
    else if (strcmp(trozos[i], "ex") == 0) modo |= O_EXCL;
    else if (strcmp(trozos[i], "ro") == 0) modo |= O_RDONLY;
    else if (strcmp(trozos[i], "wo") == 0) modo |= O_WRONLY;
    else if (strcmp(trozos[i], "rw") == 0) modo |= O_RDWR;
    else if (strcmp(trozos[i], "ap") == 0) modo |= O_APPEND;
    else if (strcmp(trozos[i], "tr") == 0) modo |= O_TRUNC;
    else break;   //si no es un modo conocido, dejamos de leer modos
  }

  //el 0777 son los permisos que tendra el fichero si se crea con cr
  if ((df = open(trozos[1], modo, 0777)) == -1) {
    perror("Imposible abrir fichero");
    return;
  }
  if (df >= MAX_ABIERTOS) {   //por si el sistema nos da un descriptor que no cabe en nuestro array
    close(df);
    printf("Demasiados ficheros abiertos\n");
    return;
  }

  //lo guardamos en la lista, en la posicion de su descriptor
  strncpy(abiertos[df].nombre, trozos[1], MAX_NOMBRE - 1);
  abiertos[df].nombre[MAX_NOMBRE - 1] = '\0';   //por si el nombre es muy largo, nos aseguramos de que acaba en \0
  abiertos[df].modo = modo;
  printf("Anadida entrada %d a la tabla ficheros abiertos\n", df);
}


//funcion para el comando close, cierra el descriptor que le digamos y lo quita de la lista
void Cmd_close (char *trozos[]) {
  int df;

  if (trozos[1] == NULL) {   //si solo pone close, enseñamos los abiertos
    Cmd_listopen(trozos);
    return;
  }

  if ((df = LeerDescriptor(trozos[1])) == -1)   //si el descriptor no vale salimos
    return;

  if (close(df) == -1) {   //si el sistema no lo puede cerrar, avisamos con perror
    perror("Imposible cerrar descriptor");
    return;
  }
  abiertos[df].nombre[0] = '\0';   //dejamos el nombre vacio, asi en nuestra lista ya sale como cerrado
}


//funcion para el comando dup, duplica un descriptor y mete el nuevo en la lista
void Cmd_dup (char *trozos[]) {
  int df, nuevo;
  char aux[MAX_NOMBRE + 32];   //un poco mas grande que un nombre, porque le añadimos "dup X ( )"

  if (trozos[1] == NULL) {   //si solo pone dup, enseñamos los abiertos
    Cmd_listopen(trozos);
    return;
  }
  if ((df = LeerDescriptor(trozos[1])) == -1)   //si el descriptor no vale salimos
    return;

  if ((nuevo = dup(df)) == -1) {   //le pedimos al sistema otro descriptor para el mismo fichero
    perror("Imposible duplicar descriptor");
    return;
  }
  if (nuevo >= MAX_ABIERTOS) {   //por si no cabe en nuestro array
    close(nuevo);
    printf("Demasiados ficheros abiertos\n");
    return;
  }

  //en la lista le ponemos un nombre que diga de quien es copia, y el mismo modo que el original
  sprintf(aux, "dup %d (%s)", df, abiertos[df].nombre);   //como en el codigo de ayuda de los profesores
  strncpy(abiertos[nuevo].nombre, aux, MAX_NOMBRE - 1);   //como en open: copiamos sin pasarnos
  abiertos[nuevo].nombre[MAX_NOMBRE - 1] = '\0';
  abiertos[nuevo].modo = fcntl(nuevo, F_GETFL);   //como en el codigo de los profesores: le preguntamos el modo al sistema
  printf("Anadida entrada %d a la tabla ficheros abiertos\n", nuevo);
}


//funcion para el comando lseek, mueve la posicion de lectura/escritura de un descriptor
void Cmd_lseek (char *trozos[]) {
  int df, ref;
  long pos, nueva;   //posiciones dentro del fichero

  if (trozos[1] == NULL || trozos[2] == NULL || trozos[3] == NULL) {   //necesitamos los 3 argumentos
    printf("Parametros incorrectos\n");
    return;
  }
  if ((df = LeerDescriptor(trozos[1])) == -1)
    return;

  pos = atol(trozos[2]);   //atol es como atoi pero para long. La posicion puede ser negativa (ej. -5 desde el final)

  //miramos desde donde hay que contar
  if      (strcmp(trozos[3], "SEEK_SET") == 0) ref = SEEK_SET;   //desde el principio
  else if (strcmp(trozos[3], "SEEK_CUR") == 0) ref = SEEK_CUR;   //desde donde estamos ahora
  else if (strcmp(trozos[3], "SEEK_END") == 0) ref = SEEK_END;   //desde el final
  else {
    printf("Referencia no valida: %s\n", trozos[3]);
    return;
  }

  if ((nueva = lseek(df, pos, ref)) == -1) {   //lseek nos devuelve la posicion nueva, o -1 si falla
    perror("Imposible hacer lseek");
    return;
  }
  printf("Descriptor %d posicionado en %ld\n", df, nueva);
}

//funcion para el comando readstr, lee cont bytes de un descriptor y los muestra como texto
void Cmd_readstr (char *trozos[]) {
  int df, cont, leidos;
  char buffer[MAX_TEXT];   //aqui guardamos lo que leemos

  if (trozos[1] == NULL || trozos[2] == NULL) {
    printf("Parametros incorrectos\n");
    return;
  }
  if ((df = LeerDescriptor(trozos[1])) == -1)
    return;

  cont = atoi(trozos[2]);
  if (cont <= 0) {   //tiene que ser un numero positivo
    printf("Parametros incorrectos\n");
    return;
  }
  if (cont > MAX_TEXT - 1)   //no leemos mas de lo que cabe en el buffer (dejamos 1 para el \0)
    cont = MAX_TEXT - 1;

  if ((leidos = read(df, buffer, cont)) == -1) {   //read devuelve cuantos bytes ha leido de verdad, o -1 si falla
    perror("Imposible leer");
    return;
  }
  buffer[leidos] = '\0';   //cerramos el texto para poder imprimirlo con %s
  printf("Leidos %d bytes del descriptor %d\n%s\n", leidos, df, buffer);
}


//funcion para el comando writestr, escribe un texto en un descriptor
void Cmd_writestr (char *trozos[]) {
  int df, i;

  if (trozos[1] == NULL || trozos[2] == NULL) {
    printf("Parametros incorrectos\n");
    return;
  }
  if ((df = LeerDescriptor(trozos[1])) == -1)
    return;

  //el texto puede tener varias palabras (TrocearCadena las separo), asi que las escribimos todas con un espacio entre medias
  for (i = 2; trozos[i] != NULL; i++) {
    if (i > 2) {   //antes de cada palabra menos la primera ponemos un espacio
      if (write(df, " ", 1) == -1) {
        perror("Imposible escribir");
        return;
      }
    }
    if (write(df, trozos[i], strlen(trozos[i])) == -1) {   //write devuelve -1 si falla
      perror("Imposible escribir");
      return;
    }
  }
}


//funcion para el comando makefile, crea un fichero vacio
void Cmd_makefile (char *trozos[]) {
  int df;

  if (trozos[1] == NULL) {   //si no pone nombre no sabemos que crear
    MostrarDirectorioActual();   //sin nombre, como el shell de referencia: enseñamos donde estamos
    return;
  }
  //O_CREAT lo crea y O_EXCL hace que falle si ya existe, asi no machacamos uno que ya habia
  if ((df = open(trozos[1], O_CREAT | O_EXCL | O_WRONLY, 0777)) == -1) {   //0777 como open, el shell de referencia los crea asi
    perror("Imposible crear fichero");
    return;
  }
  close(df);   //solo queriamos crearlo, asi que lo cerramos al momento (no lo metemos en la lista)
}


//funcion para el comando makedir, crea un directorio
void Cmd_makedir (char *trozos[]) {
  if (trozos[1] == NULL) {
    MostrarDirectorioActual();   //sin nombre, como el shell de referencia: enseñamos donde estamos
    return;
  }
  if (mkdir(trozos[1], 0755) == -1)   //0755: nosotros todo, los demas leer y entrar
    perror("Imposible crear directorio");
}


//borra una sola cosa: si es directorio con rmdir (tiene que estar vacio) y si no con unlink
//devuelve 0 si lo borra y -1 si no
int BorrarUno (char *nombre) {
  struct stat datos;   //aqui lstat nos deja la informacion del fichero

  if (lstat(nombre, &datos) == -1) {   //lstat y no stat: si es un enlace miramos el enlace, no a donde apunta
    printf("Imposible borrar %s: %s\n", nombre, strerror(errno));   //strerror(errno) es el motivo del error en texto, el mismo que saca perror
    return -1;
  }
  if (S_ISDIR(datos.st_mode)) {   //S_ISDIR mira en st_mode si es un directorio
    if (rmdir(nombre) == -1) {   //rmdir solo borra directorios vacios
      printf("Imposible borrar %s: %s\n", nombre, strerror(errno));
      return -1;
    }
  } else if (unlink(nombre) == -1) {   //unlink borra ficheros y enlaces
    printf("Imposible borrar %s: %s\n", nombre, strerror(errno));
    return -1;
  }
  return 0;
}


//funcion para el comando delete, borra ficheros o directorios vacios (puede haber varios nombres)
void Cmd_delete (char *trozos[]) {
  int i;

  if (trozos[1] == NULL) {
    MostrarDirectorioActual();   //sin nombre, como el shell de referencia: enseñamos donde estamos
    return;
  }
  for (i = 1; trozos[i] != NULL; i++)   //vamos uno por uno, si uno falla seguimos con el siguiente
    BorrarUno(trozos[i]);
}


//borra algo y, si es un directorio, primero todo lo que tiene dentro (se llama a si misma = recursiva)
//devuelve 0 si lo borra todo y -1 si algo falla
int BorrarArbol (char *nombre) {
  struct stat datos;
  DIR *dir;
  struct dirent *entrada;   //cada cosa que hay dentro del directorio
  char ruta[MAX_NOMBRE];

  if (lstat(nombre, &datos) == -1) {
    printf("Imposible borrar %s: %s\n", nombre, strerror(errno));
    return -1;
  }
  if (!S_ISDIR(datos.st_mode))   //si no es un directorio, es como un delete normal
    return BorrarUno(nombre);

  if ((dir = opendir(nombre)) == NULL) {   //abrimos el directorio para ver lo que tiene
    printf("Imposible borrar %s: %s\n", nombre, strerror(errno));
    return -1;
  }
  while ((entrada = readdir(dir)) != NULL) {   //readdir nos da las cosas de dentro una a una, y NULL al acabar
    //. es el propio directorio y .. el de arriba: si no los saltamos borrariamos todo hacia arriba
    if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0)
      continue;
    //montamos la ruta completa: nombre/lo_de_dentro
    snprintf(ruta, MAX_NOMBRE, "%s/%s", nombre, entrada->d_name);   //snprintf es como printf pero escribe en ruta, y como mucho MAX_NOMBRE letras
    BorrarArbol(ruta);   //aqui esta la recursividad: si dentro hay otro directorio hace lo mismo con el
  }
  closedir(dir);   //importante cerrarlo, si no se queda abierto (y valgrind se queja)

  return BorrarUno(nombre);   //ya esta vacio, ahora si se puede borrar con rmdir
}


//funcion para el comando deltree, borra ficheros o directorios con todo su contenido
void Cmd_deltree (char *trozos[]) {
  int i;

  if (trozos[1] == NULL) {
    MostrarDirectorioActual();   //sin nombre, como el shell de referencia: enseñamos donde estamos
    return;
  }
  for (i = 1; trozos[i] != NULL; i++)
    BorrarArbol(trozos[i]);
}


//las opciones de listfile y list, cada una vale 1 si el usuario la ha puesto y 0 si no
struct OPCIONES {
  int largo;   //-long: una linea con toda la informacion
  int link;    //-link: si es un enlace, tambien a donde apunta
  int acc;     //-acc: fecha de ultimo acceso en vez de la de modificacion
  int hid;     //-hid: tambien los ficheros ocultos (los que empiezan por .)
  int reca;    //-reca: recursivo, entrando en las subcarpetas DESPUES de listar
  int recb;    //-recb: recursivo, entrando en las subcarpetas ANTES de listar
};


//lee las opciones que vienen al principio (desde trozos[1]) y devuelve la posicion del primer nombre
int LeerOpciones (char *trozos[], struct OPCIONES *op) {
  int i;

  //empezamos con todas las opciones a 0 (no puestas)
  op->largo = 0;
  op->link = 0;
  op->acc = 0;
  op->hid = 0;
  op->reca = 0;
  op->recb = 0;
  for (i = 1; trozos[i] != NULL; i++) {
    if      (strcmp(trozos[i], "-long") == 0) op->largo = 1;
    else if (strcmp(trozos[i], "-link") == 0) op->link = 1;
    else if (strcmp(trozos[i], "-acc") == 0)  op->acc = 1;
    else if (strcmp(trozos[i], "-hid") == 0)  op->hid = 1;
    else if (strcmp(trozos[i], "-reca") == 0) op->reca = 1;
    else if (strcmp(trozos[i], "-recb") == 0) op->recb = 1;
    else break;   //lo primero que no es una opcion ya es un nombre
  }
  return i;
}


//funciones de ayuda de los profesores (las copiamos tal cual del codigo de ayuda)
//LetraTF devuelve la letra del tipo de fichero: d directorio, l enlace, - fichero normal...
char LetraTF (mode_t m)
{
     switch (m&S_IFMT) { /*and bit a bit con los bits de formato,0170000 */
        case S_IFSOCK: return 's'; /*socket */
        case S_IFLNK: return 'l'; /*symbolic link*/
        case S_IFREG: return '-'; /* fichero normal*/
        case S_IFBLK: return 'b'; /*block device*/
        case S_IFDIR: return 'd'; /*directorio */
        case S_IFCHR: return 'c'; /*char device*/
        case S_IFIFO: return 'p'; /*pipe*/
        default: return '?'; /*desconocido, no deberia aparecer*/
     }
}

//ConvierteModo escribe los permisos en formato rwx (como ls -l) a partir de st_mode
//permisos tiene que tener sitio para 12 caracteres
char * ConvierteModo (mode_t m, char *permisos)
{
    strcpy (permisos,"---------- ");

    permisos[0]=LetraTF(m);
    if (m&S_IRUSR) permisos[1]='r';    /*propietario*/
    if (m&S_IWUSR) permisos[2]='w';
    if (m&S_IXUSR) permisos[3]='x';
    if (m&S_IRGRP) permisos[4]='r';    /*grupo*/
    if (m&S_IWGRP) permisos[5]='w';
    if (m&S_IXGRP) permisos[6]='x';
    if (m&S_IROTH) permisos[7]='r';    /*resto*/
    if (m&S_IWOTH) permisos[8]='w';
    if (m&S_IXOTH) permisos[9]='x';
    if (m&S_ISUID) permisos[3]='s';    /*setuid, setgid y stickybit*/
    if (m&S_ISGID) permisos[6]='s';
    if (m&S_ISVTX) permisos[9]='t';

    return permisos;
}


//muestra la informacion de un fichero. ruta es donde esta y nombre lo que escribimos en pantalla
void InfoFichero (char *ruta, char *nombre, struct OPCIONES *op) {
  struct stat datos;
  struct passwd *pw;   //datos del usuario propietario
  struct group *gr;    //datos del grupo
  char fecha[32], permisos[12], usuario[32], grupo[32], destino[MAX_NOMBRE];
  time_t t;
  int n;

  if (lstat(ruta, &datos) == -1) {   //lstat para que, si es un enlace, nos de la info del enlace
    printf(" ****error al acceder a %s:%s\n", ruta, strerror(errno));   //mensaje como el del shell de referencia
    return;
  }

  if (!op->largo) {   //sin -long solo el tamaño y el nombre
    printf("%8ld  %s", datos.st_size, nombre);
  } else {
    //con -acc usamos la fecha de ultimo acceso, si no la de ultima modificacion
    if (op->acc)
      t = datos.st_atime;
    else
      t = datos.st_mtime;
    strftime(fecha, sizeof(fecha), "%Y/%m/%d-%H:%M", localtime(&t));

    //getpwuid y getgrgid traducen el numero de usuario y de grupo a su nombre
    //si devuelven NULL (usuario borrado, sin dueño...) ponemos ??????, porque usar pw->pw_name con NULL rompe el programa
    if ((pw = getpwuid(datos.st_uid)) != NULL)
      snprintf(usuario, sizeof(usuario), "%s", pw->pw_name);
    else
      snprintf(usuario, sizeof(usuario), "??????");   //como el shell de referencia
    if ((gr = getgrgid(datos.st_gid)) != NULL)
      snprintf(grupo, sizeof(grupo), "%s", gr->gr_name);
    else
      snprintf(grupo, sizeof(grupo), "??????");

    ConvierteModo(datos.st_mode, permisos);   //funcion de los profesores
    //fecha, numero de enlaces, inodo, propietario, grupo, permisos, tamaño y nombre, todo en UNA linea
    printf("%s %3lu (%8lu) %8s %8s %s%8ld %s", fecha, datos.st_nlink, datos.st_ino,
           usuario, grupo, permisos, datos.st_size, nombre);
  }

  //con -link, si es un enlace simbolico, decimos tambien a donde apunta
  if (op->link && S_ISLNK(datos.st_mode)) {
    if ((n = readlink(ruta, destino, MAX_NOMBRE - 1)) != -1) {   //readlink NO pone el \0, hay que ponerlo
      destino[n] = '\0';
      printf(" -> %s", destino);
    }
  }
  printf("\n");
}


//funcion para el comando listfile: informacion de cada nombre (si es un directorio, del directorio en si, no de lo de dentro)
void Cmd_listfile (char *trozos[]) {
  struct OPCIONES op;
  int i = LeerOpciones(trozos, &op);   //i es donde empiezan los nombres

  if (trozos[i] == NULL) {   //si no hay nombres, mostramos el directorio actual como hace chdir
    MostrarDirectorioActual();
    return;
  }
  for (; trozos[i] != NULL; i++)
    InfoFichero(trozos[i], trozos[i], &op);
}


void ListarDirectorio (char *dir, struct OPCIONES *op);   //prototipo, porque esta y la de abajo se llaman la una a la otra

//entra en cada subdirectorio de dir y lo lista (es lo que hace la recursividad de -reca y -recb)
void RecorrerSubdirectorios (char *dir, struct OPCIONES *op) {
  DIR *d;
  struct dirent *entrada;
  struct stat datos;
  char ruta[MAX_NOMBRE];

  if ((d = opendir(dir)) == NULL) {
    perror(dir);
    return;
  }
  while ((entrada = readdir(d)) != NULL) {
    //. y .. nunca, si no entrariamos en bucle infinito; y los ocultos solo con -hid
    if (strcmp(entrada->d_name, ".") == 0 || strcmp(entrada->d_name, "..") == 0)
      continue;
    if (entrada->d_name[0] == '.' && !op->hid)
      continue;
    snprintf(ruta, MAX_NOMBRE, "%s/%s", dir, entrada->d_name);   //montamos la ruta: dir/lo_de_dentro
    //lstat: si es un enlace a un directorio no entramos, asi no damos vueltas infinitas
    if (lstat(ruta, &datos) == 0 && S_ISDIR(datos.st_mode))
      ListarDirectorio(ruta, op);
  }
  closedir(d);
}


//lista lo que hay dentro de un directorio (y sus subdirectorios si hay -reca o -recb)
void ListarDirectorio (char *dir, struct OPCIONES *op) {
  DIR *d;
  struct dirent *entrada;
  char ruta[MAX_NOMBRE];

  if (op->recb)   //-recb: primero las subcarpetas y despues esta
    RecorrerSubdirectorios(dir, op);

  if ((d = opendir(dir)) == NULL) {
    perror(dir);
    return;
  }
  printf("************%s\n", dir);   //cabecera para saber de que directorio es lo que sale debajo
  while ((entrada = readdir(d)) != NULL) {
    if (entrada->d_name[0] == '.' && !op->hid)   //los ocultos (empiezan por .) solo con -hid
      continue;
    snprintf(ruta, MAX_NOMBRE, "%s/%s", dir, entrada->d_name);
    InfoFichero(ruta, entrada->d_name, op);   //la info la sacamos de la ruta, pero escribimos solo el nombre
  }
  closedir(d);

  if (op->reca)   //-reca: primero esta y despues las subcarpetas
    RecorrerSubdirectorios(dir, op);
}


//funcion para el comando list: como listfile, pero si es un directorio muestra lo que tiene dentro
void Cmd_list (char *trozos[]) {
  struct OPCIONES op;
  struct stat datos;
  int i = LeerOpciones(trozos, &op);

  if (trozos[i] == NULL) {   //sin nombres, igual que listfile: el directorio actual
    MostrarDirectorioActual();
    return;
  }
  for (; trozos[i] != NULL; i++) {
    if (lstat(trozos[i], &datos) == -1) {
      printf(" ****error al acceder a %s:%s\n", trozos[i], strerror(errno));
      continue;
    }
    if (S_ISDIR(datos.st_mode))
      ListarDirectorio(trozos[i], &op);   //si es un directorio, lo de dentro
    else
      InfoFichero(trozos[i], trozos[i], &op);   //si no, como listfile
  }
}




//iniciamos el main
int main () {

  char cadena [MAX_TEXT];   //guardamos la frase leida por el fgets
  char *trozos [MAX_TEXT/2 + 1];  //array donde guardamos las palabras troceadas. como mucho hay MAX_TEXT/2 palabras, y +1 para el NULL del final
  int i;    //indice para recorrer la tabla de comandos
  InicializarAbiertos();   //metemos 0, 1 y 2 en la lista antes de empezar
    
  //Bucle principal del shell (infinito: solo se sale con exit/bye, que llaman a exit(0), o con Ctrl+D)
  while (1) {
    printf("-> ");
    fflush(stdout); //para mostrar el prompt (->) inmediatamente

    //lectura completa del texto escrito por el usuario
    if (fgets (cadena, MAX_TEXT, stdin) == NULL) {
      break;  //si pulsa Ctrl+D (fin de fichero) salimos del bucle
    }

    //troceamos esa linea leida por el fgets; si solo pulsamos enter, volvemos a pedir la orden
    if (TrocearCadena (cadena, trozos) == 0) {
      continue;
    }

    //buscamos el comando en la tabla y llamamos a su funcion
    for (i = 0; comandos[i].nombre != NULL; i++) {        //recorremos la tabla hasta la fila NULL
      if (strcmp(comandos[i].nombre, trozos[0]) == 0) {   //si el nombre de la fila coincide con lo escrito
        comandos[i].funcion(trozos);   //llamamos a la funcion guardada en esa fila
        break;                         //ya lo encontramos, dejamos de buscar
      }
    }

    //si el bucle llego a la fila NULL, es que el comando no esta en la tabla
    if (comandos[i].nombre == NULL) {
      printf("Comando no reconocido %s\n", trozos[0]);
    }
  }

  return 0;
}
