    #include <iostream>
    #include <vector>
    #include <unordered_map>
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_ttf.h>
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
    #include <mutex>

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
    #define BUFFER_SIZE 1024*1024

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

std::vector<unsigned char> joinStrings(const std::vector<std::string>& vec, size_t startIndex, const std::string& delimiter = "\n") {
    if (startIndex >= vec.size()) {
        return {};
    }

    std::string result;
    for (size_t i = startIndex; i < vec.size(); ++i) {
        if (!result.empty()) {
            result += delimiter;
        }
        result += vec[i];
    }

    return std::vector<unsigned char>(result.begin(), result.end());
}
      int safe_stoi(const string& str) {
      try {
          return stoi(str);
      } catch (const std::invalid_argument& e) {
          cerr << "Invalid argument: " << str << " cannot be converted to int" << endl;
          return 0;
      } catch (const std::out_of_range& e) {
          cerr << "Out of range: " << str << " is out of integer range" << endl;
          return 0;
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
      vector<string> eventBuffer;
      mutex bufferMutex;
      char tempBuffer[BUFFER_SIZE];

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
            cout << "Error accepting connection on port " << port << ": " << strerror(errno) << endl;
          }
          return false;
        }

        int flags = fcntl(clientSocket, F_GETFL, 0);
        fcntl(clientSocket, F_SETFL, flags | O_NONBLOCK);

        cout << "Client connected on port  " << port << " from " << inet_ntoa(clientAddr.sin_addr) << endl;
        return true;
      }

      vector<vector<string>> readCmd() {
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

      void addEventToBuffer(const string& evt) {
        lock_guard<mutex> lock(bufferMutex);
        eventBuffer.push_back(evt);
      }

      string getEventsFromBuffer() {
        lock_guard<mutex> lock(bufferMutex);
        if (eventBuffer.empty()) {
          return "\n";
        }

        string allEvents;
        for (const auto& evt : eventBuffer) {
          allEvents += evt + "\n";
        }
        eventBuffer.clear();
        return allEvents;
      }

      void handleClientRequests() {
        if (clientSocket == -1) return;

        char tempBuffer[BUFFER_SIZE];
        ssize_t bytesRead = recv(clientSocket, tempBuffer, sizeof(tempBuffer) - 1, 0);

        if (bytesRead > 0) {
          tempBuffer[bytesRead] = '\0';
          string request(tempBuffer);

          if (request == "GET\n") {
            string events = getEventsFromBuffer();
            send(clientSocket, events.c_str(), events.size(), 0);
          }
        } else if (bytesRead == 0 || (bytesRead == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
          cout << "Client disconnected on port " << port << endl;
          close(clientSocket);
          clientSocket = -1;
        }
      }
    };

    int main() {
      //try {
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
          throw runtime_error("SDL init failed: " + string(SDL_GetError()));
        }

        if (TTF_Init() < 0) {
          throw runtime_error("SDL_ttf init failed: " + string(TTF_GetError()));
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

        unordered_map<int, SDL_Texture*> textures;
        TTF_Font* currentFont = nullptr;
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
              evtServer.addEventToBuffer(evt_out);
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
                    safe_stoi(CMD[(l<<2)+1]), 
                    safe_stoi(CMD[(l<<2)+2]), 
                    safe_stoi(CMD[(l<<2)+3]), 
                    safe_stoi(CMD[(l<<2)+4]));
              } else if (!CMD[0].compare("]")) {
                SDL_RenderPresent(renderer);
              } else if (!CMD[0].compare("SC")) {
                CURRENT_COLOR = Color{safe_stoi(CMD[1]), safe_stoi(CMD[2]), safe_stoi(CMD[3])};
                SDL_SetRenderDrawColor(renderer, CURRENT_COLOR.r, CURRENT_COLOR.g, CURRENT_COLOR.b, 255);
              } else if (!CMD[0].compare("FR")) {
                int n = (CMD.size()-1) >> 2;
                for (int l = 0; l < n; l++) {
                  SDL_Rect rect{
                    safe_stoi(CMD[(l<<2)+1]), 
                    safe_stoi(CMD[(l<<2)+2]), 
                    safe_stoi(CMD[(l<<2)+3]), 
                    safe_stoi(CMD[(l<<2)+4])
                  };
                  SDL_RenderFillRect(renderer, &rect);
                }
              } else if (!CMD[0].compare("FC")) {
                int n = (CMD.size()-1) / 3;
                for (int l = 0; l < n; l++)
                  fillCircle(renderer, 
                    safe_stoi(CMD[3*l+1]), 
                    safe_stoi(CMD[3*l+2]), 
                    safe_stoi(CMD[3*l+3]));
              } else if (!CMD[0].compare("[")) {
                SDL_SetRenderDrawColor(renderer, BG_COLOR.r, BG_COLOR.g, BG_COLOR.b, 255);
                SDL_RenderClear(renderer);
                SDL_SetRenderDrawColor(renderer, CURRENT_COLOR.r, CURRENT_COLOR.g, CURRENT_COLOR.b, 255);
              } else if (!CMD[0].compare("SF")) {
                if (CMD.size() > 1) {
                  if (currentFont) {
                    TTF_CloseFont(currentFont);
                  }
                  currentFont = TTF_OpenFont(CMD[1].c_str(), 24); // Default size 24
                  if (!currentFont) {
                    cerr << "Failed to load font: " << TTF_GetError() << endl;
                  }
                }
              } else if (!CMD[0].compare("DT")) {
                if (CMD.size() > 4 && currentFont) {
                  int x = safe_stoi(CMD[1]);
                  int y = safe_stoi(CMD[2]);
                  int w = safe_stoi(CMD[3]);
                  int h = safe_stoi(CMD[4]);
                  string text = CMD[5];

                  SDL_Color sdlColor = {static_cast<Uint8>(CURRENT_COLOR.r), static_cast<Uint8>(CURRENT_COLOR.g), static_cast<Uint8>(CURRENT_COLOR.b), 255};
                  SDL_Surface* textSurface = TTF_RenderText_Blended(currentFont, text.c_str(), sdlColor);
                  if (textSurface) {
                    SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);
                    SDL_FreeSurface(textSurface);

                    if (textTexture) {
                      SDL_Rect destRect{x, y, w, h};
                      SDL_RenderCopy(renderer, textTexture, nullptr, &destRect);
                      SDL_DestroyTexture(textTexture);
                    }
                  } else {
                    cerr << "Failed to render text: " << TTF_GetError() << endl;
                  }
                }
              } else if (!CMD[0].compare("LI")) {
                if (CMD.size() > 4) {
                  int index = safe_stoi(CMD[1]);
                  int width = safe_stoi(CMD[2]);
                  int height = safe_stoi(CMD[3]);
                  int dataSize = width*height*3;
                  cout << index << endl << width << endl << height <<endl;
                  // for (int i = 0; i<CMD.size(); i++)
                  //  cout << CMD[i] << endl;
                  vector<unsigned char> data = joinStrings(CMD,4);
                  //if (recv(cmdServer.clientSocket, data.data(), dataSize, MSG_WAITALL) == dataSize) {
                    //cout << data.data();

                    SDL_Surface* surface = SDL_CreateRGBSurfaceFrom(
                        data.data(),
                        width,
                        height,
                        24,
                        width * 3,
                        0x000000FF,
                        0x0000FF00,
                        0x00FF0000,
                        0
                    );

                    if (!surface) {
                        cerr << "Failed to create surface for texture: " << SDL_GetError() << endl;
                        break;
                    }

                    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
                    SDL_FreeSurface(surface);

                    if (!texture) {
                        cerr << "Failed to create texture: " << SDL_GetError() << endl;
                        break;
                    }

                    if (index == -1) {
                        index = textures.size();
                    } else if (textures.count(index)) {
                        SDL_DestroyTexture(textures[index]);
                    }

                    textures[index] = texture;

                    ////

/*                   
cout << "Bytes (index: " << index << "):" << endl;

const int bytesPerRow = width * 3;
for (int i = 0; i < dataSize; i++) {
    printf("%02X ", data[i]);
    if ((i + 1) % bytesPerRow == 0) {
        cout << endl;
    }
}

if (dataSize % bytesPerRow != 0) {
    cout << endl;
}
*/

                    ///
              //  }
              }
              } else if (!CMD[0].compare("DI")) {
                if (CMD.size() > 3) {
                  int index = safe_stoi(CMD[1]);
                  int x = safe_stoi(CMD[2]);
                  int y = safe_stoi(CMD[3]);

                  if (index < 0) {
                    index = textures.size() + index;
                  }

                  if (textures.count(index)) {
                    SDL_Texture* texture = textures[index];
                    SDL_Rect dstRect{x, y, 0, 0};
                    SDL_QueryTexture(texture, nullptr, nullptr, &dstRect.w, &dstRect.h);
                    SDL_RenderCopy(renderer, texture, nullptr, &dstRect);
                  } else {
                    cerr << "Invalid texture index: " << index << endl;
                  }
                }
              }
            }
          }

          //cmdServer.handleClientRequests();
          evtServer.handleClientRequests();
        }

        for (auto& [_, texture] : textures) {
          SDL_DestroyTexture(texture);
        }

        if (currentFont) {
          TTF_CloseFont(currentFont);
        }

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
      //} catch (const exception& e) {
      //  cerr << "Error: " << e.what() << endl;
      //}
      return 0;
    }


