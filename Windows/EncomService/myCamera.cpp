#include "myCamera.hpp"
#include "myUI.hpp"

#include <atlbase.h>
#include <aclapi.h>

#include <thread>
#include <chrono>
#include <algorithm>

extern UIManager* myUIManager;
extern std::mutex lock_UI;

static int interrupt_callback(void* ptr)
{
	time_t* lasttime = (time_t*)ptr;
	if (*lasttime > 0)
	{
		if (time(nullptr) - *lasttime > 10)
			return 1;
	}
	return 0;
}

ComponentCamera::ComponentCamera()
{
	av_register_all();
	avformat_network_init();
	if (!createMemFile()) return;
	initialized = true;
}

ComponentCamera::~ComponentCamera()
{
	desetup();
	avformat_network_deinit();
	if (pBuffer1) UnmapViewOfFile(pBuffer1);
	if (pBuffer2) UnmapViewOfFile(pBuffer2);
	if (hMapFile1) CloseHandle(hMapFile1);
	if (hMapFile2) CloseHandle(hMapFile2);
	if (hMutex1) CloseHandle(hMutex1);
	if (hMutex2) CloseHandle(hMutex2);
}

bool ComponentCamera::setup(const std::string address, const std::string port)
{
	desetup();
	prepareMemory();
	std::this_thread::sleep_for(std::chrono::milliseconds(2000));
	this->address = address;
	this->port = port;
	std::string url = "rtsp://" + address + ":" + port + "?h264=50000-30-640-480&camera=";
	if (frontCamera)
		url += "front";
	else
		url += "back";
	// initialize context
	ctx_format = avformat_alloc_context();
	if (!ctx_format) return false;
	// set timeout
	time_t currentTime = time(nullptr);
	ctx_format->interrupt_callback.callback = interrupt_callback;
	ctx_format->interrupt_callback.opaque = (void*)&currentTime;
	// open input
	if (avformat_open_input(&ctx_format, url.c_str(), NULL, NULL) != 0)
	{
		return false;
	}
	ctx_format->interrupt_callback.callback = nullptr;
	ctx_format->interrupt_callback.opaque = nullptr;
	// get info
	if (avformat_find_stream_info(ctx_format, NULL) < 0) return false;
	// find video stream index
	for (unsigned i = 0; i < ctx_format->nb_streams; i++)
	{
		if (ctx_format->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
		{
			video_stream_index = i;
			break;
		}
	}
	if (video_stream_index < 0) return false;
	// create packet
	av_init_packet(&packet);
	// start reading
	av_read_play(ctx_format);
	// initialize codec
	codec = avcodec_find_decoder(AV_CODEC_ID_H264);
	if (!codec) return false;
	ctx_codec = avcodec_alloc_context3(codec);
	if (!ctx_codec) return false;
	avcodec_get_context_defaults3(ctx_codec, codec);
	avcodec_copy_context(ctx_codec, ctx_format->streams[video_stream_index]->codec);
	if (avcodec_open2(ctx_codec, codec, NULL) < 0) return false;
	// initialize convert images
	ctx_convert = sws_getContext(ctx_codec->width, ctx_codec->height, ctx_codec->pix_fmt,
		ctx_codec->width, ctx_codec->height, AV_PIX_FMT_RGB24, SWS_BICUBIC, NULL, NULL, NULL);
	int size = avpicture_get_size(AV_PIX_FMT_YUV420P, ctx_codec->width, ctx_codec->height);
	picture.buffer = (uint8_t*)(av_malloc(size));
	if (!picture.buffer) return false;
	picture.picture = av_frame_alloc();
	if (!picture.picture) return false;
	picture.picture_rgb = av_frame_alloc();
	if (!picture.picture_rgb) return false;
	size = avpicture_get_size(AV_PIX_FMT_RGB24, ctx_codec->width, ctx_codec->height);
	picture.buffer2 = (uint8_t*)(av_malloc(size));
	if (!picture.buffer2) return false;
	avpicture_fill((AVPicture*)picture.picture, picture.buffer, AV_PIX_FMT_YUV420P, ctx_codec->width, ctx_codec->height);
	avpicture_fill((AVPicture*)picture.picture_rgb, picture.buffer2, AV_PIX_FMT_RGB24, ctx_codec->width, ctx_codec->height);
	av_dict_copy(&picture.picture_rgb->metadata, picture.picture->metadata, AV_DICT_DONT_OVERWRITE);
	// set data size
	dataSize = ctx_codec->width * ctx_codec->height * 3;
	return true;
}

void ComponentCamera::desetup()
{
	video_stream_index = -1;
	dataSize = 0;
	address = "";
	port = "";
	if (ctx_format)
	{
		av_read_pause(ctx_format);
		avformat_close_input(&ctx_format);
		avformat_free_context(ctx_format);
		ctx_format = nullptr;
	}
	if (picture.buffer)
	{
		av_free(picture.buffer);
		picture.buffer = nullptr;
	}
	if (picture.buffer2)
	{
		av_free(picture.buffer2);
		picture.buffer2 = nullptr;
	}
	if (picture.picture)
	{
		av_free(picture.picture);
		picture.picture = nullptr;
	}
	if (picture.picture_rgb)
	{
		av_free(picture.picture_rgb);
		picture.picture_rgb = nullptr;
	}
	if (packet.size)
	{
		av_free_packet(&packet);
	}
	if (ctx_codec)
	{
		avcodec_free_context(&ctx_codec);
		ctx_codec = nullptr;
	}
	memset(&packet, 0, sizeof(packet));
	memset(&picture, 0, sizeof(picture));
	endMemory();
}

void ComponentCamera::loop(bool enabled)
{
	if (av_read_frame(ctx_format, &packet) >= 0 && packet.stream_index == video_stream_index)
	{
		int check = 0;
		int result = avcodec_decode_video2(ctx_codec, picture.picture, &check, &packet);
		if (result && check && enabled)
		{
			sws_scale(ctx_convert, picture.picture->data, picture.picture->linesize, 0,
				ctx_codec->height, picture.picture_rgb->data, picture.picture_rgb->linesize);
			writeToMemory();
		}
		av_free_packet(&packet);
		av_init_packet(&packet);
	}
	else
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

bool ComponentCamera::toggleCamera()
{
	frontCamera = !frontCamera;
	return setup(this->address, this->port);
}

bool ComponentCamera::createMemFile()
{
	SECURITY_DESCRIPTOR SD;
	SECURITY_ATTRIBUTES SA;
	if (!InitializeSecurityDescriptor(&SD, SECURITY_DESCRIPTOR_REVISION)) return false;
	if (!SetSecurityDescriptorDacl(&SD, TRUE, NULL, FALSE)) return false;
	SA.nLength = sizeof(SA);
	SA.lpSecurityDescriptor = &SD;
	SA.bInheritHandle = true;

	// create file handles
	hMapFile1 = CreateFileMapping(INVALID_HANDLE_VALUE, &SA, PAGE_READWRITE, 0, BUFFER_SIZE, szName1);
	if (!hMapFile1)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to allocate memory for Camera shared memory\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		printf("%d\n", GetLastError());
		return false;
	}
	hMapFile2 = CreateFileMapping(INVALID_HANDLE_VALUE, &SA, PAGE_READWRITE, 0, BUFFER_SIZE, szName2);
	if (!hMapFile2)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to allocate memory for Camera shared memory\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		CloseHandle(hMapFile1); hMapFile1 = NULL;
		return false;
	}
	// create buffer pointers
	pBuffer1 = MapViewOfFile(hMapFile1, FILE_MAP_ALL_ACCESS, 0, 0, BUFFER_SIZE);
	if (!pBuffer1)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to get memory map view for Camera shared memory\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		CloseHandle(hMapFile1); hMapFile1 = NULL;
		CloseHandle(hMapFile2); hMapFile2 = NULL;
		return false;
	}
	pBuffer2 = MapViewOfFile(hMapFile2, FILE_MAP_ALL_ACCESS, 0, 0, BUFFER_SIZE);
	if (!pBuffer2)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to get memory map view for Camera shared memory\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		UnmapViewOfFile(pBuffer1); pBuffer1 = nullptr;
		CloseHandle(hMapFile1); hMapFile1 = NULL;
		CloseHandle(hMapFile2); hMapFile2 = NULL;
		return false;
	}
	// create mutex
	hMutex1 = CreateMutex(&SA, FALSE, mutexName1);
	if (!hMutex1)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to create mutex for Camera shared memory\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		UnmapViewOfFile(pBuffer1); pBuffer1 = nullptr;
		UnmapViewOfFile(pBuffer2); pBuffer2 = nullptr;
		CloseHandle(hMapFile1); hMapFile1 = NULL;
		CloseHandle(hMapFile2); hMapFile2 = NULL;
		return false;
	}
	hMutex2 = CreateMutex(&SA, FALSE, mutexName2);
	if (!hMutex2)
	{
		lock_UI.lock();
		if (myUIManager)
			myUIManager->pushMessage("Failed to create mutex for Camera shared memory\n", UIManager::MESSAGE_SEVERITY::M_ERROR);
		lock_UI.unlock();
		UnmapViewOfFile(pBuffer1); pBuffer1 = nullptr;
		UnmapViewOfFile(pBuffer2); pBuffer2 = nullptr;
		CloseHandle(hMapFile1); hMapFile1 = NULL;
		CloseHandle(hMapFile2); hMapFile2 = NULL;
		CloseHandle(hMutex1); hMutex1 = NULL;
		return false;
	}
	// get ownership of mutex
	DWORD dwWaitResult = WaitForSingleObject(hMutex1, INFINITE);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		// set 1st byte (camera stopped)
		memset(pBuffer1, 0, sizeof(uint8_t));
		// set next byte (1 for writing)
		memset(((uint8_t*)pBuffer1 + sizeof(uint8_t)), 1, sizeof(uint8_t));

		ReleaseMutex(hMutex1);
	}
	dwWaitResult = WaitForSingleObject(hMutex2, INFINITE);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		// set 1st byte (camera stopped)
		memset(pBuffer2, 0, sizeof(uint8_t));
		// set next byte (1 for writing)
		memset(((uint8_t*)pBuffer2 + sizeof(uint8_t)), 1, sizeof(uint8_t));

		ReleaseMutex(hMutex2);
	}
	return true;
}

void ComponentCamera::writeToMemory()
{
	// check for available mapping file
	uint8_t val = 0;
	LPVOID ptr = nullptr;
	HANDLE mutex = NULL;
	if (!lastUsedFile1)
	{
		mutex = hMutex1;
		ptr = pBuffer1;
		DWORD dwWaitResult = WaitForSingleObject(hMutex1, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			memset((uint8_t*)pBuffer1 + sizeof(uint8_t), 1, sizeof(uint8_t)); // set to writing
			ReleaseMutex(hMutex1);
		}
	}
	else
	{
		mutex = hMutex2;
		ptr = pBuffer2;
		DWORD dwWaitResult = WaitForSingleObject(hMutex2, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			memset((uint8_t*)pBuffer2 + sizeof(uint8_t), 1, sizeof(uint8_t)); // set to writing
			ReleaseMutex(hMutex2);
		}
	}
	lastUsedFile1 = !lastUsedFile1;
	// rotate the image by 180 degrees
	/*int H = ctx_codec->height;
	int W = ctx_codec->width;
	for (unsigned xx = 0; xx < H; xx++)
	{
		for (unsigned yy = 0; yy < W; yy++)
		{
			((uint8_t*)ptr)[(xx * W + yy) * 3 + 0 + 2] = picture.picture_rgb->data[0][((H - 1 - xx) * W + (W - 1 - yy)) * 3 + 0];
			((uint8_t*)ptr)[(xx * W + yy) * 3 + 1 + 2] = picture.picture_rgb->data[0][((H - 1 - xx) * W + (W - 1 - yy)) * 3 + 1];
			((uint8_t*)ptr)[(xx * W + yy) * 3 + 2 + 2] = picture.picture_rgb->data[0][((H - 1 - xx) * W + (W - 1 - yy)) * 3 + 2];

		}
	}*/
	// memcpy((uint8_t*)ptr + 2 * sizeof(uint8_t), picture.picture_rgb->data[0], dataSize); // copy data to shared memory
	std::reverse_copy(picture.picture_rgb->data[0], picture.picture_rgb->data[0] + dataSize, (uint8_t*)ptr + 2 * sizeof(uint8_t));
	DWORD dwWaitResult = WaitForSingleObject(mutex, INFINITE);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		memset((uint8_t*)ptr+sizeof(uint8_t), 0, sizeof(unsigned char)); // set file ready for read
		ReleaseMutex(mutex);
	}
}

void ComponentCamera::endMemory()
{
	if (pBuffer1 && hMutex1)
	{
		DWORD dwWaitResult = WaitForSingleObject(hMutex1, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			memset(pBuffer1, 0, sizeof(uint8_t)); // set to camera stop
			memset((uint8_t*)pBuffer1 + sizeof(uint8_t), 1, sizeof(uint8_t));
			ReleaseMutex(hMutex1);
		}
	}
	if (pBuffer2 && hMutex2)
	{
		DWORD dwWaitResult = WaitForSingleObject(hMutex2, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			memset(pBuffer2, 0, sizeof(uint8_t)); // set to camera stop
			memset((uint8_t*)pBuffer2 + sizeof(uint8_t), 1, sizeof(uint8_t));
			ReleaseMutex(hMutex2);
		}
	}
}

void ComponentCamera::prepareMemory()
{
	if (pBuffer1 && hMutex1)
	{
		DWORD dwWaitResult = WaitForSingleObject(hMutex1, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			memset(pBuffer1, 1, sizeof(uint8_t)); // set to camera started
			ReleaseMutex(hMutex1);
		}
	}
	if (pBuffer2 && hMutex2)
	{
		DWORD dwWaitResult = WaitForSingleObject(hMutex2, INFINITE);
		if (dwWaitResult == WAIT_OBJECT_0)
		{
			memset(pBuffer2, 1, sizeof(uint8_t)); // set to camera started
			ReleaseMutex(hMutex2);
		}
	}
}