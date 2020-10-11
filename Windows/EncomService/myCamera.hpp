#pragma once

#include <Windows.h>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libswscale/swscale.h>
}

#include <string>

// reference: https://stackoverflow.com/questions/10715170/receiving-rtsp-stream-using-ffmpeg-library

// url example
// rtsp://192.168.137.158:1234?h264=50000-20-640-480&camera=front

// definition for each shared memory mapping layout
// 2 bytes (protected by a Mutex):
// 1st: whether the camera is active 0 or 1
// 2nd: whether this area is in use 0 or 1 (writing)
// rest actual data

class ComponentCamera
{
struct Picture
{
	uint8_t* buffer = nullptr;
	uint8_t* buffer2 = nullptr;
	AVFrame* picture = nullptr;
	AVFrame* picture_rgb = nullptr;
};

public:
	ComponentCamera();
	~ComponentCamera();

	bool setup(const std::string address, const std::string port);
	void desetup();
	bool toggleCamera();
	void loop();

private:
	bool createMemFile();
	void writeToMemory();
	void endMemory();
	void prepareMemory();

public:
	std::string address;
	std::string port;
	bool initialized = false;

private:
	int video_stream_index = -1;
	bool frontCamera = true;
	int dataSize = 0;
	SwsContext* ctx_convert = nullptr;
	AVFormatContext* ctx_format = nullptr;
	AVCodecContext* ctx_codec = nullptr;
	AVCodec* codec = nullptr;
	AVPacket packet = {};
	Picture picture = {};

	bool lastUsedFile1 = false; // is last written file1 ?
	// shared memory file mapping
	HANDLE hMapFile1;
	LPVOID pBuffer1;
	HANDLE hMapFile2;
	LPVOID pBuffer2;
	// define buffer size
	const DWORD BUFFER_SIZE = 1200000UL;
	// set file names
	const wchar_t* szName1 = TEXT("Global\\EncomCamera1");
	const wchar_t* szName2 = TEXT("Global\\EncomCamera2");
	// set property for mutex
	HANDLE hMutex1;
	HANDLE hMutex2;
	const wchar_t* mutexName1 = TEXT("Global\\EncomCameraMutex1");
	const wchar_t* mutexName2 = TEXT("Global\\EncomCameraMutex2");
};