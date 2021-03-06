/*
 *  minikernel/include/kernel.h
 *
 *  Minikernel. Versi�n 1.0
 *
 *  Fernando P�rez Costoya
 *
 */

/*
 *
 * Fichero de cabecera que contiene definiciones usadas por kernel.c
 *
 *      SE DEBE MODIFICAR PARA INCLUIR NUEVA FUNCIONALIDAD
 *
 */

#ifndef _KERNEL_H
#define _KERNEL_H

#include "const.h"
#include "HAL.h"
#include "llamsis.h"


#define NO_RECURSIVO 0
#define RECURSIVO 1




/*
 * Definimos un mutex compuesto por su nombre, tipo , arrays procesos abiertos y bloqueados.

 */
typedef struct{
    char *nombre; 	// nombre del mutex
	int tipo;		// tipo del mutex (no recursivo = 0, recursivo = 1)
	int procesos[MAX_PROC]; // Procesos con el mutex abierto
	int procesosBloqueados[MAX_PROC]; // Procesos bloqueados en el mutex
} mutex;


/*
 *
 * Definicion del tipo que corresponde con el BCP.
 * Se va a modificar al incluir la funcionalidad pedida.
 *
 */
typedef struct BCP_t *BCPptr;

typedef struct BCP_t {
    int id;				/* identificaci�n del proceso */
    int estado;			/* TERMINADO|LISTO|EJECUCION|BLOQUEADO*/
    contexto_t contexto_regs;	/* copia de registros de UCP */
    void * pila;			/* direcci�n inicial de la pila */
	BCPptr siguiente;		/* puntero a otro BCP */
	void *info_mem;			/* descriptor del mapa de memoria */

	/**Funcion dormir**/
	int tiempo_inicio_bloq;		/* momento de bloqueo */
	int segundos_bloqueo;		/* n�mero de segundos de bloqueo */

	/*Funcion contabilidad*/
	int contador_sistema;		/* numero de interr. en modo sistema */
	int contador_usuario;		/* numero de interr. en modo usuario */

	/**Funcion MUTEX*/
	

	/*Round Robin*/
	int ticksRestantes; /* n�mero de ticks restantes para terminar rodaja */

	/*Leer caracteres*/
	int bloqueo_por_lectura;/* 1 indica que esta bloqueado por lectura de caracter */

} BCP;



/*
 *
 * Definicion del tipo que corresponde con la cabecera de una lista
 * de BCPs. Este tipo se puede usar para diversas listas (procesos listos,
 * procesos bloqueados en sem�foro, etc.).
 *
 */

typedef struct{
	BCP *primero;
	BCP *ultimo;
} lista_BCPs;


/*
 * Variable global que identifica el proceso actual
 */

BCP * p_proc_actual=NULL;

/*
 * Variable global que representa la tabla de procesos
 */

BCP tabla_procs[MAX_PROC];

/*
 * Variable global que representa la cola de procesos listos
 */
lista_BCPs lista_listos= {NULL, NULL};


/*
 * Variable global que representa la cola de procesos bloqueados
 */
lista_BCPs lista_bloqueados = {NULL, NULL};

/*
 * Variable global que representa el acceso a zona de usuario en memoria
 */
int accesoParam = 0;

/*
 * Variable global que representa el n�mero de llamadas a int_reloj
 */
int numTicks = 0;

/*
   Variable global que representa el id del proceso al que va
   dirigida la int sw de planificacion
 */
int id_int_soft = 0;

/*
 * Buffer de caracteres procesados del terminal
 */
char bufferCaracteres[TAM_BUF_TERM];

/*
 * Variable global que indica el n�mero de caracteres en el buffer
 */
int caracteresEnBuffer = 0;


/*
 * Array de mutex
 */
mutex array_mutex[NUM_MUT];

/*
 * Variable global que indica el n�mero de mutex existentes
 */
int mutexExistentes = 0;


/*
 *
 * Definici�n del tipo que corresponde con una entrada en la tabla de
 * llamadas al sistema.
 *
 */
typedef struct{
	int (*fservicio)();
} servicio;


/*
Guarda el numero de veces ejecutado en modo sistema y cuantas en modo usuario.
 */
typedef struct tiemposDejecucion {
    int usuario;
    int sistema;
} tiempos_ejec;


/*
 * Prototipos de las rutinas que realizan cada llamada al sistema
 */
int sis_crear_proceso();
int sis_terminar_proceso();
int sis_escribir();
/* Nuevas para la pr�ctica */
int sis_obtener_id_pr();
int sis_dormir();
int sis_tiempos_proceso();

int sis_crear_mutex();
int sis_abrir_mutex();
int sis_lock();
int sis_unlock();
int sis_cerrar_mutex();
int sis_leer_caracter();

/*
 * Variable global que contiene las rutinas que realizan cada llamada
 */
servicio tabla_servicios[NSERVICIOS]={	{
					sis_crear_proceso},
					{sis_terminar_proceso},
					{sis_escribir},
					{sis_obtener_id_pr},
					{sis_dormir},
					{sis_tiempos_proceso},
					{sis_crear_mutex},
					{sis_abrir_mutex},
					{sis_lock},
					{sis_unlock},
					{sis_cerrar_mutex},
					{sis_leer_caracter}


				};

#endif /* _KERNEL_H */

