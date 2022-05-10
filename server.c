#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "functions.h"
#include "socket_functions.h"
#include "server_functions.h"



int main(){

	inicia_server_socket();


	struct packet packet_recv, packet_send;
	memset(&packet_send, 0, sizeof(struct packet));
	memset(&packet_recv, 0, sizeof(struct packet));


	int fim_programa = 0;
	
	while(fim_programa == 0){
		
		if(! captura_mensagem_server(&packet_recv)){
			packet_send.sequenc = packet_recv.sequenc;
			envia_NACK(&packet_send);
			continue;
		}
		
		switch(packet_recv.type){

			case CD:
				executa_cd(&packet_recv);
				break;

			case ls:
				executa_ls(&packet_recv);
				break;

			case ver:
				executa_cat(&packet_recv);
				break;

			case KILL:
				close_server(&packet_recv);
				fim_programa = 1;
				break;

			case linha:
				executa_linha(&packet_recv);
				break;

			case linhas:
				executa_linhas(&packet_recv);
				break;

			case edit:
				executa_edit(&packet_recv);
				break;

			case compilar:
				executa_compilar(&packet_recv);
				break;

		}
	}
	
	fecha_server_socket();
	return 0;
}
