#include <stdio.h>
#include <stdlib.h>

int main(int argc, char *argv[])
{
    /* Le serveur attend 2 arguments : l'IP et le port de SimDom */
    if (argc != 3) {
        fprintf(stderr, "Usage : %s <ip_simdom> <port_simdom>\n", argv[0]);
        return EXIT_FAILURE;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);

    printf("Serveur domotique démarré\n");
    printf("SimDom : %s:%d\n", ip, port);

    return EXIT_SUCCESS;
}