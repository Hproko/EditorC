#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include "functions.h"



//Monta o byte de paridade do pacote (usa paridade vertical para detectar erros)
void monta_paridade(struct packet *packet){

    unsigned char paridade;

    paridade = packet->size ^ packet->type;
    paridade = paridade ^ packet->sequenc;

    for(int i=0;i<15;i++)
        paridade = paridade ^ packet->data[i];

    
    packet->paridade = paridade;
}


//Checagem  de erros paridade vertical
int check_parity(struct packet *packet){

    unsigned char check;

    check = packet->size ^ packet->type;
    check = check ^ packet->sequenc;

    for(int i=0;i<15;i++)
        check = check ^ packet->data[i];

    if(check != packet->paridade)
        return 1;
    
    return 0;

    
}


//Calcula tamanho da msg no pacote
void tam_msg(struct packet *packet){
    
    packet->size = strlen((char *)packet->data) + 1;
}

//aumenta indice da mensagem
void incrementa_frame(struct packet *packet){

    packet->sequenc+=1;
    if(packet->sequenc == 16)
        packet->sequenc = 0;
}





