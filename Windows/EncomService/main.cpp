#include "main.hpp"

UIManager* myUIManager;
std::mutex lock_UI;

NetworkManager* myNetworkManager;
std::mutex lock_Network;

bool GLOB_PROGRAM_EXIT;
std::mutex GLOB_LOCK;

int main(int, char**)
{
	GLOB_PROGRAM_EXIT = false;

	myUIManager = nullptr;
	myNetworkManager = nullptr;

	std::thread thread_UI(task_UI);
	std::thread thread_Network(task_Network);

	while (!GLOB_PROGRAM_EXIT)
	{
		// on this main thread, just sleep
		std::this_thread::sleep_for(std::chrono::milliseconds(50));
	}

	thread_Network.join();
	thread_UI.join();

	return 0;
}

void task_UI()
{
	myUIManager = new UIManager();
	if (myUIManager->isReady())
		myUIManager->loop();
	else
	{
		GLOB_LOCK.lock();
		GLOB_PROGRAM_EXIT = true;
		GLOB_LOCK.unlock();
	}
	lock_UI.lock();
	delete myUIManager;
	lock_UI.unlock();
}

void task_Network()
{
	myNetworkManager = new NetworkManager();
	if (!myNetworkManager->initialized)
	{
		delete myNetworkManager;
		return;
	}
	while (!GLOB_PROGRAM_EXIT && myNetworkManager->initialized)
		myNetworkManager->process();
	lock_Network.lock();
	delete myNetworkManager;
	lock_Network.unlock();
}