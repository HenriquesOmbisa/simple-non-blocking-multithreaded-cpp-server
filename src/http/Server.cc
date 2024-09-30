#include "Server.hpp"

http::Server::Server(){
    setHost("0.0.0.0");
}

void http::Server::init()
{
    serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        std::cerr << "Error creating socket" << std::endl;
        exit(1);
    }
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddr.sin_port = htons(port);
    if (bind(serverSocket, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) 
    {
        std::cerr << "Error binding to socket" << std::endl;
        exit(1);
    }
}
void http::Server::run(){
    if(listen(serverSocket, SOMAXCONN) < 0)
    {
        std::cerr << "Error listening to socket" << std::endl;
        exit(1);
    }
    std::cout << "Server connected successfull - http://localhost:" << port << "" << std::endl;
    epollFD = epoll_create1(0);
    if(epollFD < 0)
    {
        std::cerr << "Error creating epoll" << std::endl;
        exit(1);
    }
    epollEvent.events = EPOLLIN;
    epollEvent.data.fd = serverSocket;
    if(epoll_ctl(epollFD, EPOLL_CTL_ADD, serverSocket, &epollEvent) < 0)
    {
        std::cerr << "Error adding server socket to epoll" << std::endl;
        exit(1);
    }
    while (true)
    {
        int nfds = epoll_wait(epollFD, &epollEvent, MAX_EPOLL_EVENTS, -1);
        if(nfds < 0)
        {
            std::cerr << "Error in epoll_wait" << std::endl;
            exit(1);
        }
        for (int i = 0; i < nfds; i++)
        {
            if(epollEvent.data.fd == serverSocket)
            {
                int clientSocket = accept(serverSocket, NULL, NULL);
                if(clientSocket < 0)
                {
                    std::cerr << "Error accepting connection" << std::endl;
                    exit(1);
                }
                epollEvent.events = EPOLLIN;
                epollEvent.data.fd = clientSocket;
                if(epoll_ctl(epollFD, EPOLL_CTL_ADD, clientSocket, &epollEvent) < 0)
                {
                    std::cerr << "Error adding client socket to epoll" << std::endl;
                    exit(1);
                }
                std::thread clientThread(&http::Server::handleClient, this, clientSocket);
                clientThread.detach();
            }
        }
    }
    close(serverSocket);
}

void http::Server::handleClient(int clientSocket)
{
    char buffer[MAX_BUFFER_SIZE];
    memset(buffer, 0, MAX_BUFFER_SIZE);
    int bytesRead = read(clientSocket, buffer, MAX_BUFFER_SIZE);
    if(bytesRead < 0)
    {
        std::cerr << "Error reading from client socket" << std::endl;
        exit(1);
    }
    if(bytesRead == 0)
    {
        close(clientSocket);
        return;
    }
    std::string request(buffer);
    //std::cout << request << std::endl;
    std::istringstream ss(request);
    std::string method;
    std::string path;
    std::string version;

    ss >> method;
    ss >> path;
    ss >> version;

    auto it =  [this, method, path](){
        for (auto &&route : routes)
        {
            if(route.method == method && route.path == path)
            {
                return route.callback();
            }
        }
        std::string empty = "";
        return empty;
    };

    if (!it().empty())
    {
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + it();
        write(clientSocket, response.c_str(), response.length());
    }else{
        std::string contentType = getMimeType(path);
        std::cout << path << ": " << contentType << std::endl;

        if (!contentType.empty())
        {
            std::string fileContent = Render::file("./static" + path);

            std::string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: " + contentType + "\r\n";
            response += "\r\n" + fileContent;

            write(clientSocket, response.c_str(), response.length());
        }else{
            std::string response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n<h1>404 Not Found</h1>";
            write(clientSocket, response.c_str(), response.length());
        }
    }
    close(clientSocket);
}
void http::Server::get(const std::string& path, std::function<std::string()>&& callback)
{
    method = "GET";
    http::Route route(method, path, std::move(callback));
    routes.push_back(route);
}
void http::Server::Listen(int port_){
    port = port_;
    init();
    run();
}
void http::Server::setHost(std::string host_) { host = htonl(inet_addr(host_.c_str())); }

std::string http::Server::getFileExtension(const std::string& filename)
{
    size_t pos = filename.find_last_of('.');
    if (pos != std::string::npos) {
        return filename.substr(pos + 1);
    }
    return "";
}

std::string http::Server::getMimeType(std::string& fileName) {
    if (fileName.find(".html") != std::string::npos)
        return "text/html";
    else if (fileName.find(".jpg") != std::string::npos || fileName.find(".jpeg") != std::string::npos)
        return "image/jpeg";
    else if(fileName.find(".jpeg") != std::string::npos)
        return "image/jpeg";
    else if (fileName.find(".png") != std::string::npos)
        return "image/png";
    else if (fileName.find(".gif") != std::string::npos)
        return "image/gif";
    else if (fileName.find(".js") != std::string::npos)
        return "application/javascript";
    else if (fileName.find(".css") != std::string::npos)
        return "text/css";
    else if (fileName.find(".mp4") != std::string::npos)
        return "video/mp4";
    else if (fileName.find(".webm") != std::string::npos)
        return "video/webm";
    else if (fileName.find(".ogg") != std::string::npos)
        return "video/ogg";
    else if (fileName.find(".mov") != std::string::npos)
        return "video/mov";
    else if (fileName.find(".mp3") != std::string::npos)
        return "audio/mp3";
    else if (fileName.find(".wav") != std::string::npos)
        return "audio/wav";
    else if (fileName.find(".ttf") != std::string::npos)
        return "font/ttf";
    else
        return "";
}


http::Server::~Server(){}

