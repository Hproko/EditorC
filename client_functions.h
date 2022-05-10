
//comando cd realiza a troca de diretorio no servidor
void comando_cd(char *buffer);

//Abre o socket
void inicia_client_socket();

//Fecha o socket 
void fecha_client_socket();

//comando ls lista os arquivos dentro do diretorio atual do serviro
void comando_ls();

//lcd faz a troca de diretorio no client(local)
void local_cd();

//lls lista os arquivos no diretorio do client(local)
void local_ls();

//Faz a captura de uma mensagem e testa se ela tem origem, destino e faz ainda detecção de erros
int captura_mensagem_client(struct packet *packet);

//Mostra o conteudo de arquivo do servidor na tela do client
void comando_cat(char *buffer);

//Imprime na tela do client a linha do arquivo que esta no servidor
void comando_linha( int num_linha, char* buffer);

//Imprime o bloco de linhas do arquivo que esta no servidor
void comando_linhas(int num_linha_i, int num_linha_f, char* buffer);

//Comando edit faz a substituicao ou cria nova linha dentro de um arquivo do servidor 
void comando_edit(char *nome_arquivo, int num_linha, char* texto);

//Faz a compilação de um arquivo no servidor
void comando_compilar(char* buffer);

//Envia uma mensagem para o servidor desligar a conexao 
void exit_server();