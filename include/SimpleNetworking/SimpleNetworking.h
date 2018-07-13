#include <vector>
#include <string>
#include <thread>
#include <algorithm> 
#include <stdio.h>
#include <tchar.h>
#include "stdlib.h"
#include "Ws2tcpip.h"
#include "WinSock2.h"
#include <windows.h>
#include <iostream>
#include <functional>
#include <sstream>
#include <iomanip>
#pragma comment (lib, "Ws2_32.lib")

#define SCK_VERSION2 0x0202

namespace simplenetworking
{
  namespace util
  {
    unsigned long long TimeMiliseconds()
    {
      return std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
    }

    int RandomRange(int min, int max)
    {
      return min + (int)((double)rand() / (RAND_MAX + 1) * (max - min + 1));
    }
  }
  struct ClientInfo
  {
    SOCKET socket;
    std::string clientid;
    sockaddr_in address;
    unsigned long long lastresponse;

    ClientInfo(SOCKET sock, std::string cid)
    {
      socket = sock;
      clientid = cid;
      lastresponse = util::TimeMiliseconds();
    }
  };

  class PacketData
  {
  public:
    std::string data;

    PacketData(std::string dta)
    {
      data = dta;
    }

    PacketData()
    {

    }

    template<class T>
    void AddValue(std::string name, T value)
    {
      std::ostd::stringstream out;
      out << std::fixed << std::setprecision(16) << value;
      data += name + ":<" + out.str() + ">";
    }

    template<class T>
    void AddArray(std::string name, std::vector<T> arr)
    {
      std::string output = name + ":[";
      for (auto i = arr.begin(); i != arr.end(); i++)
      {
        std::ostd::stringstream out;
        out << std::fixed << std::setprecision(16) << *i;
        output += out.str();
        if (i != arr.end() - 1)
        {
          output += ",";
        }
      }
      output += "]";
      data += output;
    }

    std::string GetString(std::string name)
    {
      return GetVar(name);
    }

    int GetInt(std::string name)
    {
      return std::stoi(GetVar(name));
    }

    long long GetLong(std::string name)
    {
      return std::stoll(GetVar(name));
    }

    float GetFloat(std::string name)
    {
      return std::stof(GetVar(name));
    }

    double GetDouble(std::string name)
    {
      return std::stod(GetVar(name));
    }

    std::vector<std::string> GetArray(std::string name)
    {
      std::vector<std::string> output;
      std::string input = data;
      int start = input.find(name);
      if (start == std::string::npos)
      {
        return output;
      }
      input = input.substr(start + name.length() + 2);
      int end = input.find_first_of("]");
      std::string list = input.substr(0, end);
      std::string cur;
      while (list.find(",", 0) != std::string::npos)
      {
        int pos = list.find(",", 0);
        cur = list.substr(0, pos);
        list.erase(0, pos + 1);
        output.push_back(cur);
      }
      output.push_back(list);
      return output;
    }
  private:
    std::string GetVar(std::string name)
    {
      std::string output = "";
      std::string input = data;
      int start = input.find(name);
      if (start == std::string::npos)
      {
        return output;
      }
      input = input.substr(start + name.length() + 2);
      int end = input.find_first_of(">");
      output = input.substr(0, end);
      return output;
    }
  };

  class Request
  {
  public:
    std::string name;
    std::function<void(std::string clientid, PacketData data)> callback;

    Request(std::string n, std::function<void(std::string clientid, PacketData data)> cb)
    {
      name = n;
      callback = cb;
    }

    bool operator==(std::string& other)
    {
      return name == other;
    }
  };

  class Command
  {
  public:
    std::string name;
    std::function<void(PacketData data)> callback;

    Command(std::string n, std::function<void(PacketData data)> cb)
    {
      name = n;
      callback = cb;
    }

    bool operator==(std::string& other)
    {
      return name == other;
    }
  };

  class Packet
  {
  public:
    std::string rawpacket;
    PacketData data;

    virtual std::string Parse() = 0;

    int Size()
    {
      return Parse().length();
    }

    Packet()
    {

    }

  protected:
    std::string GetValueFromPacket(std::string value)
    {
      std::string output = "";
      int start = rawpacket.find(value);
      if (start == std::string::npos)
      {
        return output;
      }
      std::string trimmed = rawpacket.substr(start);
      start = trimmed.find_first_of("=") + 1;
      if (start == std::string::npos)
      {
        return output;
      }
      int end = trimmed.find_first_of(";");
      output = trimmed.substr(start, end - start);
      return output;
    }
  };

  class ServerPacket : public Packet
  {
  public:
    std::string command;

    ServerPacket(std::string cmd, std::string dta)
    {
      command = cmd;
      data = PacketData(dta);
    }

    ServerPacket(std::string cmd, PacketData dta)
    {
      command = cmd;
      data = dta;
    }

    ServerPacket(std::string cmd)
    {
      command = cmd;
    }

    ServerPacket(char* buffer, bool isBuffer)
    {
      rawpacket = std::string(buffer);
      command = GetValueFromPacket("command");
      data = PacketData(GetValueFromPacket("data"));
    }

    std::string Parse() override
    {
      return "command=" + command + ";data=" + data.data + '\0';
    }
  };

  class ClientPacket : public Packet
  {
  public:
    std::string clientid;
    std::string request;

    ClientPacket(std::string cid, std::string rqst, std::string dta)
    {
      clientid = cid;
      request = rqst;
      data = PacketData(dta);
    }

    ClientPacket(std::string cid, std::string rqst, PacketData dta)
    {
      clientid = cid;
      request = rqst;
      data = dta;
    }

    ClientPacket(std::string cid, std::string rqst)
    {
      clientid = cid;
      request = rqst;
      data = PacketData();
    }

    ClientPacket(char* buffer)
    {
      rawpacket = std::string(buffer);
      clientid = GetValueFromPacket("clientid");
      request = GetValueFromPacket("request");
      data = PacketData(GetValueFromPacket("data"));
    }

    std::string Parse() override
    {
      return "clientid=" + clientid + ";request=" + request + ";data=" + data.data + '\0';
    }
  };

  enum EConnectionType
  {
    TCP,
    UDP
  };

  class Server
  {
  public:
    int port;
    int timeout = 10000;
    std::vector<ClientInfo> clients;

    Server(int portnum, std::vector<Request> handlerlist)
    {
      port = portnum;
      handlers_ = handlerlist;
      address_.sin_family = AF_INET;
      address_.sin_port = htons(port);
      inet_pton(AF_INET, "0.0.0.0", &address_.sin_addr);
      addresssize_ = sizeof(address_);
    }

    Server()
    {
      Server(7777, std::vector<Request>());
    }

    bool Start()
    {
      if (WSAStartup(MAKEWORD(2, 1), &WSAData()) < 0)
      {
        return false;
      }
      socketudp_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      sockettcp_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (::bind(socketudp_, (sockaddr*)&address_, sizeof(address_)) < 0)
      {
        return false;
      }
      if (::bind(sockettcp_, (sockaddr*)&address_, sizeof(address_)) < 0)
      {
        return false;
      }
      if (listen(sockettcp_, SOMAXCONN) < 0)
      {
        return false;
      }
      unsigned long mode = 1;
      ioctlsocket(sockettcp_, FIONBIO, &mode);
      ioctlsocket(socketudp_, FIONBIO, &mode);
      Run();
      return true;
    }

    int Send(ClientInfo client, EConnectionType type, ServerPacket packet)
    {
      if (type == EConnectionType::UDP)
      {
        return sendto(socketudp_, packet.Parse().c_str(), packet.Size(), 0, (sockaddr*)&client.address, sizeof(sockaddr_in));
      }
      else if (type == EConnectionType::TCP)
      {
        return ::send(client.socket, packet.Parse().c_str(), packet.Size(), NULL);
      }
      else
      {
        return -1;
      }
    }

    void SendAll(EConnectionType type, ServerPacket packet)
    {
      for (const ClientInfo& c : clients)
      {
        Send(c, type, packet);
      }
    }

    void AddHandler(Request request)
    {
      handlers_.push_back(request);
    }

    void RemoveHandler(std::string name)
    {
      std::remove_if(handlers_.begin(), handlers_.end(), [&] (Request reuqest) {return reuqest == name; });
    }

    void Close()
    {
      closesocket(socketudp_);
      closesocket(sockettcp_);
      WSACleanup();
    }

  private:
    SOCKET sockettcp_;
    SOCKET socketudp_;
    sockaddr_in address_;
    fd_set read_;
    int addresssize_;
    std::vector<Request> handlers_;
    Request norequest_ = Request("null", [] (std::string clientid, PacketData data) { return; });
    std::thread update_;

    void Run()
    {
      std::function<void()> serverrun = [&] () 
      {
        while (true)
        {
          FD_ZERO(&read_);
          FD_SET(sockettcp_, &read_);
          FD_SET(socketudp_, &read_);
          for (ClientInfo& c : clients)
          {
            FD_SET(c.socket, &read_);
          }
          if (select(0, &read_, NULL, NULL, NULL) > 0)
          {
            if (FD_ISSET(sockettcp_, &read_))
            {
              SOCKET client;
              sockaddr_in addr;
              int addrsize = sizeof(addr);
              client = accept(sockettcp_, (sockaddr*)&addr, &addrsize);
              if (client != INVALID_SOCKET)
              {
                AddClient(client);
              }
            }
            for (ClientInfo& c : clients)
            {
              if (IsConnected(c))
              {
                if (FD_ISSET(c.socket, &read_) || FD_ISSET(socketudp_, &read_))
                {
                  char buffer[65535];
                  sockaddr_in addr;
                  int addrsize = sizeof(addr);
                  if (recv(c.socket, buffer, sizeof(buffer), 0) > 0 || recvfrom(socketudp_, buffer, sizeof(buffer), 0, (sockaddr*)&addr, &addrsize) > 0)
                  {
                    ClientPacket packet(buffer);
                    ClientInfo& client = GetClient(packet.clientid);
                    client.lastresponse = util::TimeMiliseconds();
                    if (FD_ISSET(socketudp_, &read_))
                    {
                      client.address = addr;
                    }
                    if (packet.request == "disconnect")
                    {
                      RemoveClient(packet.clientid);
                    }
                    else
                    {
                      GetRequest(packet.request).callback(packet.clientid, packet.data);
                    }
                  }
                }
              }
            }
          }
        }
      };
      update_ = std::thread(serverrun);
    }

    ClientInfo& GetClient(std::string clientid)
    {
      return *std::find_if(clients.begin(), clients.end(), [clientid] (ClientInfo x) { return x.clientid == clientid; });
    }

    Request& GetRequest(std::string request)
    {
      auto result = find_if(handlers_.begin(), handlers_.end(), [&] (Request x) { return x == request; });
      return result == handlers_.end() ? norequest_ : *result;
    }

    void RemoveClient(std::string clientid)
    {
      GetRequest("disconnect").callback(clientid, PacketData());
      closesocket(GetClient(clientid).socket);
      clients.erase(std::remove_if(clients.begin(), clients.end(), [&] (ClientInfo x) { return x.clientid == clientid; }), clients.end());
    }

    ClientInfo AddClient(SOCKET sock)
    {
      std::string id = GenerateClientID();
      ClientInfo clientinfo(sock, id);
      clients.push_back(clientinfo);
      ServerPacket packet("setclientid");
      packet.data.AddValue("clientid", id);
      Send(clientinfo, EConnectionType::TCP, packet);
      GetRequest("connect").callback(clientinfo.clientid, PacketData());
      return clientinfo;
    }

    bool IsConnected(ClientInfo client)
    {
      ServerPacket packet("heartbeat");
      if (Send(client, EConnectionType::TCP, packet) > 0)
      {
        return true;
      }
      else
      {
        RemoveClient(client.clientid);
        return false;
      }
    }

    std::string GenerateClientID()
    {
      std::string output = "";
      srand(time(0));
      std::vector<std::string> letters = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
      for (int i = 0; i < 16; i++)
      {
        output += letters[util::RandomRange(0, letters.size() - 1)];
      }
      return output;
    }
  };

  class Client
  {
  public:
    std::string clientid;
    int timeout = 10000;

    Client(std::string ipaddr, int portnum, std::vector<Command> handlerlist)
    {
      handlers_ = handlerlist;
      address_.sin_family = AF_INET;
      address_.sin_port = htons(portnum);
      inet_pton(AF_INET, ipaddr.c_str(), &address_.sin_addr);
    }

    Client()
    {
      Client("0.0.0.0", 7777, std::vector<Command>());
    }

    bool Start()
    {
      if (WSAStartup(MAKEWORD(2, 1), &WSAData()) < 0)
      {
        return false;
      }
      socketudp_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
      sockettcp_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
      sockaddr_in addr;
      addr.sin_family = AF_INET;
      inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
      if (::bind(socketudp_, (sockaddr*)&addr, sizeof(sockaddr_in)) < 0)
      {
        return false;
      }
      if (::connect(sockettcp_, (sockaddr*)&address_, sizeof(sockaddr_in)) < 0)
      {
        return false;
      }
      unsigned long mode = 1;
      ioctlsocket(sockettcp_, FIONBIO, &mode);
      ioctlsocket(socketudp_, FIONBIO, &mode);
      Run();
      return true;
    }

    int Send(EConnectionType type, ClientPacket packet)
    {
      if (type == EConnectionType::UDP)
      {
        return sendto(socketudp_, packet.Parse().c_str(), packet.Size(), 0, (sockaddr*)&address_, sizeof(sockaddr_in));
      }
      else if (type == EConnectionType::TCP)
      {
        return ::send(sockettcp_, packet.Parse().c_str(), packet.Size(), NULL);
      }
      else
      {
        return -1;
      }
    }

    void AddHandler(Command command)
    {
      handlers_.push_back(command);
    }

    void RemoveHandler(std::string name)
    {
      std::remove_if(handlers_.begin(), handlers_.end(), [&] (Command command) {return command == name; });
    }

    void Disconnect()
    {
      Send(EConnectionType::TCP, ClientPacket(clientid, "disconnect"));
      GetCommand("disconnect").callback(PacketData());
      closesocket(socketudp_);
      closesocket(sockettcp_);
      WSACleanup();
      connected_ = false;
    }

  private:
    SOCKET sockettcp_;
    SOCKET socketudp_;
    sockaddr_in address_;
    fd_set read_;
    unsigned long long lastresponse_;
    std::vector<Command> handlers_;
    Command nocommand_ = Command("null", [] (PacketData data) {});
    std::thread update_;
    std::thread checktimeout_;
    bool connected_ = false;

    void Run()
    {
      std::function<void()> clientrun = [&] () 
      {
        while (true)
        {
          FD_ZERO(&read_);
          FD_SET(sockettcp_, &read_);
          FD_SET(socketudp_, &read_);
          if (select(0, &read_, NULL, NULL, NULL) > 0)
          {
            if (FD_ISSET(sockettcp_, &read_) || FD_ISSET(socketudp_, &read_))
            {
              char buffer[65535];
              sockaddr_in addr;
              int addrsize = sizeof(addr);
              if (recv(sockettcp_, buffer, sizeof(buffer), 0) > 0 || recvfrom(socketudp_, buffer, sizeof(buffer), 0, (sockaddr*)&addr, &addrsize) > 0)
              {
                lastresponse_ = util::TimeMiliseconds();
                ServerPacket packet(buffer, true);
                if (packet.command == "setclientid")
                {
                  clientid = packet.data.GetString("clientid");
                  GetCommand("connected").callback(packet.data);
                  connected_ = true;
                  Send(EConnectionType::UDP, ClientPacket(clientid, "connected"));
                }
                else
                {
                  GetCommand(packet.command).callback(packet.data);
                }
              }
            }
          }
        }
      };
      std::function<void()> checkto = [&] () {
        while (true)
        {
          if (connected_)
          {
            if (util::TimeMiliseconds() - lastresponse_ > timeout)
            {
              Disconnect();
            }
          }
          Sleep(500);
        }
      };
      update_ = std::thread(clientrun);
      checktimeout_ = std::thread(checkto);
    }

    Command& GetCommand(std::string command)
    {
      auto result = find_if(handlers_.begin(), handlers_.end(), [&] (Command x) { return x == command; });
      return result == handlers_.end() ? nocommand_ : *result;
    }
  };
}