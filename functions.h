


//Lista de tipos que a mensagem pode assumir
#define CD 0
#define ls 1
#define ver 2
#define linha 3
#define linhas 4
#define edit 5
#define compilar 6
#define fim_nome 7
#define ACK 8
#define NACK 9
#define line_num 10
#define Cont_ls 11
#define Cont_arq 12
#define fim_transmissao 13
#define KILL 14
#define Erro 15

//Marcador de inicio
#define begin 0X7E

//Endereco do client e do servidor
#define client 0x1
#define server 0x2

//Estrutura do pacote 
struct packet{
	unsigned inicio:8;
	unsigned dest:2;
	unsigned orig:2;
	unsigned size:4;
	unsigned sequenc:4;
	unsigned type:4;
	unsigned char data[15];
	unsigned paridade:8;
};

//Criacao do byte de paridade do pacote
void monta_paridade(struct packet *packet);

//Calculo do tamanho msg
void tam_msg(struct packet *packet);

//Checagem de erros na msg (paridade vertical)
int check_parity(struct packet *packet);

//Incrementa indice da msg
void incrementa_frame(struct packet *packet);


