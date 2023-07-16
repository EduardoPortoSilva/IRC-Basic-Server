#include <iostream>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <csignal>
#include <atomic>
#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <csignal>
#include <atomic>
#include <set>
#include <limits.h>
#include <regex>

using namespace std;

int clientSocket;
bool quit = false;


void handleServerCommunication(int serverSocket) { // Recebe as mensagens do servidor e printa o que é necessario
    char buffer[4096];

    while (true) {
        memset(buffer, 0, sizeof(buffer)); // Limpa o buffer
        ssize_t bytesRead = recv(serverSocket, buffer, sizeof(buffer), 0);
        if (strcmp(buffer,"Admin disconnect") == 0){// Se foi kickado
            close(clientSocket);
            cout << "A admin close your connection D:" << endl;
            exit(0);
        }
        if (quit && buffer[0] == 'F' && buffer[1] == '\0'){// Se fechou a conexão por vontade própria
            close(clientSocket);
            cout << "Fechando" << endl;
            exit(0);
        }
        if (bytesRead <= 0) { // Se alguma coisa morreu
            close(clientSocket);
            return ;
        }

        // Exibir os dados recebidos
        cout << string(buffer, bytesRead) << endl;
        
    }
}
int socket_server;
void close_socket(int signal){ // Fecha o programa de forma adequada
    if (signal == SIGINT){
        close(clientSocket);
    }
    cout << 'F' << endl;
    exit(0);   
    char buffer[4096];
    while (true) {
        // Receber dados do servidor
        ssize_t bytesRead = recv(socket_server, buffer, sizeof(buffer), 0);
        if (quit && buffer[0] == 'F' && buffer[1] == '\0'){
            close(clientSocket);
            cout << "Fechando";
            exit(0);
        }
    } 
}

void changeDefaultCtrlC(){
    signal(SIGINT, close_socket);  
}

int startConn(){ // Abre a comunicação com o servidor
    clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket == -1) {
        cerr << "Erro ao criar o socket." << endl;
        exit(1);
    }
    socket_server = clientSocket;
    // Configurar o endereço do servidor
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8080);  // Porta do servidor
    if (inet_pton(AF_INET, "127.0.0.1", &(serverAddress.sin_addr)) <= 0) {
        cerr << "Erro ao configurar o endereço do servidor." << endl;
        close(clientSocket);
        exit(1);
    }

    // Conectar ao servidor
    if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        cerr << "Erro ao conectar ao servidor." << endl;
        close(clientSocket);
        exit(1);
    }

    

    return clientSocket;
}

int main() { // Configura a conexão e le a entrada padrão para mandar para o servidor
    
    cout << "Início do cliente." << endl;
    changeDefaultCtrlC();
    bool connected =  false;
    while (!connected)
    {
        cout << "Connect with the server with '/connect'" << endl;
        string in;
        cin >> in;
        regex regex("\\s+");
        vector<string> tokens(sregex_token_iterator(in.begin(), in.end(), regex, -1), sregex_token_iterator());
        if (tokens[0] == "/connect"){
            startConn();
            
            connected = true;
        }
    }
    thread serverThread(handleServerCommunication, clientSocket);
    while (true) {
        // Le a entrada do usuário
        string userInput;
        getline(cin, userInput);
        if (userInput[0] == '/'){
            regex regex("\\s+");
            string input(userInput);
            vector<string> tokens(sregex_token_iterator(input.begin(), input.end(), regex, -1), sregex_token_iterator());
            if (tokens[0] == "/quit"){
                quit = true;
            }
        }
        
        // Enviar os dados ao servidor
        ssize_t bytesSent = send(clientSocket, userInput.c_str(), userInput.length(), 0);
        if(userInput[0] = '\0'){
            close(clientSocket);
            return 1;
        }
        if (bytesSent == -1) {
            cerr << "Erro ao enviar os dados." << endl;
        }
    }

    // Fechar o socket do cliente
    close(clientSocket);

    cout << "Fim do cliente." << endl;

    return 0;
}
