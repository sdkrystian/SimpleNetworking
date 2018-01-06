#include "stdafx.h"
#include "Networking.h"
#include <vector>
#include <thread>
#include <functional>

using std::vector;
using std::cout;
using std::cin;
using std::endl;
using std::string;

Server server;
Client client;
void onConnect(string clientid, PacketData data);
void onServerConnected(string clientid, PacketData data);
void onDisconnect(string clientid, PacketData data);
void onClientMessage(string clientid, PacketData data);
void onServerMessage(PacketData data);
void onClientConnected(PacketData data);

void main()	
{
	cout << "Run as server? ";
	bool isServer;
	cin >> isServer;
	if (isServer)
	{
		server = Server(7777, { Request("connect", onConnect), Request("connected", onServerConnected), Request("message", onClientMessage), Request("disconnect", onDisconnect) });
		server.start();
		cout << "Server Started" << endl;
		while (true)
		{
			string in;
			std::getline(cin, in);
			if (in != "")
			{
				for (ClientInfo c : server.clients)
				{
					ServerPacket packet("message");
					packet.data.addValue("message", in);
					server.send(c, EConnectionType::UDP, packet);
				}
			}
		}
	}
	else
	{
		client = Client("192.168.1.254", 7777, { Command("message", onServerMessage), Command("connected", onClientConnected) });
		client.start();
		cout << "Client Started \n" << endl;
		while (true)
		{
			string in;
			std::getline(cin, in);
			if (in != "")
			{
				if (in == "remove")
				{
					client.removeHandler("message");
				}
				ClientPacket packet(client.clientid, "message");
				packet.data.addValue("message", in);
				client.send(EConnectionType::UDP, packet);
			}
		}
	}
	cin.ignore();
}

void onConnect(string clientid, PacketData data)
{
	cout << "Client connecting..." << endl;
}

void onServerConnected(string clientid, PacketData data)
{
	cout << "Client " << clientid << " connected" << endl;
}

void onClientConnected(PacketData data)
{
	cout << "Connected to server" << endl;
}

void onDisconnect(string clientid, PacketData data)
{
	cout << "Client " << clientid << " disconnected" << endl;
}

void onClientMessage(string clientid, PacketData data)
{ 
	cout << "[Message] Client " << clientid << ": " << data.getString("message") << endl;
} 

void onServerMessage(PacketData data)
{ 
	cout << "[Message] Server: " << data.getString("message") << endl;
} 