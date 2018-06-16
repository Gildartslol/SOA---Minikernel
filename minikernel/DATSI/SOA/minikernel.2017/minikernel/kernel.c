/*
 *  kernel/kernel.c
 *
 *  Minikernel. Versión 1.0
 *
 *  Fernando Pérez Costoya
 *
 */

/*
 *
 * Fichero que contiene la funcionalidad del sistema operativo
 *
 */
#include <string.h>
#include "kernel.h"	/* Contiene defs. usadas por este modulo */

/*
 *
 * Funciones relacionadas con la tabla de procesos:
 *	iniciar_tabla_proc buscar_BCP_libre
 *
 */

/*
 * Función que inicia la tabla de procesos
 */
static void iniciar_tabla_proc(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		tabla_procs[i].estado=NO_USADA;
}

/*
 * Función que busca una entrada libre en la tabla de procesos
 */
static int buscar_BCP_libre(){
	int i;

	for (i=0; i<MAX_PROC; i++)
		if (tabla_procs[i].estado==NO_USADA)
			return i;
	return -1;
}

/*
 *
 * Funciones que facilitan el manejo de las listas de BCPs
 *	insertar_ultimo eliminar_primero eliminar_elem
 *
 * NOTA: PRIMERO SE DEBE LLAMAR A eliminar Y LUEGO A insertar
 */

/*
 * Inserta un BCP al final de la lista.
 */
static void insertar_ultimo(lista_BCPs *lista, BCP * proc){
	if (lista->primero==NULL)
		lista->primero= proc;
	else
		lista->ultimo->siguiente=proc;
	lista->ultimo= proc;
	proc->siguiente=NULL;
}

/*
 * Elimina el primer BCP de la lista.
 */
static void eliminar_primero(lista_BCPs *lista){

	if (lista->ultimo==lista->primero)
		lista->ultimo=NULL;
	lista->primero=lista->primero->siguiente;
}

/*
 * Elimina un determinado BCP de la lista.
 */
static void eliminar_elem(lista_BCPs *lista, BCP * proc){
	BCP *paux=lista->primero;

	if (paux==proc)
		eliminar_primero(lista);
	else {
		for ( ; ((paux) && (paux->siguiente!=proc));
			paux=paux->siguiente);
		if (paux) {
			if (lista->ultimo==paux->siguiente)
				lista->ultimo=paux;
			paux->siguiente=paux->siguiente->siguiente;
		}
	}
}

/*
 *
 * Funciones relacionadas con la planificacion
 *	espera_int planificador
 */

/*
 * Espera a que se produzca una interrupcion
 */
static void espera_int(){
	int nivel;

	/*printk("-> NO HAY LISTOS. ESPERA INT\n");*/
	/* Baja al mínimo el nivel de interrupción mientras espera */
	nivel=fijar_nivel_int(NIVEL_1);
	halt();
	fijar_nivel_int(nivel);
}

/*
 * Función de planificacion que implementa un algoritmo FIFO.
 */
static BCP * planificador(){
	while (lista_listos.primero==NULL)
		espera_int();		/* No hay nada que hacer */

	/*Aqui asignaremos la rodaja del proceso*/
	BCP *proceso = lista_listos.primero;
	proceso->ticksRestantes = TICKS_POR_RODAJA;



	return lista_listos.primero;
}

/*
 *
 * Funcion auxiliar que termina proceso actual liberando sus recursos.
 * Usada por llamada terminar_proceso y por rutinas que tratan excepciones
 *
 */
static void liberar_proceso(){
	BCP * p_proc_anterior;

	liberar_imagen(p_proc_actual->info_mem); /* liberar mapa */

	p_proc_actual->estado=TERMINADO;

	int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
	eliminar_primero(&lista_listos); /* proc. fuera de listos */
	fijar_nivel_int(lvl_interrupciones);

	/* Realizar cambio de contexto */
	p_proc_anterior=p_proc_actual;
	p_proc_actual=planificador();

	printk("-> C.CONTEXTO POR FIN: de %d a %d\n",
			p_proc_anterior->id, p_proc_actual->id);

	liberar_pila(p_proc_anterior->pila);
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
        return; /* no debería llegar aqui */
}

/*
 *
 * Funciones relacionadas con el tratamiento de interrupciones
 *	excepciones: exc_arit exc_mem
 *	interrupciones de reloj: int_reloj
 *	interrupciones del terminal: int_terminal
 *	llamadas al sistemas: llam_sis
 *	interrupciones SW: int_sw
 *
 */

/*
 * Tratamiento de excepciones aritmeticas
 */
static void exc_arit(){

	if (!viene_de_modo_usuario())
		panico("excepcion aritmetica cuando estaba dentro del kernel");


	printk("-> EXCEPCION ARITMETICA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no debería llegar aqui */
}

/*
 * Tratamiento de excepciones en el acceso a memoria
 */
static void exc_mem(){

if(accesoParam == 0){
		if (!viene_de_modo_usuario()){
			panico("excepcion de memoria cuando estaba dentro del kernel");
		}
	}

	printk("-> EXCEPCION DE MEMORIA EN PROC %d\n", p_proc_actual->id);
	liberar_proceso();

        return; /* no debería llegar aqui */
}

/*
 * Tratamiento de interrupciones de terminal
 */
static void int_terminal(){
	char car;

	car = leer_puerto(DIR_TERMINAL);
	printk("-> TRATANDO INT. DE TERMINAL %c\n", car);

	// si el buffer no está lleno introduce el caracter nuevo
	if(caracteresEnBuffer >= TAM_BUF_TERM){
		return;
	}

	if(caracteresEnBuffer < TAM_BUF_TERM){
		bufferCaracteres[caracteresEnBuffer] = car;
		caracteresEnBuffer++;		

		// desbloquea primer proceso bloqueado por lectura
		BCP *proceso_bloqueado = lista_bloqueados.primero;
	
		int desbloqueado = 0;
		if (proceso_bloqueado != NULL){
			if(proceso_bloqueado->bloqueo_por_lectura == 1){
				// Desbloquear proceso
				desbloqueado = 1;
				proceso_bloqueado->estado = LISTO;
				proceso_bloqueado->bloqueo_por_lectura = 0;
				int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
				eliminar_elem(&lista_bloqueados, proceso_bloqueado);
				insertar_ultimo(&lista_listos, proceso_bloqueado);
				fijar_nivel_int(lvl_interrupciones);
			}
		}
		
		while(desbloqueado != 1 && proceso_bloqueado != lista_bloqueados.ultimo){
			proceso_bloqueado = proceso_bloqueado->siguiente;
			if(proceso_bloqueado->bloqueo_por_lectura == 1){
				// Desbloquear proceso
				desbloqueado = 1;
				proceso_bloqueado->estado = LISTO;
				proceso_bloqueado->bloqueo_por_lectura = 0;
				int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
				eliminar_elem(&lista_bloqueados, proceso_bloqueado);
				insertar_ultimo(&lista_listos, proceso_bloqueado);
				fijar_nivel_int(lvl_interrupciones);
			}
		}
	}

        return;
}

/*
 * Tratamiento de interrupciones de reloj
 */
static void int_reloj(){

	//printk("-> TRATANDO INT. DE RELOJ\n");


	BCP *proceso_listo = lista_listos.primero;
	
	/* PARTE TIEMPOS_PROCESO Añadimos contadores usuario o a sistema para el proceso en ejecución. Si es nulo nada.*/
	if(proceso_listo != NULL){
		if(viene_de_modo_usuario()){
			p_proc_actual->contador_usuario++;
		}
		else{
			p_proc_actual->contador_sistema++;
		}
	

		/* Comprobamos la rodaja de tiempo */
		if(p_proc_actual->ticksRestantes <= 1){
			/*Si ha consumido toda la rodaja activamos un intr de software*/
			id_int_soft = p_proc_actual->id;
			activar_int_SW();
		}
		else{
			/*Restamos rodaja*/
			p_proc_actual->ticksRestantes--;
		}
	}


	numTicks++;

	BCP *proceso_desbloqueo = lista_bloqueados.primero;
	BCP *proceso_siguiente = NULL;
	if(proceso_desbloqueo != NULL){
		proceso_siguiente = proceso_desbloqueo->siguiente;
	}

	/*Despierta proceso dormido*/
	while(proceso_desbloqueo != NULL){
		
		/*Calculamos el tiempo de bloqueo*/
		int numTicksBloqueo = proceso_desbloqueo->segundos_bloqueo * TICK;
		int ticksTranscurridos = numTicks - proceso_desbloqueo->tiempo_inicio_bloq;

		/* ¿Debe desbloquearse el proceso? */
		if(ticksTranscurridos >= numTicksBloqueo && 
				proceso_desbloqueo->bloqueo_por_lectura == 0 &&
				proceso_desbloqueo->bloqueadoCreandoMutex == 0){

			/*Proceso pasa a listo*/
			proceso_desbloqueo->estado = LISTO;

			int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
			eliminar_elem(&lista_bloqueados, proceso_desbloqueo);
			insertar_ultimo(&lista_listos, proceso_desbloqueo);
			fijar_nivel_int(lvl_interrupciones);	
		}

		proceso_desbloqueo = proceso_siguiente;
		if(proceso_desbloqueo != NULL){
			proceso_siguiente = proceso_desbloqueo->siguiente;
		}
	}

    return;
}

/*
 * Tratamiento de llamadas al sistema
 */
static void tratar_llamsis(){
	int nserv, res;

	nserv=leer_registro(0);
	if (nserv<NSERVICIOS)
		res=(tabla_servicios[nserv].fservicio)();
	else
		res=-1;		/* servicio no existente */
	escribir_registro(0,res);
	return;
}

/*
 * Tratamiento de interrupciones software
 */
static void int_sw(){

	//printk("-> TRATANDO INT. SW\n");

	/*Queremos bloquear el proceso actual*/
	if(id_int_soft == p_proc_actual->id){
		/*Proceso actual al final de la cola de listos*/
		BCP *proceso = lista_listos.primero;
		int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
		eliminar_elem(&lista_listos, proceso);
		insertar_ultimo(&lista_listos, proceso);
		fijar_nivel_int(lvl_interrupciones);

		// Cambio de contexto por int sw de planificación
		BCP *p_proc_bloqueado = p_proc_actual;
		p_proc_actual = planificador();
		cambio_contexto(&(p_proc_bloqueado->contexto_regs), &(p_proc_actual->contexto_regs));
	}

	return;
}

/*
 *
 * Funcion auxiliar que crea un proceso reservando sus recursos.
 * Usada por llamada crear_proceso.
 *
 */
static int crear_tarea(char *prog){
	void * imagen, *pc_inicial;
	int error=0;
	int proc;
	BCP *p_proc;

	proc=buscar_BCP_libre();
	if (proc==-1)
		return -1;	/* no hay entrada libre */

	/* rellenamos el BCP*/
	p_proc=&(tabla_procs[proc]);

	/* crea la imagen de memoria leyendo ejecutable */
	imagen=crear_imagen(prog, &pc_inicial);
	if (imagen)
	{
		p_proc->info_mem=imagen;
		p_proc->pila=crear_pila(TAM_PILA);
		fijar_contexto_ini(p_proc->info_mem, p_proc->pila, TAM_PILA,
			pc_inicial,
			&(p_proc->contexto_regs));
		p_proc->id=proc;
		p_proc->estado=LISTO;

		/* lo inserta al final de cola de listos */
		int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
		insertar_ultimo(&lista_listos, p_proc);
		fijar_nivel_int(lvl_interrupciones);
		error= 0;
	}
	else
		error= -1; /* fallo al crear imagen */

	return error;
}

/*
 *
 * Rutinas que llevan a cabo las llamadas al sistema
 *	sis_crear_proceso sis_escribir
 *
 */

/*
 * Tratamiento de llamada al sistema crear_proceso. Llama a la
 * funcion auxiliar crear_tarea sis_terminar_proceso
 */
int sis_crear_proceso(){
	char *prog;
	int res;

	printk("-> PROC %d: CREAR PROCESO\n", p_proc_actual->id);
	prog=(char *)leer_registro(1);
	res=crear_tarea(prog);
	return res;
}

/*
 * Tratamiento de llamada al sistema escribir. Llama simplemente a la
 * funcion de apoyo escribir_ker
 */
int sis_escribir()
{
	char *texto;
	unsigned int longi;

	texto=(char *)leer_registro(1);
	longi=(unsigned int)leer_registro(2);

	escribir_ker(texto, longi);
	return 0;
}

/*
 * Tratamiento de llamada al sistema terminar_proceso. Llama a la
 * funcion auxiliar liberar_proceso
 */
int sis_terminar_proceso(){

	printk("-> FIN PROCESO %d\n", p_proc_actual->id);

	liberar_proceso();

        return 0; /* no debería llegar aqui */
}

/*
 *
 * Rutina de inicialización invocada en arranque
 *
 */



/** Devuelve el ID del proceso por el cual es invocado **/

int sis_obtener_id_pr() {
	return p_proc_actual->id;
}

/**Duerme el proceso los segundos especificados por registro**/
int sis_dormir(){

	unsigned int segundos;
	int lvl_interrupciones;
	segundos = (unsigned int)leer_registro(1);
	printk("-> durmiendo \n");

	p_proc_actual->estado = BLOQUEADO;
	p_proc_actual->tiempo_inicio_bloq = numTicks;
	p_proc_actual->segundos_bloqueo = segundos;	

	/*Interrupciones inhubidas y guardamos anterior nivel*/
	lvl_interrupciones = fijar_nivel_int(NIVEL_3);
	
	/*Lo sacamos de listos al proceso*/
	eliminar_elem(&lista_listos, p_proc_actual);

	/*Lo introducimos en bloqueados*/
	insertar_ultimo(&lista_bloqueados, p_proc_actual);

	/*Fijamos el anterior nivel de interrupcion*/
	fijar_nivel_int(lvl_interrupciones);

	/*Realizamos el cambio de contexto */
	BCP *p_proc_dormido = p_proc_actual;
	p_proc_actual = planificador();
	cambio_contexto(&(p_proc_dormido->contexto_regs), &(p_proc_actual->contexto_regs));

	return 0;


}

/*devuelve el número de interrupciones de reloj que se han producido desde que arrancó el sistema. */
int sis_tiempos_proceso(){

	struct tiemposDejecucion*tiempos_ejecucion;

	// Comprueba si existe argumento
	tiempos_ejecucion = (struct tiemposDejecucion*)leer_registro(1);

	if(tiempos_ejecucion != NULL){
		// Si hay argumento fija variable global
		int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
		accesoParam = 1;
		fijar_nivel_int(lvl_interrupciones);

		// Rellena estructura con el tiempo de sistema y tiempo de usuario
		tiempos_ejecucion->usuario = p_proc_actual->contador_usuario;
		tiempos_ejecucion->sistema = p_proc_actual->contador_sistema;
	}

	return numTicks;
}


int sis_crear_mutex(){

	char *nombre = (char *)leer_registro(1);
	int tipo = (int)leer_registro(2);

	// Comprueba número de mutex del proceso
	if(p_proc_actual->numMutex >= NUM_MUT_PROC){
		return -1;
	}

	// Comprueba tamaño de nombre
	if(strlen(nombre) > MAX_NOM_MUT){
		return -2;
	}	

	// Comprueba nombre único de mutex
	int i;
	for (i = 0; i < NUM_MUT; i++){
		if(array_mutex[i].nombre != NULL && 
			strcmp(array_mutex[i].nombre, nombre) == 0){
			return -3;
		}
	}

	// Compueba número de mutex en el sistema
	while(mutexExistentes == NUM_MUT){
		// Bloquear proceso actual
		p_proc_actual->estado = BLOQUEADO;
		p_proc_actual->bloqueadoCreandoMutex = 1;

		int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
		eliminar_elem(&lista_listos, p_proc_actual);
		insertar_ultimo(&lista_bloqueados, p_proc_actual);
		fijar_nivel_int(lvl_interrupciones);

		// Cambio de contexto voluntario
		BCP *proceso_bloqueado = p_proc_actual;
		p_proc_actual = planificador();
		cambio_contexto(&(proceso_bloqueado->contexto_regs), &(p_proc_actual->contexto_regs));	

		// Vuelve a activarse y comprueba nombre único de mutex
		for (i = 0; i < NUM_MUT; i++){
			if(array_mutex[i].nombre != NULL && 
				strcmp(array_mutex[i].nombre, nombre) == 0){
				return -3;
			}
		}
	}

	// Busca espacio libre para crear nuevo mutex
	int posMutex;
	for (i = 0; i < NUM_MUT; i++){
		if(array_mutex[i].nombre == NULL){
			mutex *mutexCreado = &(array_mutex[i]);
			mutexCreado->nombre = strdup(nombre);
			mutexCreado->tipo=tipo;
			array_mutex[i].procesos[p_proc_actual->id] = 1;
			posMutex = i;
			break;
		}
	}
	
	mutexExistentes++;

	// Busca descriptor libre para mutex
	int df = -4; // Descriptor
	for (i = 0; i < NUM_MUT_PROC; i++){
		if(p_proc_actual->array_mutex_proceso[i] == NULL){
			p_proc_actual->array_mutex_proceso[i] = &array_mutex[posMutex];
			p_proc_actual->numMutex++;
			df = i;
			break;
		}
	}

	return df;
}


int sis_abrir_mutex(){


char *nombre = (char *)leer_registro(1);

	// Comprueba número de mutex del proceso
	if(p_proc_actual->numMutex == NUM_MUT_PROC){
		return -1;
	}

	int i;
	int encontrado = 0;

	/*Buscamos en el array de mutex uno con el mismo nombre. COmprobamos existencia*/
	for (i = 0; i < NUM_MUT; i++){
		if(array_mutex[i].nombre != NULL && strcmp(array_mutex[i].nombre, nombre) == 0){
			array_mutex[i].procesos[p_proc_actual->id] = 1;
			encontrado = 1;
			break;
		}
	}

	/*Si no existe el mutex*/
	if(encontrado == 0){
		return -1;
	}

	/*Si existe el mutex buscamos un descriptor que asociale*/
	int df = -1; 
	for (i = 0; i < NUM_MUT_PROC; i++){
		if(p_proc_actual->array_mutex_proceso[i] == NULL){
			p_proc_actual->array_mutex_proceso[i] = &array_mutex[encontrado];
			p_proc_actual->numMutex++;
			df = i;
			break;
		}
	}

	return df;

}

int sis_lock(){


}

int sis_unlock(){

}


int sis_cerrar_mutex(){


}

/*
int sis_leer_caracter(){	
	while(1){
		// Si el buffer está vacío se bloquea
		if(caracteresEnBuffer == 0){
			p_proc_actual->estado = BLOQUEADO;
			p_proc_actual->bloqueo_por_lectura = 1;

			int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
			eliminar_elem(&lista_listos, p_proc_actual);
			insertar_ultimo(&lista_bloqueados, p_proc_actual);
			fijar_nivel_int(lvl_interrupciones);
			lvl_interrupciones = fijar_nivel_int(NIVEL_2);

			// Cambio de contexto voluntario		
			BCP *proceso_bloqueado = p_proc_actual;
			p_proc_actual = planificador();
			cambio_contexto(&(proceso_bloqueado->contexto_regs), &(p_proc_actual->contexto_regs));
			fijar_nivel_int(lvl_interrupciones);
		}
		else{
			int i;
			// Solicita el primer caracter del buffer
			int lvl_interrupciones = fijar_nivel_int(NIVEL_2);
			char car = bufferCaracteres[0];
			caracteresEnBuffer--;
			printk("-> Consumiendo %c\n", car);
			// Reordena el buffer
			for (i = 0; i < caracteresEnBuffer; i++){
				bufferCaracteres[i] = bufferCaracteres[i+1];
			}
			fijar_nivel_int(lvl_interrupciones);

			return (long)car;
		}
	}	
}

*/


int sis_leer_caracter(){
	
	int lvl_interrupciones = fijar_nivel_int(NIVEL_2);
	// Bloqueo si vacio -> con loop en vez de condicion
	while(caracteresEnBuffer == 0){
		p_proc_actual->estado = BLOQUEADO;
		p_proc_actual->bloqueo_por_lectura = 1;
		int lvl_interrupciones = fijar_nivel_int(NIVEL_3);
		eliminar_elem(&lista_listos, p_proc_actual);
		insertar_ultimo(&lista_bloqueados, p_proc_actual);
		fijar_nivel_int(lvl_interrupciones);

		// Cambio de proceso actual con cambio de contexto
		BCP *proc_bloq = p_proc_actual;
		p_proc_actual = planificador();
		cambio_contexto(&(proc_bloq->contexto_regs), &(p_proc_actual->contexto_regs));
	}

	// Recuperar primer caracter
	char car = bufferCaracteres[0];

	/*Reordenamos el buffer*/
	printk("Reasignado buffer, tamanio = %d\n", caracteresEnBuffer);
	caracteresEnBuffer--;
	int i;
	for (i = 0; i < caracteresEnBuffer; i++){
		bufferCaracteres[i] = bufferCaracteres[i+1];
	}
	fijar_nivel_int(lvl_interrupciones);

	return car;


}

int main(){
	/* se llega con las interrupciones prohibidas */

	instal_man_int(EXC_ARITM, exc_arit); 
	instal_man_int(EXC_MEM, exc_mem); 
	instal_man_int(INT_RELOJ, int_reloj); 
	instal_man_int(INT_TERMINAL, int_terminal); 
	instal_man_int(LLAM_SIS, tratar_llamsis); 
	instal_man_int(INT_SW, int_sw); 

	iniciar_cont_int();		/* inicia cont. interr. */
	iniciar_cont_reloj(TICK);	/* fija frecuencia del reloj */
	iniciar_cont_teclado();		/* inici cont. teclado */

	iniciar_tabla_proc();		/* inicia BCPs de tabla de procesos */

	/* crea proceso inicial */
	if (crear_tarea((void *)"init")<0)
		panico("no encontrado el proceso inicial");
	
	/* activa proceso inicial */
	p_proc_actual=planificador();
	cambio_contexto(NULL, &(p_proc_actual->contexto_regs));
	panico("S.O. reactivado inesperadamente");
	return 0;
}
