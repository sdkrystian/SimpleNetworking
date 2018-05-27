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

unsigned long long timeMiliseconds();

struct ClientInfo
{
	SOCKET socket;
	std::string clientid;
	sockaddr_in address;
	unsigned long long lastResponse;

	ClientInfo(SOCKET sock, std::string cid)
	{
		socket = sock;
		clientid = cid;
		lastResponse = timeMiliseconds();
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
	void addValue(std::string name, T value)
	{
		std::ostd::stringstream out;
		out << std::fixed << std::setprecision(16) << value;
		data += name + ":<" + out.str() + ">";
	}

	template<class T>
	void addArray(std::string name, std::vector<T> arr)
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

	std::string getString(std::string name)
	{
		return getVar(name);
	}

	int getInt(std::string name)
	{
		return std::stoi(getVar(name));
	}

	long long getLong(std::string name)
	{
		return std::stoll(getVar(name));
	}

	float getFloat(std::string name)
	{
		return std::stof(getVar(name));
	}

	double getDouble(std::string name)
	{
		return std::stod(getVar(name));
	}

	std::vector<std::string> getArray(std::string name)
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
	std::string getVar(std::string name)
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

	virtual std::string parse() = 0;

	int size()
	{
		return parse().length();
	}

	Packet()
	{

	}

protected:
	std::string getValueFromPacket(std::string value)
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
		command = getValueFromPacket("command");
		data =  PacketData(getValueFromPacket("data"));
	}

	std::string parse() override
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
		clientid = getValueFromPacket("clientid");
		request = getValueFromPacket("request");
		data = PacketData(getValueFromPacket("data"));
	}

	std::string parse() override
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
		handlers = handlerlist;
		address.sin_family = AF_INET;
		address.sin_port = htons(port);
		inet_pton(AF_INET, "0.0.0.0", &address.sin_addr);
		addressSize = sizeof(address);
	}

	Server()
	{
		Server(7777, std::vector<Request>());
	}

	bool start()
	{
		if (WSAStartup(MAKEWORD(2, 1), &WSAData()) < 0)
		{
			return false;
		}
		socketUDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		socketTCP = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		if (::bind(socketUDP, (sockaddr*)&address, sizeof(address)) < 0)
		{
			return false;
		}
		if (::bind(socketTCP, (sockaddr*)&address, sizeof(address)) < 0)
		{
			return false;
		}
		if (listen(socketTCP, SOMAXCONN) < 0)
		{
			return false;
		}
		unsigned long mode = 1;
		ioctlsocket(socketTCP, FIONBIO, &mode);
		ioctlsocket(socketUDP, FIONBIO, &mode);
		run();
		return true;
	}

	int send(ClientInfo client, EConnectionType type, ServerPacket packet)
	{
		if (type == EConnectionType::UDP)
		{
			return sendto(socketUDP, packet.parse().c_str(), packet.size(), 0, (sockaddr*)&client.address, sizeof(sockaddr_in));
		}
		else if (type == EConnectionType::TCP)
		{
			return ::send(client.socket, packet.parse().c_str(), packet.size(), NULL);
		}
		else
		{
			return -1;
		}
	}

	void sendall(EConnectionType type, ServerPacket packet)
	{
		for (const ClientInfo& c : clients)
		{
			send(c, type, packet);
		}
	}

	void addHandler(Request request)
	{
		handlers.push_back(request);
	}

	void removeHandler(std::string name)
	{
		std::remove_if(handlers.begin(), handlers.end(), [&](Request reuqest) {return reuqest == name; });
	}

	void close()
	{
		closesocket(socketUDP);
		closesocket(socketTCP);
		WSACleanup();
	}

private:
	SOCKET socketTCP;
	SOCKET socketUDP;
	sockaddr_in address;
	fd_set read;
	int addressSize;
	std::vector<Request> handlers;
	Request noRequest = Request("null", [](std::string clientid, PacketData data) { return; });
	std::thread update;

	void run()
	{
		std::function<void()> serverrun = [&]()
		{
			while (true)
			{
				FD_ZERO(&read);
				FD_SET(socketTCP, &read);
				FD_SET(socketUDP, &read);
				for (ClientInfo& c : clients)
				{
					FD_SET(c.socket, &read);
				}
				if (select(0, &read, NULL, NULL, NULL) > 0)
				{
					if (FD_ISSET(socketTCP, &read))
					{
						SOCKET client;
						sockaddr_in addr;
						int addrSize = sizeof(addr);
						client = accept(socketTCP, (sockaddr*)&addr, &addrSize);
						if (client != INVALID_SOCKET)
						{
							addClient(client);
						}
					}
					for (ClientInfo& c : clients)
					{
						if (isConnected(c))
						{
							if (FD_ISSET(c.socket, &read) || FD_ISSET(socketUDP, &read))
							{
								char buffer[65535];
								sockaddr_in addr;
								int addrSize = sizeof(addr);
								if (recv(c.socket, buffer, sizeof(buffer), 0) > 0 || recvfrom(socketUDP, buffer, sizeof(buffer), 0, (sockaddr*)&addr, &addrSize) > 0)
								{
									ClientPacket packet(buffer);
									ClientInfo& client = getClient(packet.clientid);
									client.lastResponse = timeMiliseconds();
									if (FD_ISSET(socketUDP, &read))
									{
										client.address = addr;
									}
									if (packet.request == "disconnect")
									{
										removeClient(packet.clientid);
									}
									else
									{
										getRequest(packet.request).callback(packet.clientid, packet.data);
									}
								}
							}
						}
					}
				}
			}
		};
		update = std::thread(serverrun);
		return;
	}

	ClientInfo& getClient(std::string clientid)
	{
		return *std::find_if(clients.begin(), clients.end(), [clientid](ClientInfo x) { return x.clientid == clientid; });
	}

	Request& getRequest(std::string request)
	{
		auto result = find_if(handlers.begin(), handlers.end(), [&](Request x) { return x == request; });
		return result == handlers.end() ? noRequest : *result;
	}

	void removeClient(std::string clientid)
	{
		getRequest("disconnect").callback(clientid, PacketData());
		closesocket(getClient(clientid).socket);
		clients.erase(std::remove_if(clients.begin(), clients.end(), [&](ClientInfo x) { return x.clientid == clientid; }), clients.end());
	}

	ClientInfo addClient(SOCKET sock)
	{
		std::string id = generateClientID();
		ClientInfo clientinfo(sock, id);
		clients.push_back(clientinfo);
		ServerPacket packet("setclientid");
		packet.data.addValue("clientid", id);
		send(clientinfo, EConnectionType::TCP, packet);
		getRequest("connect").callback(clientinfo.clientid, PacketData());
		return clientinfo;
	}

	bool isConnected(ClientInfo client)
	{
		ServerPacket packet("heartbeat");
		if (send(client, EConnectionType::TCP, packet) > 0)
		{
			return true;
		}
		else
		{
			removeClient(client.clientid);
			return false;
		}
	}

	std::string generateClientID()
	{
		std::string output = "";
		srand(time(0));
		std::vector<std::string> letters = { "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o", "p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9" };
		for (int i = 0; i < 16; i++)
		{
			output += letters[randomRange(0, letters.size() - 1)];
		}
		return output;
	}

	int randomRange(int min, int max)
	{
		return min + (int)((double)rand() / (RAND_MAX + 1) * (max - min + 1));
	}
};

class Client
{
public:
	std::string clientid;
	int timeout = 10000;

	Client(std::string ipaddr, int portnum, std::vector<Command> handlerlist)
	{		
		handlers = handlerlist;
		address.sin_family = AF_INET;
		address.sin_port = htons(portnum);
		inet_pton(AF_INET, ipaddr.c_str(), &address.sin_addr);
	}

	Client()
	{
		Client("71.234.124.86", 7777, std::vector<Command>());
	}

	bool start()
	{
		if (WSAStartup(MAKEWORD(2, 1), &WSAData()) < 0)
		{
			return false;
		}
		socketUDP = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		socketTCP = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		sockaddr_in addr;
		addr.sin_family = AF_INET;
		inet_pton(AF_INET, "0.0.0.0", &addr.sin_addr);
		if (::bind(socketUDP, (sockaddr*)&addr, sizeof(sockaddr_in)) < 0)
		{
			return false;
		}
		if (::connect(socketTCP, (sockaddr*)&address, sizeof(sockaddr_in)) < 0)
		{
			return false;
		}
		unsigned long mode = 1;
		ioctlsocket(socketTCP, FIONBIO, &mode);
		ioctlsocket(socketUDP, FIONBIO, &mode);
		run();
		return true;
	}

	int send(EConnectionType type, ClientPacket packet)
	{
		if (type == EConnectionType::UDP)
		{
			return sendto(socketUDP, packet.parse().c_str(), packet.size(), 0, (sockaddr*)&address, sizeof(sockaddr_in));
		}
		else if (type == EConnectionType::TCP)
		{
			return ::send(socketTCP, packet.parse().c_str(), packet.size(), NULL);
		}
		else
		{
			return -1;
		}
	}

	void addHandler(Command command)
	{
		handlers.push_back(command);
	}

	void removeHandler(std::string name)
	{
		std::remove_if(handlers.begin(), handlers.end(), [&](Command command) {return command == name; });
	}

	void disconnect()
	{
		send(EConnectionType::TCP, ClientPacket(clientid, "disconnect"));
		getCommand("disconnect").callback(PacketData());
		closesocket(socketUDP);
		closesocket(socketTCP);
		WSACleanup();
		connected = false;
	}

private:
	SOCKET socketTCP;
	SOCKET socketUDP;
	sockaddr_in address;
	fd_set read;
	unsigned long long lastResponse;
	std::vector<Command> handlers;
	Command noCommand = Command("null", [](PacketData data) { return; });
	std::thread update;
	std::thread checkTimeout;
	bool connected = false;

	void run()
	{
		std::function<void()> clientrun = [&]()
		{
			while (true)
			{
				FD_ZERO(&read);
				FD_SET(socketTCP, &read);
				FD_SET(socketUDP, &read);
				if (select(0, &read, NULL, NULL, NULL) > 0)
				{
					if (FD_ISSET(socketTCP, &read) || FD_ISSET(socketUDP, &read))
					{
						char buffer[65535];
						sockaddr_in addr;
						int addrSize = sizeof(addr);
						if (recv(socketTCP, buffer, sizeof(buffer), 0) > 0 || recvfrom(socketUDP, buffer, sizeof(buffer), 0, (sockaddr*)&addr, &addrSize) > 0)
						{
							lastResponse = timeMiliseconds();
							ServerPacket packet(buffer, true);
							if (packet.command == "setclientid")
							{
								clientid =  packet.data.getString("clientid");
								getCommand("connected").callback(packet.data);
								connected = true;
								send(EConnectionType::UDP, ClientPacket(clientid, "connected"));
							}
							else
							{
								getCommand(packet.command).callback(packet.data);
							}
						}
					}
				}
			}
		};
		std::function<void()> checkto = [&]()
		{
			while (true)
			{
				if (connected)
				{
					if (timeMiliseconds() - lastResponse > timeout)
					{
						disconnect();
					}
				}
				Sleep(500);
			}
		};
		update = std::thread(clientrun);
		checkTimeout = std::thread(checkto);
	}

	Command& getCommand(std::string command)
	{
		auto result = find_if(handlers.begin(), handlers.end(), [&](Command x) { return x == command; });
		return result == handlers.end() ? noCommand : *result;
	}
};	

unsigned long long timeMiliseconds()
{
	return std::chrono::system_clock::now().time_since_epoch() / std::chrono::milliseconds(1);
}