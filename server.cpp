#include <iostream>
#include <thread>
#include <vector>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <csignal>
#include <atomic>
#include <map>
#include <set>
#include <limits.h>
#include <regex>
#include <arpa/inet.h>


using namespace std;


map<int,pair<string,string>> sockets; // Guarda o vinculo entre o socket, o nickname e o canal atual
map<int, string> ip_list; //Guarda o vinculo socket e IP
map<string,string> channels; // Guarda o vinculo canal e ADM
map<int, set<pair<string,bool>>> mutes; // Serve para as multiplas threads comunicarem que foi mutado/desmutado, em ordem: <socket, <canal, mute/unmute>>

bool isSocketAlive(int socket) { // Verifica se o socket está funcionando
    int error = 0;
    socklen_t len = sizeof(error);
    int result = getsockopt(socket, SOL_SOCKET, SO_ERROR, &error, &len);
    return (error == 0 && result == 0);
}

void removeSocket(int socket){
    close(socket);
    sockets.erase(socket);
    ip_list.erase(socket);
    cout << "Cliente desconectado!" << endl;
}

int send_message(int socket, string message){ // Manda uma mensagem de texto para um socket
    for (int i = 0; i < 5; i++){
        if (send(socket, message.c_str(), message.length(), 0) != -1){
            return 1;
        }
    }
    removeSocket(socket);
    return 0;
}

void broadcastMessage(string message, string channel){ // Manda uma mensagem para todos os sockets que mantem um canal
    for(auto s: sockets){
        if(s.second.first == channel){
            send_message(s.first, message);
        }
    }
}



bool validateChannel(string channel){ // Valida o nome do canal segundo o padrão dado
    if (channel[0] != '&' && channel[0] != '#'){
        return false;
    }
    if (channel.find(",") != string::npos or channel.find(' ')!= string::npos){
        return false;
    }
    return true;
}

bool channelExists(string channel){ // Verifica se o canal ja existe, se ele não existe cria ele com admin vazio
    bool r = (channels.find(channel) != channels.end());
    channels[channel] = "";
    return r;
}

int whoIs(string user){ // Faz uma busca inversa nos sockets para retornar o socket com base no usuario
    for (auto s : sockets){
        if (s.second.second == user){
            return s.first;
        }
    }
    return -1;
}

void kickUser(string user){ // Desconecta um usuario a pedido de um ADM, não tem restrição de canal
    set<int> sockets_exclude;
    for (auto s : sockets){ //Bane todos com o nome especificado
        if (s.second.second == user){
            send_message(s.first, "Admin disconnect");
            sockets_exclude.insert(s.first);
            cout << "Cliente desconectado!" << endl;
        }
    }
    for (auto s : sockets_exclude){ // Limpa a lista de sockets e IPs
        removeSocket(s);
    }
}

bool muteUser(string user, string channel){ //Comunica a todas as threads que um par usuario/canal foi mutado
    int id = whoIs(user);
    if (id == -1){
        return false;
    }
    mutes[id].insert(pair(channel, true));
    return true;
}

bool unmuteUser(string user, string channel){ //Comunica a todas as threads que um par usuario/canal foi desmutado
    int id = whoIs(user);
    if (id == -1){
        return false;
    }
    mutes[id].insert(pair(channel, false));
    return true;
}

string whoIsIP(string user){ // Retorna o IP de um usuario
    return ip_list[whoIs(user)];
}

void handleClient(int clientSocket) { // Gerencia todo o loop de recebimento de informação dos clientes (Um chamado por thread)
    const int bufferSize = 4096;
    char buffer[bufferSize]; // Buffer de mensagens
    buffer[0] = ' ';
    string nick;
    while (nick.empty()) // Enquanto o cliente não tem nick
    {
        memset(buffer, 0, sizeof(buffer));// Limpa o buffer
        send_message(clientSocket, "Connect using '/nickname nickname'"); // Informa o usuario que ele precisa definir um nick
        ssize_t bytesRead = recv(clientSocket, buffer, 60, 0);
        if (bytesRead == -1) {
            cerr << "Erro ao ler os dados do cliente." << endl;
            close(clientSocket);
            return ;
        }
        regex regex("\\s+");
        string input(buffer);
        vector<string> tokens(sregex_token_iterator(input.begin(), input.end(), regex, -1), sregex_token_iterator());
        if(tokens[0] == "/nickname"){ //Le o comando e define o nick como solicitado
            nick = tokens[1];
        }
    }
    string channel;
    bool admin;
    while (channel.empty()) //Enquanto não está em nenhum canal
    {
        memset(buffer, 0, sizeof(buffer));
        send_message(clientSocket, "Join a chat using '/join chat_name'"); // Informa que é necessario entrar em um canal
        ssize_t bytesRead = recv(clientSocket, buffer, 206, 0);
        if (bytesRead == -1) {
            cerr << "Erro ao ler os dados do cliente." << endl;
            close(clientSocket);
            return ;
        }
        regex regex("\\s+");
        string input(buffer);
        vector<string> tokens(sregex_token_iterator(input.begin(), input.end(), regex, -1), sregex_token_iterator());
        if(tokens[0] == "/join"){ // Valida o nome do canal e se for OK entra nele
            if (validateChannel(tokens[1]) && tokens.size() <= 2){
                send_message(clientSocket, "Joined chat: "+ tokens[1]);
                admin = !channelExists(tokens[1]); // Se o canal não existir entra como ADM
                if (admin){
                    channels[tokens[1]] = nick;
                    send_message(clientSocket, "\nYou are admin");
                }
                channel = tokens[1];
            }
        }
    }
    sockets[clientSocket] = pair(channel,nick);// Informa as outras threads que o vinculo canal/nick ja esta definido para este socket
    set<string> mutes_local; // Lista local de onde este usuario ja foi mutado
    while (true)//Le o resto das mensagens
    {
        memset(buffer, 0, sizeof(buffer));//Limpa o buffer
        if (!isSocketAlive(clientSocket)){ // Se o socket morreu mata a thread
            close(clientSocket);
            return ;
        }
        ssize_t bytesRead = recv(clientSocket, buffer, bufferSize - 1, 0); //Le a mensagem
        if(mutes.find(clientSocket) != mutes.end()){ //Verifica se o usuario recebem/perdeu o mute em algum canal
            for (auto mute : mutes[clientSocket]){
                if (mute.second){
                    mutes_local.insert(mute.first);
                }else{
                    mutes_local.erase(mute.first);
                }
            }
            mutes.erase(clientSocket);
        }
        if (bytesRead == -1) {
            cerr << "Erro ao ler os dados do cliente." << endl;
            removeSocket(clientSocket);
            return ;
        }
        if (buffer[0] == '/'){//Verifica se é um comando 
                              //A presença do 'continue' em praticamente todos os if's é para ele não mandar a mensagem do comando para os outros usuarios
            regex regex("\\s+");
            string input(buffer);
            vector<string> tokens(sregex_token_iterator(input.begin(), input.end(), regex, -1), sregex_token_iterator());
            if (tokens[0] == "/quit"){// Kita o cliente do servidor
                send_message(clientSocket, "F");
                close(clientSocket);
                cout << "Cliente desconectado!" << endl;
                return ;
            }
            if (tokens[0] == "/ping"){ // Responde 'pong'
                memset(buffer, 0, sizeof(buffer));
                send_message(clientSocket, "pong");
                continue;
            }
            if (admin){//Comandos especiais de ADM
                if (tokens[0] == "/kick"){ //Kicka um usuario
                    kickUser(tokens[1]);
                    continue;
                }
                if (tokens[0] == "/mute"){ //Muta um usuario no canal atual
                    if(!muteUser(tokens[1], channel)){
                        send_message(clientSocket, "Unknown user");
                    }
                    continue;
                }
                if (tokens[0] == "/unmute"){ // Desmuta um usuario
                    if(!unmuteUser(tokens[1], channel)){
                        send_message(clientSocket, "Unknown user");
                    }
                    continue;
                }
                if(tokens[0] == "/whois"){ // Retorna o IP de um usuario
                    send_message(clientSocket, whoIsIP(tokens[1]));
                    continue;
                }
            }
        }
        if (mutes_local.find(channel) != mutes_local.end()){ //Se o usuario está mutado bloqueia a mensagem dele e informa que eles está mutado
            send_message(clientSocket, "You are muted in this channel");
            continue;
        }
        broadcastMessage(nick+':'+buffer, channel);//Propaga a mensagem para todos os usuarios do canal no padrão informado
        
    }
}

int serv_socket;//Guarda o socket do servidor para poder fecha-lo no final da execução ou em caso de erros

void close_socket(int signal){ // Muda o funcionamento do CTRL+C para finalizar adequadamente o servidor
    if (signal == SIGINT){
        for (auto s : sockets){
            close(s.first);
        }
        close(serv_socket);
        cout << 'F' << endl;
        exit(0);    
    }
}


int main() { // O loop main serve para abrir novas conexões e para configurar o servidor
    signal(SIGINT, close_socket); // Muda o ctrl+c
    cout << "Início do servidor." << endl;

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0); // Cria o socket do servidor
    serv_socket = serverSocket;
    if (serverSocket == -1) {
        cerr << "Erro ao criar o socket." << endl;
        return 1;
    }

    //Faz as configurações de rede do servidor
    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(8080); 

    // Vincular o socket do servidor ao endereço
    if (bind(serverSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
        cerr << "Erro ao vincular o socket ao endereço." << endl;
        close(serverSocket);
        return 1;
    }

    // Aguarda conexões
    if (listen(serverSocket, 5) == -1) {
        cerr << "Erro ao aguardar conexões." << endl;
        close(serverSocket);
        return 1;
    }

    cout << "Servidor aguardando conexões na porta 8080..." << endl;

    // Vetor para armazenar as threads dos clientes
    vector<thread> clientThreads;

    while (true) {
        // Aceitar uma conexão de cliente
        sockaddr_in clientAddress{};
        socklen_t clientAddressSize = sizeof(clientAddress);
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddress, &clientAddressSize);
        if (clientSocket == -1) {
            cerr << "Erro ao aceitar a conexão do cliente." << endl;
            close(serverSocket);
            return 1;
        }
        // Obter informações do endereço do cliente
        char clientIpAddress[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &(clientAddress.sin_addr), clientIpAddress, sizeof(clientIpAddress));
        ip_list[clientSocket] = clientIpAddress;
        sockets[clientSocket] = pair("","");
        cout << "Cliente conectado!" << endl;

        // Criar uma nova thread para tratar a comunicação com o cliente
        thread clientThread(handleClient, clientSocket);

        // Armazenar a thread no vetor
        clientThreads.push_back(move(clientThread));
    }

    // Aguardar a finalização de todas as threads dos clientes
    for (auto& thread : clientThreads) {
        thread.join();
    }
    // Fechar o socket do servidor
    close(serverSocket);

    cout << "Fim do servidor." << endl;

    return 0;
}
