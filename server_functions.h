


//Faz a troca de diretorio
void executa_cd(struct packet *packet);

//Lista os arquivos dentro do diretorio
void executa_ls(struct packet *packet);

//Captura mensagem testa origem, destino e faz checagem  de erro da msg
int captura_mensagem_server(struct packet *packet);

//Monta o pacote da msg que sera enviada
void monta_pacote(struct packet *packet);

//Envia conteudo do arquivo para o client
void executa_cat(struct packet *packet);

//Abre um socket
void inicia_server_socket();

//Fecha o socket
void fecha_server_socket();

void envia_NACK(struct packet *packet);

//Fecha o servidor
void close_server(struct packet *packet);

//Manda conteudo da linha de um arquivo para o client
void executa_linha(struct packet *packet);

//Manda o conteudo de um bloco de linhas de um arquivo para o client
void executa_linhas(struct packet *packet);

//Substitui uma linha ou cria nova linha em um arquivo
void executa_edit(struct packet *packet);

//Faz a compilacao de um arquivo
void executa_compilar(struct packet *packet);