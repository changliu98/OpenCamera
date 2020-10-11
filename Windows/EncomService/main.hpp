#pragma once

// include UI manager
#include "myUI.hpp"
// include Network manager
#include "myNetwork.hpp"

#include <thread>
#include <chrono>
#include <mutex>

void task_UI();
void task_Network();