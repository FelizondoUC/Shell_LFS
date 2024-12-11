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

#define MAX_LFS_INPUT 1024
#define MAX_ARGS 100
#define CONFIG_FILE "/etc/usuarios_info.txt"  // Archivo para almacenar la información del usuario

// Función para mostrar el prompt
void prompt() {
    printf("lfs-shell> ");
     fflush(stdout);
}

// Implementación del comando 'copiar'
void copiar(const char *origen, const char *destino) {
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
        printf("Archivo '%s' copiado exitosamente a '%s'\n", origen, destino);
    }
}

// Implementación del comando 'mover'
void mover(const char *origen, const char *destino) {
    printf("Moviendo archivo de: %s a: %s\n", origen, destino); // Verifica las rutas
    if (rename(origen, destino) != 0) {
        perror("Error al mover el archivo");
    }
}

// Implementación del comando 'renombrar'
void renombrar(const char *archivo, const char *nuevo_nombre) {
    if (rename(archivo, nuevo_nombre) != 0) {
        perror("Error al renombrar el archivo");
    }
}


// Implementación del comando 'listar'
void listar(const char *directorio) {
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
        printf("Directorio '%s' creado exitosamente\n", directorio);
    }
}

// Función para cambiar de directorio
void ir(const char *directorio) {
    if (chdir(directorio) == 0) {
        printf("Directorio cambiado a: %s\n", directorio);
    } else {
        perror("Error al cambiar de directorio");
    }
}

//************************************************************************************************************
//Comando para obtener el directorio actual.
void mostrar() {
    char cwd[MAX_LFS_INPUT]; // Buffer para almacenar el directorio actual
    if (getcwd(cwd, sizeof(cwd)) != NULL) { // getcwd obtiene el directorio actual
        printf("%s\n", cwd); // Imprime el directorio actual
    } else {
        perror("Error al obtener el directorio actual");
    }
}
//************************************************************************************************************

// Función para cambiar permisos
void permisos(const char *modo, const char **archivos, int cantidad) {
    mode_t permisos = strtol(modo, NULL, 8); // Convierte el modo (octal) a un valor numérico
    for (int i = 0; i < cantidad; i++) {
        if (chmod(archivos[i], permisos) == 0) {                    //chmod en este caso es una función del sistema incluida en la biblioteca estándar de C <sys/stat.h>
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
//**************************************************************************************************
//Función para cambiar la contraseña de un usuario
void cambiar_clave(const char *usuario) {
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
//****************************************************************************************************



// Función para ejecutar comandos del sistema
void ejecutar_comando_sistema(char *comando) {
    int resultado = system(comando);  // Ejecuta el comando del sistema

    if (resultado == -1) {
        perror("Error al ejecutar el comando");
    } else {
        printf("Comando ejecutado con éxito: %s\n", comando);
    }
}


//Procesar y Ejecutar Comandos
void procesar_comando(char *input) {
    char *args[MAX_ARGS];
    char *token = strtok(input, " \t\n");
    int i = 0;

    while (token != NULL && i < MAX_ARGS - 1) {
        args[i++] = token;
        token = strtok(NULL, " \t\n");
    }
    args[i] = NULL;

    if (args[0] == NULL) return;

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
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);
    } else {
        // Si no es uno de los comandos anteriores, lo ejecutamos como un comando del sistema
        ejecutar_comando_sistema(input);
    }
}


// Función principal de la shell
int main() {
    char input[MAX_LFS_INPUT];

    while (1) {
        prompt();
        if (fgets(input, MAX_LFS_INPUT, stdin) == NULL) {
            break; // Salir con Ctrl+D
        }
        procesar_comando(input);
    }

    return 0;
}