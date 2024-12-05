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

// Función para mostrar el prompt
void prompt() {
    printf("lfs-shell> ");
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
    while ((bytes_leidos = read(origen_fd, buffer, sizeof(buffer))) > 0) {
        if (write(destino_fd, buffer, bytes_leidos) < 0) {
            perror("Error al escribir en el archivo destino");
            break;
        }
    }

    if (bytes_leidos < 0) {
        perror("Error al leer el archivo origen");
    }

    close(origen_fd);
    close(destino_fd);
}

// Implementación del comando 'mover'
void mover(const char *origen, const char *destino) {
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

// Función para cambiar permisos
void permisos(const char *modo, const char **archivos, int cantidad) {
    mode_t permisos = strtol(modo, NULL, 8); // Convierte el modo (octal) a un valor numérico
    for (int i = 0; i < cantidad; i++) {
        if (chmod(archivos[i], permisos) == 0) {
            printf("Permisos cambiados para: %s\n", archivos[i]);
        } else {
            perror("Error al cambiar permisos");
        }
    }
}

// Función para cambiar propietario y grupo
void propietario(const char *nuevo_propietario, const char *nuevo_grupo, const char **archivos, int cantidad) {
    uid_t uid = -1; // -1 significa no cambiar
    gid_t gid = -1;

    // Obtener UID del propietario si es especificado
    if (nuevo_propietario != NULL && strcmp(nuevo_propietario, "-") != 0) {
        errno = 0; // Inicializar errno
        struct passwd *pwd = getpwnam(nuevo_propietario);
        if (pwd == NULL) {
            if (errno != 0) {
                perror("Error al obtener UID del propietario");
            } else {
                fprintf(stderr, "Error al obtener UID del propietario '%s': No se encontró el usuario.\n", nuevo_propietario);
            }
            return;
        }
        uid = pwd->pw_uid;
    }

    // Obtener GID del grupo si es especificado
    if (nuevo_grupo != NULL && strcmp(nuevo_grupo, "-") != 0) {
        errno = 0; // Inicializar errno
        struct group *grp = getgrnam(nuevo_grupo);
        if (grp == NULL) {
            if (errno != 0) {
                perror("Error al obtener GID del grupo");
            } else {
                fprintf(stderr, "Error al obtener GID del grupo '%s': No se encontró el grupo.\n", nuevo_grupo);
            }
            return;
        }
        gid = grp->gr_gid;
    }

    // Cambiar propietario y grupo para cada archivo
    for (int i = 0; i < cantidad; i++) {
        if (chown(archivos[i], uid, gid) == 0) {
            printf("Propietario/grupo cambiado para: %s\n", archivos[i]);
        } else {
            perror("Error al cambiar propietario/grupo");
        }
    }
}

// Procesar y ejecutar comandos
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

    if (strcmp(args[0], "propietario") == 0) {
        if (i >= 4) {
            propietario(args[1], args[2], (const char **)&args[3], i - 3);
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
    } else if (strcmp(args[0], "exit") == 0) {
        exit(0);
    } else {
        printf("Comando desconocido: %s\n", args[0]);
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
