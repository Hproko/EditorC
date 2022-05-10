#include <sys/types.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <net/ethernet.h>
#include <linux/if_packet.h>
#include <linux/if.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

#include "functions.h"
#include "socket_functions.h"

int c_socket;

int read_check;


#define TIMEOUT 5 //em segundos

void inicia_client_socket(){

	c_socket = CreateRawSocket("lo");

}

void fecha_client_socket(){

	close(c_socket);
}



void* timeout(void *packet){

	//inicio recebe o tempo atual em segundos
    time_t inicio = time(0);

	time_t intervalo;
    
	while(1){
		//Seta o intervalo entre o inicio e o tempo atual
		intervalo = time(0) - inicio;

		//Caso intervalo igual ao tempo de timeout e a funcao captura mensagem ainda nao tenha recebido dados do servirdor
		//Reenvia ultima mensagem
		if(intervalo > 0 && intervalo == TIMEOUT && read_check == 0){
			send(c_socket, (struct packet *)packet, sizeof(struct packet), 0);
        	
			#ifdef DEBUG
				printf("timeout\n");
			#endif

			inicio = time(0);
		}

		//Se a funcao captura mensagem encontrou dados do servidor entao finaliza thread
		if(read_check == 1)
			break;
	}

    pthread_exit(NULL);
}


int captura_mensagem_client(struct packet *packet){

	struct packet aux;

	int packet_size;


	//Caso mensagem nao seja para o destino correto ela eh dropada
	do{
		//Descarta a mensagem duplicada
		packet_size  = recv(c_socket, &aux, sizeof(struct packet), 0);

		packet_size = recv(c_socket, packet, sizeof(struct packet), 0);

		if(packet_size < 0){
			continue;
		}


	}while(packet->inicio != begin || packet->dest != client || packet->orig != server);
	///Detecta marcador de inicio, le destino e origem

	//Chega se a mensagem chegou corretamente
	if(check_parity(packet)){
		#ifdef DEBUG
			printf("paridade\n");
		#endif
		return 0;
	}

	#ifdef DEBUG
		printf("| %x | %d | %d | %d | %d | %x | %s | %x |\n", packet->inicio, packet->dest, packet->orig, packet->size ,packet->sequenc, packet->type, packet->data, packet->paridade);
	#endif


	//Seta a flag de leitura como um, indica para a thread que ela pode terminar
	read_check = 1;


	//Mensagem recebida com sucesso;
	return 1;
}



void monta_pacote(struct packet *packet){

	packet->inicio = begin;
	packet->orig = client;
	packet->dest = server;


	tam_msg(packet);

	monta_paridade(packet);
}


void envia_NACK(struct packet *packet){
    memset(packet->data, 0, sizeof(packet->data));
    packet->type = NACK;
    monta_pacote(packet);
    send(c_socket, packet, sizeof(struct packet), 0);
}

void envia_ACK(struct packet *packet){
    memset(packet->data, 0, sizeof(packet->data));
    packet->type = ACK;
    monta_pacote(packet);
    send(c_socket, packet, sizeof(struct packet), 0);
}


//Aguardando_msg espera receber msg  e inicia timeout
int aguardando_msg(struct packet *pkt_snd, struct packet *pkt_recv){

    pthread_t thread;

	//Seta a flag de leitura como 0 e inicia thread
	read_check = 0;
    pthread_create(&thread, NULL, timeout, pkt_snd);

    if(!captura_mensagem_client(pkt_recv)){
        pthread_join(thread, NULL);
		return 1;
	}

	//Espera pelo fim da thread para evitar memory leak
	pthread_join(thread, NULL);

    return 0;

}

void removeAspas(char *str) {


	int i =0;
	while(str[i] != '\0'){
		i++; 
	}

	str[i-2] = str[i];
}


void envia_string(char *string, struct packet *pkt_send, struct packet *pkt_recv, unsigned tipo, unsigned tipo_fim){

	pkt_send->type = tipo;


    int buffer_size;

	buffer_size = strlen(string);

	int buffer_index = 0;


	//envio do nome do diretorio na area de dados (pode enviar varias vezes para nome com mais de 15 bytes)
	while(buffer_index < buffer_size){

		memset(pkt_send->data, 0, sizeof(pkt_send->data));

		strncpy((char *)pkt_send->data, &string[buffer_index], sizeof(pkt_send->data)-1);

		monta_pacote(pkt_send);

		send(c_socket, pkt_send, sizeof(struct packet), 0);


		//Espera ack ou nack
		while(1){
	
			
			aguardando_msg(pkt_send, pkt_recv);

			if(pkt_recv->type == ACK && pkt_recv->sequenc == pkt_send->sequenc)
				break;


			if(pkt_recv->type == NACK && pkt_recv->sequenc == pkt_send->sequenc)
				send(c_socket, pkt_send, sizeof(struct packet), 0);


		}

		incrementa_frame(pkt_send);

		buffer_index += strlen((char *)pkt_send->data);


	}

	//Envia fim transmissao para o servidor
	pkt_send->type = tipo_fim;
	memset(pkt_send->data, 0, sizeof(pkt_send->data));
	monta_pacote(pkt_send);

	send(c_socket, pkt_send, sizeof(struct packet), 0);



}


void comando_cd(char *buffer, short *frame){

    struct packet pkt_send, pkt_recv;
    memset(&pkt_send, 0, sizeof(struct packet));

	envia_string(buffer, &pkt_send, &pkt_recv, CD, fim_nome);


    //Espera ack, nack ou erro 
	while(1){

		aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc){
			send(c_socket, &pkt_send, sizeof(struct packet), 0);
			continue;
		}

		if(pkt_recv.type == ACK && pkt_recv.sequenc == pkt_send.sequenc){
			break;
		}


		if(pkt_recv.type == Erro){

			printf("Erro: ");

			if(pkt_recv.data[0] == 1)
				printf("Permissao negada\n");

			if(pkt_recv.data[0] == 2)
				printf("Diretorio nao encontrado\n");

			break;
		}

	}
}



void comando_ls(){

	struct packet pkt_send, pkt_recv;
	memset(&pkt_send, 0, sizeof(pkt_send));

	pkt_send.type = ls;

	monta_pacote(&pkt_send);

    //Manda requisicao do comando ls para o servidor
	send(c_socket, &pkt_send, sizeof(struct packet), 0);

	//Aguarda mensagem com o conteudo do ls
	while(1){
		
		aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == Cont_ls)
			break;

		if(pkt_recv.type == NACK && pkt_send.sequenc == pkt_recv.sequenc)
			send(c_socket, &pkt_send, sizeof(struct packet), 0);

		
	}

	//Imprime as mensagens na tela caso seja conteudo do ls e sai do loop quando receber fim transmissao
	while(1){
		
		if(pkt_recv.type == fim_transmissao){
			pkt_send.sequenc = pkt_recv.sequenc;
			envia_ACK(&pkt_send);
			break;
		}

		if(pkt_recv.type == Cont_ls){
			#ifndef DEBUG
				printf("%s", pkt_recv.data);
			#endif
			pkt_send.sequenc = pkt_recv.sequenc;
			envia_ACK(&pkt_send);
		}

        //Espera mensagem do servidor (caso detecte erro envia NACK)
		while(! captura_mensagem_client(&pkt_recv)){
            pkt_send.sequenc = pkt_recv.sequenc;
			envia_NACK(&pkt_send);
        }


	}


	return;

}



void local_cd(){

	char path[100];

	getchar();
	scanf("%99[^\n]", path);

	errno = 0;
	if(chdir(path) != 0){
		if(errno == EACCES)
			printf("Permissao negada\n");

		if(errno == ENOENT)
			printf("Diretorio nao encontrado\n");
	}
}



void local_ls(){

	FILE *arq;

	arq = popen("ls", "r");


	printf("\nConteudo do diretorio local:\n");


	char line[1024];

	fgets(line, 1024, arq);
	while(! feof(arq)){
		printf("%s", line);
		fgets(line, 1024, arq);
	}

    pclose(arq);


}



void comando_cat(char *buffer){

    struct packet pkt_send, pkt_recv;
    memset(&pkt_send, 0, sizeof(pkt_send));


	envia_string(buffer, &pkt_send, &pkt_recv, ver, fim_nome);

  
	//Aguarda conteudo de arquivo, fim transmissao, ou erro 
	while(1){
    	aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == Cont_arq){
			break;
		}

		if(pkt_recv.type == fim_transmissao){

			//Se receber fim transmissao significa que o arquivo do servidor estava vazio
			pkt_send.sequenc = pkt_recv.sequenc;
   			envia_ACK(&pkt_send);
			return;
		}

    	if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc){
			pkt_send.sequenc = pkt_recv.sequenc;
			send(c_socket, &pkt_send, sizeof(struct packet), 0);
		}
        	
    
   		if(pkt_recv.type == Erro){

			if(pkt_recv.data[0] == 1)
				printf("Permissao negada\n");
		
			if(pkt_recv.data[0] == 3)
				printf("Arquivo inexistente\n");
        	return;  
    	}
	}

    if(pkt_recv.type == Cont_arq){

        do{

            printf("%s", pkt_recv.data);
            pkt_send.sequenc = pkt_recv.sequenc;
			envia_ACK(&pkt_send);


            while(! captura_mensagem_client(&pkt_recv)){
                envia_NACK(&pkt_send);
            }

        }while(pkt_recv.type != fim_transmissao);
    }    

	pkt_send.sequenc = pkt_recv.sequenc;
   	envia_ACK(&pkt_send);

    return;


}


void exit_server(){

	
	struct packet pkt_send, pkt_recv;
	memset(pkt_send.data, 0, sizeof(pkt_send.data));

	pkt_send.type = KILL;
	monta_pacote(&pkt_send);
	
	send(c_socket, &pkt_send, sizeof(struct packet), 0);

	while(1){
		aguardando_msg(&pkt_send, &pkt_recv);
		if(pkt_recv.type == ACK)
			break;
		if(pkt_recv.type == NACK)
			send(c_socket, &pkt_send, sizeof(struct packet), 0);
	}
}


void comando_linha(int num_linha, char* buffer){



	struct packet pkt_send, pkt_recv;
    memset(&pkt_send, 0, sizeof(struct packet));


	envia_string(buffer, &pkt_send, &pkt_recv, linha, fim_nome);

	//Espera ack, nack ou erro 
	while(1){

		aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == ACK && pkt_recv.sequenc == pkt_send.sequenc){
			break;
		}

		if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc){
			pkt_send.sequenc = pkt_recv.sequenc;
			send(c_socket, &pkt_send, sizeof(struct packet), 0);
		}
			



		if(pkt_recv.type == Erro && pkt_recv.sequenc == pkt_send.sequenc){

			printf("Erro: ");

			if(pkt_recv.data[0] == 1)
				printf("Permissao negada\n");

			if(pkt_recv.data[0] == 3)
				printf("Arquivo inexistente\n");
	
			return;
		}

	}



	memset(pkt_send.data, 0, sizeof(pkt_send.data));

	//Envia numero de linha para o servidor
	pkt_send.type = line_num;

	incrementa_frame(&pkt_send);


	//linha que eh um inteiro ocupa 4 posições no vetor de dados do pacote
	memcpy(&pkt_send.data[0], &num_linha, sizeof(num_linha));

	monta_pacote(&pkt_send);

	send(c_socket, &pkt_send, sizeof(struct packet), 0);



	//Aguarda mensagem do servidor 
	while(1){
			aguardando_msg(&pkt_send, &pkt_recv);

			if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc)
				send(c_socket, &pkt_send, sizeof(struct packet), 0);

			if(pkt_recv.type == Cont_arq) 
				break;
		
			//Se receber fim transmissao aqui o arquivo estava vazio
			if(pkt_recv.type == fim_transmissao){
				pkt_send.sequenc = pkt_recv.sequenc;
				envia_ACK(&pkt_send);
				return;
			}
	}


	//imprime o conteudo do arquivo ate receber um fim_transmissao
	while(pkt_recv.type != fim_transmissao){

		if(pkt_recv.type == Cont_arq){

			printf("%s", pkt_recv.data);
			
			pkt_send.sequenc = pkt_recv.sequenc;

			envia_ACK(&pkt_send);

		}

		while(! captura_mensagem_client(&pkt_recv)){
			pkt_send.sequenc = pkt_recv.sequenc;
			envia_NACK(&pkt_recv);
		}


	}


	pkt_send.sequenc = pkt_recv.sequenc;
	envia_ACK(&pkt_send);

}

void comando_linhas(int num_linha_i, int num_linha_f, char* buffer){



	struct packet pkt_send, pkt_recv;
    memset(&pkt_send, 0, sizeof(struct packet));

	envia_string(buffer, &pkt_send, &pkt_recv, linhas, fim_nome);


	//Espera ack, nack ou erro 
	while(1){
		aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == ACK && pkt_recv.sequenc == pkt_send.sequenc){
			
			break;
		}

		if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc){

			pkt_send.sequenc = pkt_recv.sequenc;
			send(c_socket, &pkt_send, sizeof(struct packet), 0);
		}

		if(pkt_recv.type == Erro && pkt_recv.sequenc == pkt_send.sequenc){

			printf("Erro: ");

			if(pkt_recv.data[0] == 1)
				printf("Permissao negada\n");

			if(pkt_recv.data[0] == 3)
				printf("Arquivo inexistente\n");
	
			return;
		}

	}


	//Envia linha inicial e linha final para o servidor
	pkt_send.type = line_num;

	memset(pkt_send.data, 0, sizeof(pkt_send.data));

	incrementa_frame(&pkt_send);


	//linha inicial ocupa os 4 primeiros bytes do campo de dados e linha final ocupa os 4 bytes seguintes
	memcpy(&pkt_send.data[0], &num_linha_i, sizeof(num_linha_i));

	memcpy(&pkt_send.data[4], &num_linha_f, sizeof(num_linha_f));


	monta_pacote(&pkt_send);

	send(c_socket, &pkt_send, sizeof(struct packet), 0);


	//Aguarda o conteudo do arquivo
	while(1){
			aguardando_msg(&pkt_send, &pkt_recv);

			if(pkt_recv.type == Cont_arq) 
				break;

			//Caso receba fim transmissao envia ack e retorna pra main
			if(pkt_recv.type == fim_transmissao){
				pkt_send.sequenc = pkt_recv.sequenc;
				envia_ACK(&pkt_send);
				return;
			}

			if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc){
				pkt_send.sequenc = pkt_recv.sequenc;
				send(c_socket, &pkt_send, sizeof(struct packet), 0);
			}
				

	}

	//Imprime conteudo na tela ate que receba fim transmissao
	while(pkt_recv.type != fim_transmissao){



		if(pkt_recv.type == Cont_arq){

			printf("%s", pkt_recv.data);
			
			pkt_send.sequenc = pkt_recv.sequenc;

			envia_ACK(&pkt_send);

		}

		while(! captura_mensagem_client(&pkt_recv)){
			pkt_send.sequenc = pkt_recv.sequenc;
			envia_NACK(&pkt_recv);
		}


	}


	pkt_send.sequenc = pkt_recv.sequenc;
	envia_ACK(&pkt_send);



}


void comando_edit(char *nome_arquivo, int num_linha, char* texto){

	struct packet pkt_send, pkt_recv;
    memset(&pkt_send, 0, sizeof(pkt_send));

    envia_string(nome_arquivo, &pkt_send, &pkt_recv, edit, fim_nome);


	//Aguarda por ack, nack ou erro
	while(1){

    	aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == ACK && pkt_recv.sequenc == pkt_send.sequenc)
			break;

    	if(pkt_recv.type == NACK){
			pkt_send.sequenc = pkt_recv.sequenc;
			send(c_socket, &pkt_send, sizeof(struct packet), 0);
		}
    	    
    
    	if(pkt_recv.type == Erro){

			if(pkt_recv.data[0] == 1)
				printf("Permissao negada\n");
		
			if(pkt_recv.data[0] == 3)
				printf("Arquivo inexistente\n");
    	    
			return;   
    	}
	}


	//envia o numero da linha que sera editada 
	pkt_send.type = line_num;

	memset(pkt_send.data, 0, sizeof(pkt_send.data));
	
	incrementa_frame(&pkt_send);

	//numero da linha ocupa 4 bytes do campo de dados
	memcpy(&pkt_send.data[0], &num_linha, sizeof(num_linha));

	monta_pacote(&pkt_send);

	send(c_socket, &pkt_send, sizeof(struct packet), 0);

	
	//Espera ack, nack ou erro 
	while(1){

		aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == ACK && pkt_recv.sequenc == pkt_send.sequenc)
			break;


		if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc)
			send(c_socket, &pkt_send, sizeof(struct packet), 0);



		if(pkt_recv.type == Erro && pkt_recv.sequenc == pkt_send.sequenc){

			printf("Erro: ");

			if(pkt_recv.data[0] == 4)
				printf("Linha inexistente\n");

			return;
		}

	}

	incrementa_frame(&pkt_send);

	memset(pkt_send.data, 0, sizeof(pkt_send.data));



	memcpy(texto, &texto[1], strlen(texto)-1);

	removeAspas(texto);
			
	//envia texto para ser substituido na linha
	envia_string(texto, &pkt_send, &pkt_recv, Cont_arq, fim_transmissao);

	//Espera ack ou nack
	while(1){
	
		aguardando_msg(&pkt_send, &pkt_recv);

		if(pkt_recv.type == ACK && pkt_recv.sequenc == pkt_send.sequenc)
			break;


		if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc)
			send(c_socket, &pkt_send, sizeof(struct packet), 0);

	}
}



void comando_compilar(char* buffer){

	struct packet pkt_send, pkt_recv;
	memset(&pkt_send, 0, sizeof(struct packet));


	//Envia string com o nome e os comandos para o servidor
	envia_string(buffer, &pkt_send, &pkt_recv, compilar, fim_nome);


	//Aguarda server enviar conteudo de arquivo, erro ou fim transmissao
	while(1){

		aguardando_msg(&pkt_send, &pkt_recv);

		if((pkt_recv.type == Cont_arq || pkt_recv.type == fim_transmissao) && pkt_recv.sequenc == pkt_send.sequenc)
			break;

		if(pkt_recv.type == Erro && pkt_recv.sequenc == pkt_send.sequenc){

			if(pkt_recv.data[0] == 1)
				printf("Permissao negada\n");
		
			if(pkt_recv.data[0] == 3)
				printf("Arquivo inexistente\n");

        	return;  
		}

		if(pkt_recv.type == NACK && pkt_recv.sequenc == pkt_send.sequenc)
			send(c_socket, &pkt_send, sizeof(struct packet), 0);
			
	}


	//Imprime o resultado da compilacao na tela e para quando receber fim transmissao
	while(1){
		
		if(pkt_recv.type == fim_transmissao){
			pkt_send.sequenc = pkt_recv.sequenc;
			envia_ACK(&pkt_send);
			break;
		}

		if(pkt_recv.type == Cont_arq){
			printf("%s", pkt_recv.data);
			pkt_send.sequenc = pkt_recv.sequenc;
			envia_ACK(&pkt_send);
		}

        //Espera mensagem do servidor (caso detecte erro envia NACK)
		while(! captura_mensagem_client(&pkt_recv)){
            pkt_send.sequenc = pkt_recv.sequenc;
			envia_NACK(&pkt_send);
        }


	}

}