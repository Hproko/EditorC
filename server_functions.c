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
#include <pthread.h>
#include <time.h>


#include "functions.h"
#include "socket_functions.h"



int s_socket;

int read_check;

#define TIMEOUT 5 //em segundos

void inicia_server_socket(){

	s_socket = CreateRawSocket("lo");

}

void fecha_server_socket(){

	close(s_socket);
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
			send(s_socket, (struct packet *)packet, sizeof(struct packet), 0);
        	
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

int captura_mensagem_server(struct packet *packet){

	struct packet aux;

	int packet_size;

	do{
		//Descarta mensagem duplicada
		packet_size  = recv(s_socket, &aux, sizeof(struct packet), 0);

		packet_size = recv(s_socket, packet, sizeof(struct packet), 0);

		if(packet_size < 0){
			continue;
		}
	}while(packet->inicio != begin || packet->dest != server || packet->orig != client);
	//Testa origem e destino  da mensagem que chegou


	//Checagem de erro no packet
	if(check_parity(packet)){
		#ifdef DEBUG
			printf("paridade\n");
		#endif
		return 0;
	}

	#ifdef DEBUG
		printf("| %x | %d | %d | %d | %d | %x | %s | %x |\n", packet->inicio, packet->dest, packet->orig, packet->size ,packet->sequenc, packet->type, packet->data, packet->paridade);
	#endif

	//Flag que indica que a mensagem chegou entao finaliza thread
	read_check = 1;


	//Mensagem recebida com sucesso;
	return 1;
}

//Aguardando_msg espera receber msg  e inicia timeout
int aguardando_msg(struct packet *pkt_snd, struct packet *pkt_recv){

    pthread_t thread;

	//seta flag para 0 inicia a thread
	read_check = 0;
    pthread_create(&thread, NULL, timeout, pkt_snd);
    if(!captura_mensagem_server(pkt_recv)){
        pthread_join(thread, NULL);
		return 1;
	}
	pthread_join(thread, NULL);

    return 0;

}

//Retira um nome de arquivo de uma string
void get_file_name(char *str, char *nome_arquivo){


	int index_str= 0, index_nome = 0;

	for(int i=0;i<strlen(str);i++){

		if(str[i] == ' ')
			index_str = i+1;

		if(str[i] == '.')
			break;
	}

	while(str[index_str] != ' ' && str[index_str] != '\0'){
		nome_arquivo[index_nome] = str[index_str];
		index_nome++;
		index_str++;
	}
	nome_arquivo[index_nome] = '\0';
}

void monta_pacote(struct packet *packet){
	packet->orig = server;
	packet->dest = client;
	packet->inicio = begin;

	tam_msg(packet);

	monta_paridade(packet);
}

void envia_NACK(struct packet *packet){
    memset(packet->data, 0, sizeof(packet->data));
    packet->type = NACK;
    monta_pacote(packet);
    send(s_socket, packet, sizeof(struct packet), 0);
}

void envia_ACK(struct packet *packet){
    memset(packet->data, 0, sizeof(packet->data));
    packet->type = ACK;
    monta_pacote(packet);
    send(s_socket, packet, sizeof(struct packet), 0);
}

void envia_conteudo_arq(FILE *arq, struct packet *pkt_send, struct packet *pkt_recv, unsigned tipo){


	pkt_send->type = tipo;
	pkt_send->sequenc = pkt_recv->sequenc;

	fgets((char *)pkt_send->data, 15, arq);

	//Envia o arquivo de 15 em 15 bytes para o client	
	while(! feof(arq)){

		monta_pacote(pkt_send);
		send(s_socket, pkt_send, sizeof(struct packet), 0);

		//Espera ack ou nack do client para prosseguir
		while(1){
			aguardando_msg(pkt_send, pkt_recv);
			
			if(pkt_recv->type == ACK && pkt_recv->sequenc == pkt_send->sequenc)
				break;

			if(pkt_recv->type == NACK && pkt_recv->sequenc == pkt_send->sequenc)
				send(s_socket, pkt_send, sizeof(struct packet), 0);
			
		}


		incrementa_frame(pkt_send);

		memset(pkt_send->data, 0, sizeof(pkt_send->data));

		fgets((char *)pkt_send->data, 15, arq);

	}

	if(pkt_send->data[0] != 0){
		monta_pacote(pkt_send);
		send(s_socket, pkt_send, sizeof(struct packet), 0);

		//Espera ack ou nack do client para prosseguir
		while(1){
			aguardando_msg(pkt_send, pkt_recv);
			
			if(pkt_recv->type == ACK && pkt_recv->sequenc == pkt_send->sequenc)
				break;

			if(pkt_recv->type == NACK && pkt_recv->sequenc == pkt_send->sequenc)
				send(s_socket, pkt_send, sizeof(struct packet), 0);
			
		}
	}

	//Manda fim transmissao para o client (Quando chega ao fim do arquivo)
	pkt_send->type = fim_transmissao; 
	monta_pacote(pkt_send);
	send(s_socket, pkt_send, sizeof(struct packet), 0);


}

void recebe_string(char *string, struct packet *pkt_send, struct packet *pkt_recv){


	//Para quando receber fim_nome ou fim_transmissao
	while(pkt_recv->type != fim_nome && pkt_recv->type != fim_transmissao){

		strncat(string, (char *)pkt_recv->data, 15);

		pkt_send->sequenc = pkt_recv->sequenc;
		envia_ACK(pkt_send);
		
		//Recebe novo pacote
		while(!captura_mensagem_server(pkt_recv)){
			pkt_send->sequenc = pkt_recv->sequenc;
			envia_NACK(pkt_send);
		}

	}
}

void executa_cd(struct packet *packet){


	//pacote que sera enviado ao client
	struct packet pkt_send;
	memset(&pkt_send, 0, sizeof(pkt_send));


	//caminho ate o diretorio destino
	char path[100];
	memset(path, 0, sizeof(path));

	pkt_send.type = ACK;

	//recebe nome do arquivo ate receber um tipo fim transmissao
	recebe_string(path, &pkt_send, packet);


	//Tenta mudar de diretorio
	errno = 0;
	
	memset(pkt_send.data, 0, sizeof(pkt_send.data));

	if(pkt_send.type != NACK){
		if(chdir(path) != 0){ 
			
			//Erros de acesso chdir
			if(errno == EACCES){
				pkt_send.type = Erro;
				pkt_send.data[0] = 1;
			}

			if(errno == ENOENT){
				pkt_send.type = Erro;
				pkt_send.data[0] = 2;
			}
		}
	}


	//Envia Erro, ack ou nack para o cliente	


	pkt_send.sequenc = packet->sequenc;
	monta_pacote(&pkt_send);

	send(s_socket, &pkt_send, sizeof(struct packet), 0);

	memset(path,0, sizeof(path));
}


void executa_ls(struct packet *packet){

	FILE *arq;

	//Chama o comando ls e coloca a saida dele num descritor de arquivo
	arq = popen("pwd ; ls", "r");


	struct packet pkt_send;
	memset(&pkt_send, 0, sizeof(pkt_send));

	envia_conteudo_arq(arq, &pkt_send, packet, Cont_ls);

	//Espera um ack ou nack do client
	while(1){

			aguardando_msg(&pkt_send, packet);
			
			if(packet->type == ACK && packet->sequenc == pkt_send.sequenc)
				break;

			if(packet->type == NACK && packet->sequenc == pkt_send.sequenc)
				send(s_socket, &pkt_send, sizeof(struct packet), 0);
		
	}
	

	pclose(arq);


}


void executa_cat(struct packet *packet){

	struct packet pkt_send;
	memset(&pkt_send, 0, sizeof(struct packet));


	FILE *arq;


	char command[160] = "cat -n ";

	char path[100];
	memset(path, 0, sizeof(path));
	
	//Concatena o nome do arquivo na string command 
	recebe_string(path, &pkt_send, packet);

	
	//Teste de erro 
	errno = 0;

	arq = fopen(path, "r");

	if((arq == NULL)){

		pkt_send.type = Erro;

		if(errno == EACCES)
			pkt_send.data[0] = 1;

		if(errno == ENOENT)
			pkt_send.data[0] = 3;

		pkt_send.sequenc = packet->sequenc;

		monta_pacote(&pkt_send);

		send(s_socket, &pkt_send, sizeof(struct packet), 0);
		return;
	}


	fclose(arq);

	//Concatena o path ate o arquivo no comando
	strcat(command, path);



	//Chama o comando cat e coloca a saida num descritor de arquivo
	arq = popen(command, "r");


	envia_conteudo_arq(arq, &pkt_send, packet, Cont_arq);

	pclose(arq);

	//Espera ack ou nack (caso seja nack reenvia fim_transmissao)
	while(1){

		aguardando_msg(&pkt_send, packet);

		if(packet->type == ACK && packet->sequenc == pkt_send.sequenc)
			break;
		if(packet->type == NACK && packet->sequenc == pkt_send.sequenc)
			send(s_socket, &pkt_send, sizeof(struct packet), 0);
	
	}




}



void close_server(struct packet *packet){

	struct packet pkt_send;
	pkt_send.sequenc = packet->sequenc;
	envia_ACK(&pkt_send);

}



void executa_linha(struct packet *packet){

	struct packet pkt_send;
	memset(&pkt_send, 0, sizeof(struct packet));


	FILE *arq;

	int num_linha = 0;

	char number[50];


	char command[160] = "sed -n ";

	char path[100];
	memset(path, 0, sizeof(path));
	
	recebe_string(path, &pkt_send, packet);

	//Teste de erro
	arq = fopen(path, "r");

	if((arq == NULL)){

		pkt_send.type = Erro;

		if(errno == EACCES)
			pkt_send.data[0] = 1;

		if(errno == ENOENT)
			pkt_send.data[0] = 3;

		pkt_send.sequenc = packet->sequenc;

		monta_pacote(&pkt_send);

		send(s_socket, &pkt_send, sizeof(struct packet), 0);
		return;
	}


	fclose(arq);



	pkt_send.sequenc = packet->sequenc;
	envia_ACK(&pkt_send);

	//Espera o client enviar o numero da linha 
	while(1){
		captura_mensagem_server(packet);

		if(packet->type == line_num){
			memcpy(&num_linha, &packet->data[0], sizeof(num_linha));

			sprintf(number, "%d", num_linha);
			break;
		}
	}


	//Concatena numero e path para criar o comando sed
	strcat(command, number);
	strcat(command, "p ");
	strcat(command, path);

	arq = popen(command, "r");

	envia_conteudo_arq(arq, &pkt_send, packet, Cont_arq);

	pclose(arq);

	//Espera ack ou nack (caso seja nack reenvia fim_transmissao)
	while(1){

		aguardando_msg(&pkt_send, packet);

		if(packet->type == ACK && packet->sequenc == pkt_send.sequenc)
			break;
		if(packet->type == NACK && packet->sequenc == pkt_send.sequenc)
			send(s_socket, &pkt_send, sizeof(struct packet), 0);
	
	}	
	

}


void executa_linhas(struct packet *packet){
	struct packet pkt_send;
	memset(&pkt_send, 0, sizeof(struct packet));


	FILE *arq;

	int num_linha_i = 0;
	int num_linha_f = 0;

	char number_i[15];
	char number_f[15];


	char command[160] = "sed -n ";

	char path[100];
	memset(path, 0, sizeof(path));
	
	//Concatena o nome do arquivo na string command 
	recebe_string(path, &pkt_send, packet);



	//Teste de erro
	errno = 0;

	arq = fopen(path, "r");

	if((arq == NULL)){

		pkt_send.type = Erro;

		if(errno == EACCES)
			pkt_send.data[0] = 1;

		if(errno == ENOENT)
			pkt_send.data[0] = 3;

		pkt_send.sequenc = packet->sequenc;
		monta_pacote(&pkt_send);
		send(s_socket, &pkt_send, sizeof(struct packet), 0);
		return;
	}

	fclose(arq);




	pkt_send.sequenc = packet->sequenc;
	envia_ACK(&pkt_send);

	//Espera  o client enviar os numeros das linhas
	while(1){
		captura_mensagem_server(packet);

		if(packet->type == line_num){
			memcpy(&num_linha_i, &packet->data[0], sizeof(int));

			memcpy(&num_linha_f, &packet->data[4], sizeof(int));

			sprintf(number_i, "%d", num_linha_i);
			sprintf(number_f, "%d", num_linha_f);
			break;
		}
	}





	//Concatena as linhas e o path para formar o comando sed
	strcat(command, number_i);
	strcat(command,",");
	strcat(command, number_f);
	strcat(command, "p ");
	strcat(command, path);

	arq = popen(command, "r");

	envia_conteudo_arq(arq, &pkt_send, packet, Cont_arq);

	pclose(arq);

	//Espera ack ou nack (caso seja nack reenvia fim_transmissao)
	while(1){

		aguardando_msg(&pkt_send, packet);

		if(packet->type == ACK && packet->sequenc == pkt_send.sequenc)
			break;
		if(packet->type == NACK && packet->sequenc == pkt_send.sequenc)
			send(s_socket, &pkt_send, sizeof(struct packet), 0);
	
	}	
	

}


void executa_edit(struct packet *packet){

	struct packet pkt_send;
	memset(&pkt_send, 0, sizeof(struct packet));


	FILE *arq;

	int num_linha;

	char number[5];
	
	int num_linhas_arquivo = 0;
	
	char aux;


	char texto[300];
	memset(texto, 0, sizeof(texto));

	char path[100];
	memset(path, 0, sizeof(path));
	
	recebe_string(path, &pkt_send, packet);

	
	//Teste de erro
	errno = 0;

	arq = fopen(path, "r");

	//Testa se arquivo existe e suas permissoes
	if((arq == NULL)){

		pkt_send.type = Erro;

		if(errno == EACCES)
			pkt_send.data[0] = 1;

		if(errno == ENOENT)
			pkt_send.data[0] = 3;

		pkt_send.sequenc = packet->sequenc;
		monta_pacote(&pkt_send);
		send(s_socket, &pkt_send, sizeof(struct packet), 0);
		return;
	}

	fclose(arq);

	pkt_send.sequenc = packet->sequenc;
	envia_ACK(&pkt_send);

	//Aguarda o client enviar o numero da linha que sera substituida
	while(1){

		while(!captura_mensagem_server(packet)){
			pkt_send.sequenc = packet->sequenc;
			envia_NACK(&pkt_send);
		}

		if(packet->type == line_num){
			memcpy(&num_linha, &packet->data[0], sizeof(num_linha));
			sprintf(number, "%d", num_linha);
			break;
		}
	}



	//Conta numero de linhas no arquivo
	arq = fopen(path, "r");

	while(fread(&aux, sizeof(char), 1, arq)){
		if(aux == '\n')
			num_linhas_arquivo++;
	}
	fclose(arq);
	


	//Caso a linha escolhida pelo usuario nao possa ser usada manda erro
	if(num_linha > num_linhas_arquivo + 1){

		pkt_send.type = Erro;
		
		pkt_send.data[0] = 4;
		
		pkt_send.sequenc = packet->sequenc;
		
		monta_pacote(&pkt_send);
		
		send(s_socket, &pkt_send, sizeof(struct packet), 0);

		return;
	}

	pkt_send.sequenc = packet->sequenc;
	envia_ACK(&pkt_send);



	//Espera client enviar o texto
	while(1){
	
		while(! captura_mensagem_server(packet)){
			pkt_send.sequenc = packet->sequenc;
			envia_NACK(&pkt_send);
		}	

		if(packet->type == Cont_arq)
			break;
	}

	recebe_string(texto, &pkt_send, packet);



	//Adiciona nova linha no arquivo
	if(num_linha == num_linhas_arquivo + 1){


			arq = fopen(path, "a+");
			strcat(texto, "\n");
			fwrite(texto, sizeof(char), strlen(texto), arq);
			fclose(arq);
		

	}

	
	//Substitui a linha ja existente no arquivo
	else{

		char command[500] = "sed -i '";
		strcat(command, number);
		strcat(command, "s/.*/");
		strcat(command, texto);
		strcat(command, "/' ");
		strcat(command, path);

		
		system(command);



	}


	pkt_send.sequenc = packet->sequenc;
	envia_ACK(&pkt_send);
		
	memset(path, 0, sizeof(path));
	memset(texto, 0, sizeof(texto));

}

void executa_compilar(struct packet *packet){

	struct packet pkt_send;
	memset(&pkt_send, 0, sizeof(struct packet));

	char arch_arguments[200];
	memset(arch_arguments, 0, sizeof(arch_arguments));

	char nome_arquivo[100] ;

	char command[160];
	memset(command, 0, sizeof(command));

	FILE *arq;


	recebe_string(arch_arguments, &pkt_send, packet);


	get_file_name(arch_arguments, nome_arquivo);

	//teste de erro
	errno = 0;

	arq = fopen(nome_arquivo, "r");

	if(errno != 0){
		pkt_send.type = Erro;

		if(errno == EACCES)
			pkt_send.data[0] = 1;

		if(errno == ENOENT)
			pkt_send.data[0] = 3;

		pkt_send.sequenc = packet->sequenc;
		monta_pacote(&pkt_send);
		send(s_socket, &pkt_send, sizeof(struct packet), 0);
		return;
	}

	fclose(arq);

	//Concatena path ao comando gcc e coloca saida de erro em stdout
	strcat(command, "gcc ");
	strcat(command, arch_arguments);
	strcat(command, " 2>&1");

	//Compila o arquivo 
	arq = popen(command, "r");

	envia_conteudo_arq(arq, &pkt_send, packet, Cont_arq);


	//Espera o client enviar um ack ou nack
	while(1){
		aguardando_msg(&pkt_send, packet);

		if(packet->type == ACK && packet->sequenc == pkt_send.sequenc)
			break;

		if(packet->type == NACK && packet->sequenc == pkt_send.sequenc){
			send(s_socket, &pkt_send, sizeof(struct packet), 0);
		}
	}

	pclose(arq);

}
