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
#include "functions.h"

#include "socket_functions.h"
#include "client_functions.h"


int main(){

	int num_linha_i, num_linha_f;
	
	char comando[10];

	char path[50];

	char texto[300];

	char buffer[100];

	
	inicia_client_socket();

	
	while(1){

		printf("\nlocal -%s$ ", getcwd(path, sizeof(path)));
		
		scanf("%s",comando);

		if(!strcmp(comando, "cd")){
			getchar();
			scanf("%99[^\n]", buffer);
			comando_cd(buffer);
			continue;
		}

		if(!strcmp(comando, "ls")){
			
				 comando_ls();
			
		}

		if(!strcmp(comando,"lcd")){
			local_cd();
			continue;
		}

		if(!strcmp(comando, "lls")){
			local_ls();
			continue;
		}

		if(! strcmp(comando, "ver")){
			getchar();
			scanf("%s", buffer);
			comando_cat(buffer);
		
			continue;
		}
		if(! strcmp(comando, "exit")){
			exit_server();
			break;
		}

		if(! strcmp(comando,"linha")){
			//Tira espaco 
			getchar();
			scanf("%d", &num_linha_i);

			//Tira espaco
			getchar();
			scanf("%s", buffer);

	
			comando_linha(num_linha_i, buffer);
		
			
			continue;
		}

		if(! strcmp(comando,"linhas")){
			
			//Tira espaco 
			getchar();
			scanf("%d", &num_linha_i);
			
			//Tira espaco 
			getchar();
			scanf("%d", &num_linha_f);

			//Tira espaco
			getchar();
			scanf("%s", buffer);

			comando_linhas(num_linha_i, num_linha_f, buffer);
			
			
			continue;


		}

		if(! strcmp(comando, "edit")){
			
			//Tira espaco 
			getchar();
			scanf("%d", &num_linha_i);

			//Tira espaco
			getchar();
			scanf("%s", buffer);

			getchar();
			scanf("%299[^\n]", texto);
			if(texto[0] != '"' || texto[strlen(texto)-1] != '"'){
				printf("Use aspas para delimitar o texto\n");
				continue;
			}

			comando_edit(buffer, num_linha_i, texto);

			continue;


		}

		if(! strcmp(comando, "compilar")){

			getchar();
			scanf("%99[^\n]", buffer);

			comando_compilar(buffer);
		}


	}

	fecha_client_socket();
	return 0;

}
