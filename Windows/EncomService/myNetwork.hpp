#pragma once

#include <WinSock2.h>
#include <WS2tcpip.h>

#include <string>
#include <sstream>
#include <deque>
#include <thread>
#include <mutex>

class NetworkManager
{
public:
	static bool is_little_endian()
	{
		union
		{
			uint32_t i;
			char c[4];
		} u = { 0x01020304 };
		return u.c[0] == 0x04;
	}
	static int reverse_bytes_int(uint32_t num)
	{
		unsigned char bytes[4];
		bytes[0] = (num >> 24) & 0xFF;
		bytes[1] = (num >> 16) & 0xFF;
		bytes[2] = (num >> 8) & 0xFF;
		bytes[3] = (num) & 0xFF;
		int newNum;
		std::memcpy(&newNum, bytes, sizeof(int));
		return newNum;
	}
	// define requests from UI
	enum class UI_REQUESTS
	{
		REQUEST_NONE = 0,
		REQUEST_ACCEPT = 1,
		REQUEST_DECLINE = 2,
		REQUEST_STOP = 3,
		REQUEST_INIT = 4, // should only be called once
	};
	// client accept decline status
	enum class UI_CLIENT_STATUS
	{
		UNDECIDED = 0,
		ACCEPTED = 1,
		DECLINED = 2,
	};

	NetworkManager();
	~NetworkManager();
	// process requests (from UI)
	void process();
	// get current IP address (for UI)
	std::string getMyIP();
	// get connected client IP address (for UI)
	std::string getMyClientIP();
	// get connection status (for UI)
	bool isConnectionGood();
	// get camera status (for UI)
	bool isCameraGood();

private:
	void set_my_ip();
	// start listening for connections
	void ui_accept();
	// decline a request and put it to block list
	void ui_decline();
	// stop current connections and restart background connection
	void ui_stop();
	// stop all connections
	void stop_all();

	// init connection socket
	void init_socket_conn();

	// background thread for listening
	void background();
	// camera thread for local rtsp server
	void localCamera();

	// check in block list
	bool checkBlockList(IN_ADDR newAddr);
	// add to block list
	void addToBlockList(IN_ADDR newAddr);
public:
	bool isLittleEndian = true;
	bool initialized = false;
	// set requests (for UI)
	UI_REQUESTS ui_requests = UI_REQUESTS::REQUEST_NONE;
	std::mutex ui_requests_lock;
	// UI checkbox
	bool enable_camera = false;
	bool toggleCamera = false;
private:
	// ip information
	IN_ADDR my_ip_addr = {};
	IN_ADDR my_client_ip_addr = {};
	// blocklist
	std::deque<IN_ADDR> blocked_ip;
	const unsigned blocked_ip_max = 50;
	std::mutex blocked_ip_lock;
	// set status
	bool up_connection = true; // whether to loop in connection socket thread
	bool up_camera = true;
	// socket setup connection
	std::thread trdConn;
	SOCKET scLocalConn = INVALID_SOCKET;
	SOCKET scClientConn = INVALID_SOCKET;
	const int scLocalConnPort = 65500;
	std::mutex scConn_lock;
	// camera thread
	std::thread trdCamera;
	bool trdCameraRunning = false;
	int rtspServerPort = 1234;
	// set client address for more information
	SOCKADDR_IN myClientSocketAddr = {};
	// status for waiting click events
	UI_CLIENT_STATUS myClientUIStatus = UI_CLIENT_STATUS::UNDECIDED;
};