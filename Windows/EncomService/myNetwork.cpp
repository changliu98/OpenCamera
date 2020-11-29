#include "myNetwork.hpp"
#include "myUI.hpp"
#include "myCamera.hpp"

#include <cstdlib>
#include <ctime>
#include <vector>
#include <stdio.h>

extern bool GLOB_PROGRAM_EXIT;
extern std::mutex GLOB_LOCK;

extern UIManager* myUIManager;
extern std::mutex lock_UI;

NetworkManager::NetworkManager()
{
	// init Winsock version 2.2
	WSADATA data{};
	if (WSAStartup(MAKEWORD(2, 2), &data))
	{
		UIManager::showWindowsMessageError("Failed to initialize Winsock version 2.2");
		return;
	}
	set_my_ip();
	initialized = true;
	isLittleEndian = NetworkManager::is_little_endian();
}

NetworkManager::~NetworkManager()
{
	stop_all();
	WSACleanup();
}

std::string NetworkManager::getMyIP()
{
	char addrbuff[30];
	inet_ntop(AF_INET, &my_ip_addr, addrbuff, 30);
	return std::string(addrbuff);
}

std::string NetworkManager::getMyClientIP()
{
	char addrbuff[30];
	inet_ntop(AF_INET, &my_client_ip_addr, addrbuff, 30);
	return std::string(addrbuff);
}

void NetworkManager::set_my_ip()
{
	memset(&my_ip_addr, 0, sizeof(IN_ADDR));
	// get adapter information
	SOCKADDR_IN testAddr;
	SOCKET testSocket = socket(AF_INET, SOCK_DGRAM, 0);
	if (INVALID_SOCKET == testSocket)
	{
		if (lock_UI.try_lock())
		{
			if(myUIManager)
				myUIManager->pushMessage("Cannot get current IP address\nFailed to open network socket for TEST\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
			lock_UI.unlock();
		}
		return;
	}
	memset(&testAddr, 0, sizeof(testAddr));
	testAddr.sin_family = AF_INET;
	inet_pton(AF_INET, "1.1.1.1", &testAddr.sin_addr);
	testAddr.sin_port = htons(53);
	if (SOCKET_ERROR == connect(testSocket, (SOCKADDR*)&testAddr, sizeof(testAddr)))
	{
		if (lock_UI.try_lock())
		{
			if (myUIManager)
				myUIManager->pushMessage("Cannot get current IP address\nFailed to connect to TEST dns server: 8.8.8.8\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
			lock_UI.unlock();
		}
		closesocket(testSocket);
		return;
	}
	SOCKADDR_IN result;
	int len = sizeof(result);
	if (SOCKET_ERROR == getsockname(testSocket, (SOCKADDR*)&result, &len))
	{
		if (lock_UI.try_lock())
		{
			if (myUIManager)
				myUIManager->pushMessage("Cannot get current IP address\nFailed to get network TEST socket information\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
			lock_UI.unlock();
		}
		closesocket(testSocket);
		return;
	}
	closesocket(testSocket);
	my_ip_addr = result.sin_addr;
}

void NetworkManager::process()
{
	if (GLOB_PROGRAM_EXIT)
		stop_all();
	while (!GLOB_PROGRAM_EXIT)
	{
		// check for UI requests
		switch (ui_requests)
		{
		case UI_REQUESTS::REQUEST_NONE:
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
			break;
		case UI_REQUESTS::REQUEST_ACCEPT:
			ui_accept();
			ui_requests_lock.lock();
			ui_requests = UI_REQUESTS::REQUEST_NONE;
			ui_requests_lock.unlock();
			break;
		case UI_REQUESTS::REQUEST_DECLINE:
			ui_decline();
			ui_requests_lock.lock();
			ui_requests = UI_REQUESTS::REQUEST_NONE;
			ui_requests_lock.unlock();
			break;
		case UI_REQUESTS::REQUEST_STOP:
			ui_stop();
			ui_requests_lock.lock();
			ui_requests = UI_REQUESTS::REQUEST_NONE;
			ui_requests_lock.unlock();
			break;
		case UI_REQUESTS::REQUEST_INIT:
			if (trdConn.joinable())
				trdConn.join();
			trdConn = std::thread(&NetworkManager::background, this);
			ui_requests_lock.lock();
			ui_requests = UI_REQUESTS::REQUEST_NONE;
			ui_requests_lock.unlock();
			break;
		}
	}
}

void NetworkManager::localCamera()
{
	trdCameraRunning = true;
	toggleCamera = false;
	ComponentCamera* camera = new ComponentCamera();
	if (!camera->initialized)
	{
		delete camera;
		trdCameraRunning = false;
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to initialize Camera component", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		return;
	}
	bool success = camera->setup(getMyClientIP(), std::to_string(rtspServerPort));
	if (!success)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to setup FFMPEG" , UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
	}
	lock_UI.lock();
	if (myUIManager)
		myUIManager->pushMessage("Camera thread started", UIManager::MESSAGE_SEVERITY::M_INFO);
	lock_UI.unlock();
	while (up_camera && success)
	{
		if(enable_camera)
			camera->loop();
		if (toggleCamera)
		{
			toggleCamera = false;
			if(!camera->toggleCamera()) break;
		}
	}
	lock_UI.lock();
	if (myUIManager)
		myUIManager->pushMessage("Camera thread stopped", UIManager::MESSAGE_SEVERITY::M_INFO);
	lock_UI.unlock();
	delete camera;
	trdCameraRunning = false;
}

void NetworkManager::background()
{
	init_socket_conn();
	up_connection = true;
	if ((scLocalConn != INVALID_SOCKET))
	{
		while (!GLOB_PROGRAM_EXIT && up_connection)
		{
			if (scClientConn != INVALID_SOCKET)
				closesocket(scClientConn);
			int addrlen = sizeof(SOCKADDR_IN);
			scClientConn = accept(scLocalConn, (SOCKADDR*)&myClientSocketAddr, &addrlen);
			// test connected client in block list
			if (checkBlockList(myClientSocketAddr.sin_addr))
			{
				closesocket(scClientConn);
				scClientConn = INVALID_SOCKET;
				continue;
			}
			if (scClientConn == INVALID_SOCKET)
				std::this_thread::sleep_for(std::chrono::milliseconds(10)); // avoid continous while loop (CPU overhead)
			else
			{
				char addrname[30];
				inet_ntop(AF_INET, &myClientSocketAddr.sin_addr, addrname, 30);
				lock_UI.lock();
				if (myUIManager)
					myUIManager->pushMessage("A new device want to connect\nIP address = " + std::string(addrname) + "\nPlease accept or decline", UIManager::MESSAGE_SEVERITY::M_INFO);
				lock_UI.unlock();
				// enter loop to wait for user click
				myClientUIStatus = UI_CLIENT_STATUS::UNDECIDED; // reset back
				while (true)
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					if (myClientUIStatus != UI_CLIENT_STATUS::UNDECIDED)
						break;
				}
				if (myClientUIStatus == UI_CLIENT_STATUS::DECLINED)
				{
					closesocket(scClientConn);
					scClientConn = INVALID_SOCKET;
					lock_UI.lock();
					if (myUIManager)
						myUIManager->pushMessage("Declined. Device added to block list\n", UIManager::MESSAGE_SEVERITY::M_WARNING);
					lock_UI.unlock();
					addToBlockList(myClientSocketAddr.sin_addr);
				}
				else
				{
					uint32_t tmpVal;
					std::stringstream ss;
					for (int i = 0; i < 4; i++)
					{
						int val;
						recv(scClientConn, (char*)&tmpVal, sizeof(uint32_t), 0);
						if (isLittleEndian)
							val = NetworkManager::reverse_bytes_int(tmpVal);
						else
							val = tmpVal;
						ss << val;
						if (i < 3) ss << ".";
					}
					std::string recvIPAddress = ss.str();
					if (recvIPAddress != "0.0.0.0")
						inet_pton(AF_INET, recvIPAddress.c_str(), &(myClientSocketAddr.sin_addr));
					// get 4 int as IP
					my_client_ip_addr = myClientSocketAddr.sin_addr;
					lock_UI.lock();
					if (myUIManager)
						myUIManager->pushMessage("Accepted. Device IP address = " + getMyClientIP() + "\n", UIManager::MESSAGE_SEVERITY::M_INFO);
					lock_UI.unlock();
					// Get Rtsp Port and Set Global Status (Camera Status)
					tmpVal = rtspServerPort;
					recv(scClientConn, (char*)&tmpVal, sizeof(uint32_t), 0);
					if (isLittleEndian)
						rtspServerPort = NetworkManager::reverse_bytes_int(tmpVal);
					else
						rtspServerPort = tmpVal;

					if (trdCamera.joinable())
						trdCamera.join();
					up_camera = true;
					trdCamera = std::thread(&NetworkManager::localCamera, this);
				}
			}
		}
	}
	else
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));
		lock_UI.lock();
		myUIManager->networkManager_called = false; // set on error, so that background process could restart
		lock_UI.unlock();
	}
}

void NetworkManager::ui_accept()
{
	myClientUIStatus = UI_CLIENT_STATUS::ACCEPTED;
}

void NetworkManager::ui_decline()
{
	myClientUIStatus = UI_CLIENT_STATUS::DECLINED;
}

void NetworkManager::ui_stop()
{
	// stop all connected sockets
	stop_all();
	// reset variables
	lock_UI.lock();
	myUIManager->networkManager_called = false;
	lock_UI.unlock();

	blocked_ip_lock.lock();
	blocked_ip.clear();
	blocked_ip_lock.unlock();

	memset(&my_client_ip_addr, 0, sizeof(IN_ADDR));
}

void NetworkManager::stop_all()
{
	up_camera = false;
	up_connection = false;
	// stop all client sockets
	if (scClientConn != INVALID_SOCKET)
	{
		scConn_lock.lock();
		shutdown(scClientConn, SD_BOTH);
		closesocket(scClientConn);
		scClientConn = INVALID_SOCKET;
		scConn_lock.unlock();
	}
	// stop all local sockets
	if (scLocalConn != INVALID_SOCKET)
	{
		scConn_lock.lock();
		shutdown(scLocalConn, SD_BOTH);
		closesocket(scLocalConn);
		scLocalConn = INVALID_SOCKET;
		scConn_lock.unlock();
	}
	if (trdCamera.joinable())
		trdCamera.join();
	if (trdConn.joinable())
		trdConn.join();
}

void NetworkManager::init_socket_conn()
{
	if (scLocalConn != INVALID_SOCKET)
		closesocket(scLocalConn);
	SOCKADDR_IN hint;
	memset(&hint, 0, sizeof(SOCKADDR_IN));
	hint.sin_family = AF_INET;
	hint.sin_port = htons(scLocalConnPort);
	hint.sin_addr = my_ip_addr;
	int len = sizeof(hint);
	scLocalConn = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (INVALID_SOCKET == scLocalConn)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Cannot create connection socket\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		return;
	}
	// bind local socket
	if (SOCKET_ERROR == bind(scLocalConn, (SOCKADDR*)&hint, len))
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Cannot bind connection socket\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		closesocket(scLocalConn);
		scLocalConn = INVALID_SOCKET;
		return;
	}
	// start listening
	if (SOCKET_ERROR == listen(scLocalConn, 1))
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Cannot start listening on connection socket\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		closesocket(scLocalConn);
		scLocalConn = INVALID_SOCKET;
		return;
	}
	lock_UI.lock();
	if (myUIManager)
		myUIManager->pushMessage("Connection socket created and started\n", UIManager::MESSAGE_SEVERITY::M_INFO);
	lock_UI.unlock();
}

bool NetworkManager::isConnectionGood()
{
	return scClientConn != INVALID_SOCKET;
}

bool NetworkManager::isCameraGood()
{
	return trdCameraRunning;
}

bool NetworkManager::checkBlockList(IN_ADDR newAddr)
{
	bool inlist = false;
	blocked_ip_lock.lock();
	for (unsigned i = 0; i < blocked_ip.size(); i++)
	{
		if (newAddr.S_un.S_addr == blocked_ip[i].S_un.S_addr)
		{
			inlist = true;
			break;
		}
	}
	blocked_ip_lock.unlock();
	return inlist;
}

void NetworkManager::addToBlockList(IN_ADDR newAddr)
{
	blocked_ip_lock.lock();
	blocked_ip.push_back(newAddr);
	while (blocked_ip.size() > blocked_ip_max)
		blocked_ip.pop_front();
	blocked_ip_lock.unlock();
}