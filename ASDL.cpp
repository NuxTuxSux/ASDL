#include <iostream>
#include <vector>
#include <SDL2/SDL.h>
#include <unistd.h>
#include <array>
#include <sstream>
#include <stdexcept>
#include <errno.h>
#include <math.h>
#include <ctime>
#include <thread>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>

using namespace std;

struct Color {
    int r, g, b;
};

#define FULLSCREEN 0
#define WINDOW_WIDTH 1024
#define WINDOW_HEIGHT 768
#define BG_COLOR Color{0,0,0}
#define CMD_PORT 12345
#define EVT_PORT 12346
#define BUFFER_SIZE 1024

void fillCircle(SDL_Renderer* renderer, int cX, int cY, int R) {
    int r = static_cast<int>(R / sqrt(2));
    SDL_Rect rect{cX - r, cY - r, 2*r, 2*r};
    SDL_RenderFillRect(renderer, &rect);
    
    for (int e = r; e <= R; e++) {
        int d = static_cast<int>(sqrt(R*R - e*e));
        SDL_RenderDrawLine(renderer, cX-d, cY-e, cX+d, cY-e);
        SDL_RenderDrawLine(renderer, cX-d, cY+e, cX+d, cY+e);
        SDL_RenderDrawLine(renderer, cX+e, cY-d, cX+e, cY+d);
        SDL_RenderDrawLine(renderer, cX-e, cY-d, cX-e, cY+d);    
    }
}

vector<string> split(const string& str, char delimiter = ' ') {
    vector<string> tokens;
    size_t start = 0, end = 0;
    while ((end = str.find(delimiter, start)) != string::npos) {
        tokens.push_back(str.substr(start, end - start));
        start = end + 1;
    }
    if (start < str.length()) {
        tokens.push_back(str.substr(start));
    }
    return tokens;
}

class TCPServer {
public:
    int serverSocket;
    int clientSocket;
    string buffer;
    int port;
    
    TCPServer(int _port) : port(_port) {
        serverSocket = socket(AF_INET, SOCK_STREAM, 0);
        if (serverSocket == -1) {
            throw runtime_error("Error creating socket: " + string(strerror(errno)));
        }
        
        int opt = 1;
        setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        sockaddr_in serverAddr{};
        serverAddr.sin_family = AF_INET;
        serverAddr.sin_addr.s_addr = INADDR_ANY;
        serverAddr.sin_port = htons(port);
        
        if (::bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
            throw runtime_error("Error binding socket: " + string(strerror(errno)));
        }
        
        if (listen(serverSocket, 1) < 0) {
            throw runtime_error("Error listening: " + string(strerror(errno)));
        }
        
        clientSocket = -1;
        cout << "Server init on port " << port << endl;
    }
    
    ~TCPServer() {
        if (clientSocket != -1) close(clientSocket);
        if (serverSocket != -1) close(serverSocket);
    }
    
    bool acceptConnection() {
        if (clientSocket != -1) {
            close(clientSocket);
            clientSocket = -1;
        }
        
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSocket == -1) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                cout << "Error accepting connection on porta" << port << ": " << strerror(errno) << endl;
            }
            return false;
        }
        
        int flags = fcntl(clientSocket, F_GETFL, 0);
        fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);
        
        cout << "Client connected on port  " << port << " da " << inet_ntoa(clientAddr.sin_addr) << endl;
        return true;
    }
    
    vector<vector<string>> readCmd() {
        char tempBuffer[BUFFER_SIZE];
        ssize_t bytesRead = recv(clientSocket, tempBuffer, sizeof(tempBuffer) - 1, 0);
        
        if (bytesRead > 0) {
            tempBuffer[bytesRead] = '\0';
            buffer += string(tempBuffer);
            
            vector<vector<string>> commands;
            size_t newlinePos;
            while ((newlinePos = buffer.find('\n')) != string::npos) {
                string completeLine = buffer.substr(0, newlinePos);
                buffer.erase(0, newlinePos + 1);
                vector<string> cmd = split(completeLine);
		if (!cmd.empty()) {
		//    cout << "Cmd received on port" << port << ": " << cmd[0] << endl;
		    commands.push_back(cmd);
                }
            }
            return commands;
        } else if (bytesRead == 0 || (bytesRead == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
            cout << "Client disconnected on port " << port << endl;
            close(clientSocket);
            clientSocket = -1;
        }
        
        return {};
    }
    
    bool sendEvent(const string& evt) {
        if (clientSocket == -1) return false;
        
        string evt_with_newline = evt + "\n";
        ssize_t bytesSent = send(clientSocket, evt_with_newline.c_str(), evt_with_newline.size(), 0);
        return bytesSent == static_cast<ssize_t>(evt_with_newline.size());
    }
};

int main() {
    try {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
            throw runtime_error("SDL init failed: " + string(SDL_GetError()));
        }

        SDL_Window* window = SDL_CreateWindow(
            "SDL Display Server",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            WINDOW_WIDTH, WINDOW_HEIGHT,
            SDL_WINDOW_SHOWN | (FULLSCREEN ? SDL_WINDOW_FULLSCREEN : 0)
        );
        
        if (!window) {
            throw runtime_error("Window creation failed " + string(SDL_GetError()));
        }

        SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
        if (!renderer) {
            throw runtime_error("Render creation failed: " + string(SDL_GetError()));
        }

        TCPServer cmdServer(CMD_PORT);
        TCPServer evtServer(EVT_PORT);
        Color CURRENT_COLOR{255,255,255};
        
        cout << "Server started. Waiting for connection..." << endl;
        
        SDL_Event event;
        size_t pos;
        bool quit = false;

        SDL_SetRenderDrawColor(renderer, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, 255);
        SDL_RenderClear(renderer);
        SDL_RenderPresent(renderer);
                while (!quit) {
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    quit = true;
                    break;
                } else if (event.type == SDL_KEYDOWN && evtServer.clientSocket != -1) {
                    string evt_out = "K " + to_string(event.key.keysym.sym);
                    evtServer.sendEvent(evt_out);
                }
            }

            if (cmdServer.clientSocket == -1) {
                cmdServer.acceptConnection();
            }
            if (evtServer.clientSocket == -1) {
                evtServer.acceptConnection();
            }
            
            if (cmdServer.clientSocket != -1) {
                auto commands = cmdServer.readCmd();
                for (const auto& CMD : commands) {
                    if (!CMD[0].compare("DL")) {
                        int n = (CMD.size()-1) >> 2;
                        for (int l = 0; l < n; l++)
                            SDL_RenderDrawLine(renderer, 
                                stoi(CMD[(l<<2)+1],&pos), 
                                stoi(CMD[(l<<2)+2],&pos), 
                                stoi(CMD[(l<<2)+3],&pos), 
                                stoi(CMD[(l<<2)+4],&pos));
                    } else if (!CMD[0].compare("]")) {
                        SDL_RenderPresent(renderer);
                    } else if (!CMD[0].compare("SC")) {
                        CURRENT_COLOR = Color{stoi(CMD[1],&pos), stoi(CMD[2],&pos), stoi(CMD[3],&pos)};
                        SDL_SetRenderDrawColor(renderer, CURRENT_COLOR.r, CURRENT_COLOR.g, CURRENT_COLOR.b, 255);
            } else if (!CMD[0].compare("FR")) {
                        int n = (CMD.size()-1) >> 2;
                        for (int l = 0; l < n; l++) {
                            SDL_Rect rect{
                                stoi(CMD[(l<<2)+1],&pos), 
                                stoi(CMD[(l<<2)+2],&pos), 
                                stoi(CMD[(l<<2)+3],&pos), 
                                stoi(CMD[(l<<2)+4],&pos)
                            };
                            SDL_RenderFillRect(renderer, &rect);
                        }
                    } else if (!CMD[0].compare("FC")) {
                        int n = (CMD.size()-1) / 3;
                        for (int l = 0; l < n; l++)
                            fillCircle(renderer, 
                                stoi(CMD[3*l+1],&pos), 
                                stoi(CMD[3*l+2],&pos), 
                                stoi(CMD[3*l+3],&pos));
                    } else if (!CMD[0].compare("[")) {
                        SDL_SetRenderDrawColor(renderer, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, 255);
                        SDL_RenderClear(renderer);
                        SDL_SetRenderDrawColor(renderer, CURRENT_COLOR.r, CURRENT_COLOR.g, CURRENT_COLOR.b, 255);
                    }
                }
            }
            
            this_thread::sleep_for(chrono::milliseconds(5));
        }
        
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        
    } catch (const exception& e) {
        cerr << "Error: " << e.what() << endl;
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}
