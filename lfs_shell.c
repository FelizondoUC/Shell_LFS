#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h> // Necesaria para funciones POSIX
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>
#include <errno.h> // Necesaria para manejar errores con errno
#include <signal.h>
#include <time.h>

#define MAX_LFS_INPUT 1024
#define MAX_ARGS 100
#define USER_DATA_FILE "/usr/local/bin/usuarios_data.txt" //Aca se guardan los datos de inicio de sesion del ususario
#define HISTORIAL_FILE "/var/log/shell/historial.log" // Archivo para el historial
#define ERROR_LOG_FILE "/var/log/shell/sistema_error.log" // Archivo para errores
// Estructura para datos de usuario
typedef struct {
    char nombre[64];
    char contrasena[64];
    char horario[64];  // Ejemplo: "10:00-17:00"
    char ips[128];     // Ejemplo: "192.168.100.57,localhost"
} Usuario;


// Funcion para obtener el timestamp actual
void obtener_timestamp(char *buffer, size_t size) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    if (t != NULL) {
        strftime(buffer, size, "%Y-%m-%d %H:%M:%S", t);
    } else {
        snprintf(buffer, size, "Timestamp no disponible");
    }
}

// Funcion para registrar en el historial
void registrar_historial(const char *comando) {
    FILE *archivo = fopen(HISTORIAL_FILE, "a");
    if (archivo == NULL) {
        perror("Error al abrir historial.log");
        return;
    }

    char timestamp[64];
    obtener_timestamp(timestamp, sizeof(timestamp));

    fprintf(archivo, "%s: %s\n", timestamp, comando);
    fclose(archivo);
}

// Funcion para registrar errores
void registrar_error(const char *mensaje) {
    FILE *archivo = fopen(ERROR_LOG_FILE, "a");
    if (archivo == NULL) {
        perror("Error al abrir sistema_error.log");
        return;
    }

    char timestamp[64];
    obtener_timestamp(timestamp, sizeof(timestamp));
    perror(mensaje);
    fprintf(archivo, "%s: ERROR: %s\n", timestamp, mensaje);
    fclose(archivo);
}

// Función para dividir el comando en argumentos
void parse_command(char *input, char **args) {
    char *token;
    int i = 0;

    // Dividir el input por espacios
    token = strtok(input, " \n");
    while (token != NULL) {
        args[i++] = token;
        token = strtok(NULL, " \n");
    }
    args[i] = NULL; // Terminar la lista de argumentos
}

void ejecutar_comando(const char *comando) {
    pid_t pid = fork();
    if (pid == 0) {
        // Proceso hijo para ejecutar el comando
        char *argv[] = {"/bin/sh", "-c", (char *)comando, NULL};
        execvp(argv[0], argv);
        perror("Error al ejecutar el comando");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        // Proceso padre espera
        waitpid(pid, NULL, 0);
        printf("Comando ejecutado: %s\n", comando);
    } else {
        registrar_error("Error al ejecutar comando genérico");
    }
}


// Función para mostrar el prompt
void prompt() {
    printf("lfs-shell> ");
     fflush(stdout);
}

// Implementación del comando 'copiar'
void copiar(const char *origen, const char *destino) {
    registrar_historial(origen);
    int origen_fd = open(origen, O_RDONLY);
    if (origen_fd < 0) {
        perror("Error al abrir el archivo origen");
        return;
    }

    int destino_fd = open(destino, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (destino_fd < 0) {
        perror("Error al crear el archivo destino");
        close(origen_fd);
        return;
    }

    char buffer[MAX_LFS_INPUT];
    ssize_t bytes_leidos;
    int exito = 1; // Variable para rastrear si ocurre un error
    while ((bytes_leidos = read(origen_fd, buffer, sizeof(buffer))) > 0) {
        if (write(destino_fd, buffer, bytes_leidos) < 0) {
            perror("Error al escribir en el archivo destino");
            exito = 0; // Marca como fallido si hay un error
            break;
        }
    }

    if (bytes_leidos < 0) {
        perror("Error al leer el archivo origen");
        exito = 0; // Marca como fallido si hay un error
    }

    close(origen_fd);
    close(destino_fd);

        // Mensaje de éxito si no hubo errores
    if (exito) {
        registrar_historial(origen);
        printf("Archivo '%s' copiado exitosamente a '%s'\n", origen, destino);
    }
}

// Implementación del comando 'mover'
void mover(const char *origen, const char *destino) {
    printf("Moviendo archivo de: %s a: %s\n", origen, destino); // Verifica las rutas
    registrar_historial(origen);
    if (rename(origen, destino) != 0) {
        perror("Error al mover el archivo");
    }
}

// Implementación del comando 'renombrar'
void renombrar(const char *archivo, const char *nuevo_nombre) {
    registrar_historial(archivo);
    if (rename(archivo, nuevo_nombre) != 0) {
        perror("Error al renombrar el archivo");
    }
}

// Implementación del comando 'listar'
void listar(const char *directorio) {
    registrar_historial(directorio);
    DIR *dir = opendir(directorio);
    if (dir == NULL) {
        perror("Error al abrir el directorio");
        return;
    }

    struct dirent *entrada;
    while ((entrada = readdir(dir)) != NULL) {
        printf("%s\n", entrada->d_name);
    }

    closedir(dir);
}

// Implementación del comando 'creardir'
void creardir(const char *directorio) {
    if (mkdir(directorio, 0755) != 0) {
        perror("Error al crear el directorio");
    } else {
        registrar_historial(directorio);
        printf("Directorio '%s' creado exitosamente\n", directorio);
    }
}

// Función para cambiar de directorio
void ir(const char *directorio) {
    if (chdir(directorio) == 0) {
        registrar_historial(directorio);
        printf("Directorio cambiado a: %s\n", directorio);
    } else {
        perror("Error al cambiar de directorio");
    }
}

//Comando para obtener el directorio actual.
void mostrar() {
    char cwd[MAX_LFS_INPUT]; // Buffer para almacenar el directorio actual
    if (getcwd(cwd, sizeof(cwd)) != NULL) { // getcwd obtiene el directorio actual
        //registrar_historial();
        printf("%s\n", cwd); // Imprime el directorio actual
    } else {
        perror("Error al obtener el directorio actual");
    }
}


// Función para cambiar permisos
void permisos(const char *modo, const char **archivos, int cantidad) {
    mode_t permisos = strtol(modo, NULL, 8); // Convierte el modo (octal) a un valor numérico
    for (int i = 0; i < cantidad; i++) {
        if (chmod(archivos[i], permisos) == 0) {                    //chmod en este caso es una función del sistema incluida en la biblioteca estándar de C <sys/stat.h>
            registrar_historial(modo);
            printf("Permisos cambiados para: %s\n", archivos[i]);
        } else {
            perror("Error al cambiar permisos");
        }
    }
}

// Función para cambiar el propietario y el grupo de los archivos
void cambiar_propietario(const char *nuevo_propietario, const char *nuevo_grupo, const char *archivos[], int cantidad_archivos) {
    struct passwd *propietario_info;
    struct group *grupo_info;
    int i;
    registrar_historial(nuevo_propietario);

    // Obtener la información del nuevo propietario
    propietario_info = getpwnam(nuevo_propietario);
    if (propietario_info == NULL) {
        printf("Error: El propietario '%s' no existe.\n", nuevo_propietario);
        return;
    }

    // Obtener la información del nuevo grupo
    grupo_info = getgrnam(nuevo_grupo);
    if (grupo_info == NULL) {
        printf("Error: El grupo '%s' no existe.\n", nuevo_grupo);
        return;
    }

    // Iterar sobre los archivos y cambiar el propietario y el grupo
    for (i = 0; i < cantidad_archivos; i++) {
        if (chown(archivos[i], propietario_info->pw_uid, grupo_info->gr_gid) == -1) {
            perror("Error al cambiar propietario y grupo");
        } else {
            printf("Propietario y grupo de '%s' cambiados a '%s' y '%s'.\n", archivos[i], nuevo_propietario, nuevo_grupo);
        }
    }
}

//Función para cambiar la contraseña de un usuario
void cambiar_clave(const char *usuario) {
    registrar_historial(usuario);
    if (usuario == NULL || strlen(usuario) == 0) {
        printf("Error: Usuario no especificado.\n");
        return;
    }

    // Construir el comando para cambiar la contraseña
    char comando[MAX_LFS_INPUT];
    snprintf(comando, sizeof(comando), "passwd %s", usuario);

    // Ejecutar el comando
    int resultado = system(comando);
    if (resultado == -1) {
        perror("Error al intentar cambiar la clave");
    } else if (resultado == 0) {
        printf("La clave para el usuario '%s' se cambió exitosamente.\n", usuario);
    } else {
        printf("Error: El comando 'passwd' devolvió un código de salida %d.\n", resultado);
    }
}

//Funcion para agregar usuario con su ip y horario laboral
void agregar_usuario(const char *nombre, const char *horario, const char *ips) {
    FILE *archivo;
    char comando[256];

    // Crear el usuario en el sistema
    snprintf(comando, sizeof(comando), "useradd -m %s", nombre);
    if (system(comando) != 0) {
        printf("Error al crear el usuario '%s'.\n", nombre);
        return;
    }

    // Establecer contraseña
    printf("Estableciendo contraseña para '%s'.\n", nombre);
    snprintf(comando, sizeof(comando), "passwd %s", nombre);
    if (system(comando) != 0) {
        printf("Error al establecer la contraseña para '%s'.\n", nombre);
        return;
    }

    // Guardar los datos del usuario en el archivo
    archivo = fopen(USER_DATA_FILE, "a");
    if (archivo == NULL) {
        registrar_error("Error al abrir el archivo de datos de usuarios");
        return;
    }

    fprintf(archivo, "Usuario: %s\n", nombre);
    fprintf(archivo, "Horario: %s\n", horario);
    fprintf(archivo, "IPs permitidas: %s\n", ips);
    fprintf(archivo, "-----------------------------------\n");

    fclose(archivo);

    printf("Usuario '%s' creado exitosamente con horario '%s' y acceso desde '%s'.\n",
           nombre, horario, ips);
}

void manejador_SIGINT(int sig) {
    printf("\nSeñal SIGINT capturada.\n");
    // No matamos el shell principal, pero podemos manejar interrupciones aquí
}

//Fucnion para ejecutar comandos del sistema
void ejecutar_comando_sistema(char **args) {
    registrar_historial(args[0]);
    pid_t pid = fork();
    if (pid == -1) {
        perror("Error al bifurcar");
        return;
    }

    if (pid == 0) {
        // Proceso hijo
        if (execvp(args[0], args) == -1) {
            perror("Error al ejecutar el comando");
        }
        exit(EXIT_FAILURE);
    } else {
        // Proceso padre
        int status;

        //Cambiar el maneajdor de selales para capturar Ctrl+C
        struct sigaction sa;
        sa.sa_handler = manejador_SIGINT;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;

        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("Comando ejecutado con éxito: %s\n", args[0]);
        } else {
            printf("El comando terminó de manera anormal: %s\n", args[0]);
        }
    }
}

void listarDemonios() {
    //registrar_historial(args[0]);
    struct dirent *entry;
    DIR *dp = opendir("/etc/init.d");

    if (dp == NULL) {
        perror("Error al abrir /etc/init.d");
        return;
    }

    printf("Demonios disponibles:\n");
    while ((entry = readdir(dp)) != NULL) {
        if (entry->d_name[0] != '.') {
            printf("  %s\n", entry->d_name);
        }
    }
    closedir(dp);
}

int obtenerPID(const char *nombre) {
    //registrar_historial(nombre);
    char ruta[MAX_LFS_INPUT];
    char buffer[128];
    snprintf(ruta, sizeof(ruta), "/var/run/%s.pid", nombre);

    FILE *archivo = fopen(ruta, "r");
    if (archivo == NULL) {
        return -1; // PID no encontrado
    }

    if (fgets(buffer, sizeof(buffer), archivo) != NULL) {
        fclose(archivo);
        return atoi(buffer);
    }

    fclose(archivo);
    return -1;
}

void iniciarDemonio(const char *nombre) {
    //registrar_historial(args[0]);
    char ruta[MAX_LFS_INPUT];
    snprintf(ruta, sizeof(ruta), "/etc/init.d/%s", nombre);

    if (access(ruta, X_OK) != 0) {
        printf("El demonio '%s' no existe o no es ejecutable.\n", nombre);
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("Error al bifurcar");
        return;
    }

    if (pid == 0) {
        // Proceso hijo
        execl(ruta, nombre, "start", (char *)NULL);
        perror("Error al ejecutar el demonio");
        exit(EXIT_FAILURE);
    } else {
        // Proceso padre
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            printf("Demonio '%s' iniciado exitosamente.\n", nombre);
        } else {
            printf("Error al iniciar el demonio '%s'.\n", nombre);
        }
    }
}

void detenerDemonio(const char *nombre) {
    //registrar_historial(args[0]);
    int pid = obtenerPID(nombre);
    if (pid == -1) {
        printf("No se encontró un proceso en ejecución para el demonio '%s'.\n", nombre);
        return;
    }

    if (kill(pid, SIGTERM) == 0) {
        printf("Demonio '%s' detenido exitosamente.\n", nombre);
    } else {
        perror("Error al detener el demonio");
    }
}

void procesarDemonio(const char *accion, const char *nombre) {
    if (strcmp(accion, "listar") == 0) {
        listarDemonios();
    } else if (strcmp(accion, "iniciar") == 0) {
        iniciarDemonio(nombre);
    } else if (strcmp(accion, "detener") == 0) {
        detenerDemonio(nombre);
    } else {
        printf("Acción desconocida. Usa 'listar', 'iniciar <nombre>', o 'detener <nombre>'.\n");
    }
}

//Función para registrar el inicio de cesion
void registrar_sesion(const char *usuario, const char *accion) {
    const char *DATA_FILE = "/usr/local/bin/usuario_horarios.log";
    FILE *archivo = fopen(DATA_FILE, "a");
    if (archivo == NULL) {
        registrar_error("Error al abrir usuario_horarios.log");
        return;
    }

    char timestamp[64];
    obtener_timestamp(timestamp, sizeof(timestamp));

    fprintf(archivo, "%s: Usuario '%s' %s sesión\n", timestamp, usuario, accion);
    fclose(archivo);
}

//Funcion para la transferencia por ftp o scp, registrando en el log
void transferencia_archivo(const char *origen, const char *destino, const char *metodo) {
    char log_path[PATH_MAX] = "Shell_transferencias.log";
    FILE *archivo = fopen(log_path, "a");
    if (archivo == NULL) {
        registrar_error("Error al abrir Shell_transferencias.log");
        return;
    }

    char timestamp[64];
    obtener_timestamp(timestamp, sizeof(timestamp));

    if (strcmp(metodo, "scp") == 0 || strcmp(metodo, "ftp") == 0) {
        fprintf(archivo, "%s: Transferencia iniciada de '%s' a '%s' usando %s\n", timestamp, origen, destino, metodo);
        fclose(archivo);
        ejecutar_comando(metodo);  // Ejemplo para delegar en herramientas como SCP
    } else {
        fprintf(archivo, "%s: Método de transferencia no soportado: '%s'\n", timestamp, metodo);
        fclose(archivo);
    }
}

//Procesar y Ejecutar Comandos
void procesar_comando(char *input) {
    char *args[MAX_ARGS];
    char *token = strtok(input, " \t\n");
    int i = 0;

    registrar_historial(input);

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;

    if (args[0] == NULL){
        return;
    }

    // Comandos implementados
    if (strcmp(args[0], "propietario") == 0) {
        if (i >= 4) {
            cambiar_propietario(args[1], args[2], (const char **)&args[3], i - 3);
        } else {
            printf("Uso: propietario <nuevo_propietario> <nuevo_grupo> <archivo1> [archivo2 ... archivoN]\n");
        }
    } else if (strcmp(args[0], "permisos") == 0) {
        if (i >= 3) {
            permisos(args[1], (const char **)&args[2], i - 2);
        } else {
            printf("Uso: permisos <modo> <archivo1> [archivo2 ... archivoN]\n");
        }
    } else if (strcmp(args[0], "copiar") == 0) {
        if (i == 3) {
            copiar(args[1], args[2]);
        } else {
            printf("Uso: copiar <archivo_origen> <archivo_destino>\n");
        }
    } else if (strcmp(args[0], "mover") == 0) {
        if (i == 3) {
            mover(args[1], args[2]);
        } else {
            printf("Uso: mover <archivo_origen> <archivo_destino>\n");
        }
    } else if (strcmp(args[0], "renombrar") == 0) {
        if (i == 3) {
            renombrar(args[1], args[2]);
        } else {
            printf("Uso: renombrar <archivo> <nuevo_nombre>\n");
        }
    } else if (strcmp(args[0], "listar") == 0) {
        const char *directorio = (i == 2) ? args[1] : ".";
        listar(directorio);
    } else if (strcmp(args[0], "creardir") == 0) {
        if (i == 2) {
            creardir(args[1]);
        } else {
            printf("Uso: creardir <nombre_directorio>\n");
        }
    } else if (strcmp(args[0], "ir") == 0) {
        if (i == 2) {
            ir(args[1]);
        } else {
            printf("Uso: ir <nombre_directorio>\n");
        }
    } else if (strcmp(args[0], "mostrar") == 0) {
        mostrar();
    } else if (strcmp(args[0], "clave") == 0) {
        if (i == 2) {
            cambiar_clave(args[1]);
        } else {
            printf("Uso: clave <usuario>\n");
        }
    } else if (strcmp(args[0], "demonio") == 0) {
        if (i >= 2) {
            if (strcmp(args[1], "listar") == 0) {
                listarDemonios();
            } else if (i == 3 && strcmp(args[1], "iniciar") == 0) {
                iniciarDemonio(args[2]);
            } else if (i == 3 && strcmp(args[1], "detener") == 0) {
                detenerDemonio(args[2]);
            } else {
                printf("Uso: demonio listar | demonio iniciar <nombre_demonio> | demonio detener <nombre_demonio>\n");
            }
        } else {
            printf("Uso: demonio listar | demonio iniciar <nombre_demonio> | demonio detener <nombre_demonio>\n");
        }
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);
    }else if (strcmp(args[0], "transferencia") == 0) {
        if (args[1] && args[2] && args[3]) {
            transferencia_archivo(args[1], args[2], args[3]);
        } else {
            printf("Uso: transferencia <origen> <destino> <metodo>\n");
        } 
    }else if (strcmp(args[0], "usuario") == 0) {
        if (args[1] && args[2] && args[3]) {
            agregar_usuario(args[1], args[2], args[3]);
        } else {
            printf("Uso: usuario <nombre> <horario> <ips>\n");
        }
    } else if (strcmp(args[0], "sesion") == 0){
        if (args[1] == NULL || args[2] == NULL) {
        printf("Uso: sesion <usuario> <accion>\n");
    } else {
        registrar_historial(args[0]);
    }
    } else {
        // Si no es uno de los comandos anteriores, lo ejecutamos como un comando del sistema
        ejecutar_comando_sistema(args);
    }
}

// Función principal de la shell
int main() {
    // Configurar el manejador de señales
    struct sigaction sa;
    sa.sa_handler = manejador_SIGINT;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Reiniciar las llamadas al sistema interrumpidas

    if (sigaction(SIGINT, &sa, NULL) == -1) {
        perror("Error al configurar el manejador de señales");
        exit(EXIT_FAILURE);
    }
    
    char input[MAX_LFS_INPUT];

    registrar_sesion("lfs_usuario", "inició");

    while (1) {
        prompt();
        if (fgets(input, MAX_LFS_INPUT, stdin) == NULL) {
            registrar_sesion("lfs_usuario", "cerró");
            break; // Salir con Ctrl+D
        }
        procesar_comando(input);
    }



    return 0;
}