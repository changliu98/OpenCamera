#include "myUI.hpp"
#include "myNetwork.hpp"

#include <imgui_internal.h>

#include <Windows.h>
#include <shellapi.h>

#include <stdio.h>

#include <chrono>
#include <thread>
#include <mutex>

extern std::mutex GLOB_LOCK;
extern bool GLOB_PROGRAM_EXIT;

extern NetworkManager* myNetworkManager;
extern std::mutex lock_Network;

// customize ImGui checkbox
namespace ImGui
{
	// this functions is modified from ImGui
	bool CheckboxCustomized(const char* label, bool* v)
	{
		ImGuiWindow* window = GetCurrentWindow();
		if (window->SkipItems)
			return false;

		ImGuiContext& g = *GImGui;
		const ImGuiStyle& style = g.Style;
		const ImGuiID id = window->GetID(label);
		const ImVec2 label_size = CalcTextSize(label, NULL, true);

		const float square_sz = GetFrameHeight();
		const ImVec2 pos = window->DC.CursorPos;
		const ImRect total_bb(pos, ImVec2(pos.x + square_sz + (label_size.x > 0.0f ? style.ItemInnerSpacing.x + label_size.x : 0.0f),
			pos.y + label_size.y + style.FramePadding.y * 2.0f));
		ItemSize(total_bb, style.FramePadding.y);
		if (!ItemAdd(total_bb, id))
			return false;

		bool hovered, held;
		bool pressed = ButtonBehavior(total_bb, id, &hovered, &held);
		if (pressed)
		{
			*v = !(*v);
			MarkItemEdited(id);
		}

		const ImRect check_bb(pos, ImVec2(pos.x + square_sz, pos.y + square_sz));
		RenderNavHighlight(total_bb, id);
		RenderFrame(check_bb.Min, check_bb.Max, GetColorU32((held && hovered) ? ImGuiCol_FrameBgActive : hovered ? ImGuiCol_FrameBgHovered : ImGuiCol_FrameBg), true, style.FrameRounding);
		ImU32 check_col = GetColorU32(ImGuiCol_CheckMark);
		if (window->DC.ItemFlags & ImGuiItemFlags_MixedValue)
		{
			// Undocumented tristate/mixed/indeterminate checkbox (#2644)
			ImVec2 pad(ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)), ImMax(1.0f, IM_FLOOR(square_sz / 3.6f)));
			window->DrawList->AddRectFilled(ImVec2(check_bb.Min.x + pad.x, check_bb.Min.y + pad.y),
				ImVec2(check_bb.Max.x - pad.x, check_bb.Max.y- pad.y), check_col, style.FrameRounding);
		}
		else if (*v)
		{
			const float pad = ImMax(1.0f, IM_FLOOR(square_sz / 6.0f));
			RenderCheckMarkCustomized(window->DrawList, ImVec2(check_bb.Min.x + pad, check_bb.Min.y + pad), check_col, square_sz - pad * 2.0f);
		}

		if (g.LogEnabled)
			LogRenderedText(&total_bb.Min, *v ? "[x]" : "[ ]");
		if (label_size.x > 0.0f)
			RenderText(ImVec2(check_bb.Max.x + style.ItemInnerSpacing.x, check_bb.Min.y + style.FramePadding.y), label);

		IMGUI_TEST_ENGINE_ITEM_INFO(id, label, window->DC.ItemFlags | ImGuiItemStatusFlags_Checkable | (*v ? ImGuiItemStatusFlags_Checked : 0));
		return pressed;
	}

	void RenderCheckMarkCustomized(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz)
	{
		// instead of a tick, draw a square
		draw_list->AddRectFilled(pos, ImVec2(pos.x + sz, pos.y + sz), col);
	}
}

UIManager::UIManager() : myConnectInfo()
{
	if (!initialize_sdl()) return;
	if (!initialize_glew())
	{
		quit_sdl();
		return;
	}
	if (!initialize_imgui())
	{
		quit_sdl();
		return;
	}
	initialized = true;
}

UIManager::~UIManager()
{
	quit_imgui();
	quit_sdl();
}

bool UIManager::isReady()
{
	return initialized;
}

void UIManager::loop()
{
	bool done = false;
	Uint32 tNow = SDL_GetTicks();
	Uint32 tPrev = SDL_GetTicks();
	while (!done)
	{
		if (myNetworkManager && myNetworkManager->initialized && (!networkManager_called))
		{
			myNetworkManager->ui_requests_lock.lock();
			myNetworkManager->ui_requests = NetworkManager::UI_REQUESTS::REQUEST_INIT;
			myNetworkManager->ui_requests_lock.unlock();
			networkManager_called = true; // only call once, unless background process not starting correctly
		}
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			// process events
			ImGui_ImplSDL2_ProcessEvent(&event);
			switch (event.type)
			{
			case SDL_QUIT:
				done = true;
				GLOB_LOCK.lock();
				GLOB_PROGRAM_EXIT = true;
				GLOB_LOCK.unlock();
				break;
			case SDL_WINDOWEVENT:
				if (event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(myWindow))
				{
					done = true;
					GLOB_LOCK.lock();
					GLOB_PROGRAM_EXIT = true;
					GLOB_LOCK.unlock();
				}
				break;
			case SDL_KEYDOWN:
				if (event.key.keysym.sym == SDLK_ESCAPE)
				{
					done = true;
					GLOB_LOCK.lock();
					GLOB_PROGRAM_EXIT = true;
					GLOB_LOCK.unlock();
				}
				break;
			}
		}
		// start imgui frame
		ImGui_ImplOpenGL3_NewFrame();
		ImGui_ImplSDL2_NewFrame(myWindow);
		ImGui::NewFrame();
		// draw actual UI
		draw_UI();
		// end frame
		ImGui::Render();
		glViewport(0, 0, window_size_width, window_size_height);
		glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(myWindow);
		// limit fps
		fps_control(tPrev, tNow);
	}
}

void UIManager::draw_UI()
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowTitleAlign, ImVec2(0.5f, 0.5f));
	ImGui::PushStyleColor(ImGuiCol_WindowBg, window_color_cf);
	ImGui::PushStyleColor(ImGuiCol_Border, window_color_br);
	ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);

	{
		// connection info
		ImGui::SetNextWindowPos(ImVec2(0.6f * window_size_width, 0.00f * window_size_height), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(0.4f * window_size_width, 0.34f * window_size_height), ImGuiCond_Always);
		ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		ImGui::PushStyleColor(ImGuiCol_TitleBg, window_color_title_bg);
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, window_color_title_bg);
		ImGui::PushStyleColor(ImGuiCol_Text, window_color_font_title);
		ImGui::Begin("Connection Information", NULL, flags);
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, window_color_font_inside);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

		{
			if (lock_Network.try_lock() && myNetworkManager)
			{
				ImGui::Text("My IP: %s", myNetworkManager->getMyIP().c_str());
				lock_Network.unlock();
				ImGui::Text("Speed: 0");
				ImGui::Text("Sender IP: %s", myNetworkManager->getMyClientIP().c_str());
			}
			else
			{
				ImGui::Text("My IP: 0.0.0.0");
				ImGui::Text("Speed: 0");
				ImGui::Text("Sender IP: 0.0.0.0");
			}
		}

		ImGui::PopFont();
		ImGui::PopStyleColor();
		ImGui::End();

		// Logs and outputs
		ImGui::SetNextWindowPos(ImVec2(0.0f * window_size_width, 0.0f * window_size_height), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(0.6f * window_size_width, 1.0f * window_size_height), ImGuiCond_Always);
		flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		ImGui::PushStyleColor(ImGuiCol_TitleBg, window_color_title_bg);
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, window_color_title_bg);
		ImGui::PushStyleColor(ImGuiCol_Text, window_color_font_title);
		ImGui::Begin("Logs and Outputs", NULL, flags);
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, window_color_font_inside);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

		{
			ImGui::PushTextWrapPos(ImGui::GetFontSize() * 20.0f);
			myConnectionInfo_lock.lock();
			for (unsigned i = 0; i < myConnectInfo.size(); i++)
			{
				ImVec4 textColor = window_color_message_info;
				if(myConnectInfo[i].second == MESSAGE_SEVERITY::M_WARNING)
					textColor = window_color_message_warning;
				else if (myConnectInfo[i].second == MESSAGE_SEVERITY::M_ERROR)
					textColor = window_color_message_error;
				ImGui::TextColored(textColor, myConnectInfo[i].first.c_str());
				if(myConnectInfoNew)
					ImGui::SetScrollHereY();
			}
			if (myConnectInfoNew)
				myConnectInfoNew = false;
			myConnectionInfo_lock.unlock();
			ImGui::PopTextWrapPos();
		}

		ImGui::PopFont();
		ImGui::PopStyleColor();
		ImGui::End();

		// Interaction
		ImGui::SetNextWindowPos(ImVec2(0.6f * window_size_width, 0.34f * window_size_height), ImGuiCond_Always);
		ImGui::SetNextWindowSize(ImVec2(0.4f * window_size_width, 0.66f * window_size_height), ImGuiCond_Always);
		flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
		ImGui::PushStyleColor(ImGuiCol_TitleBg, window_color_title_bg);
		ImGui::PushStyleColor(ImGuiCol_TitleBgActive, window_color_title_bg);
		ImGui::PushStyleColor(ImGuiCol_Text, window_color_font_title);
		ImGui::Begin("Interactive", NULL, flags);
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, window_color_font_inside);
		ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[1]);

		{
			ImGui::PushStyleColor(ImGuiCol_Button, window_color_bt);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, window_color_bt_hover);

			ImGui::SetCursorPos(ImVec2(0.06f * 0.4f * window_size_width, 0.18f * 0.6f * window_size_height));
			if (ImGui::Button("Accept", ImVec2(0.41f * 0.4f * window_size_width, 0.15f * 0.58f * window_size_height)))
			{
				if (myNetworkManager)
				{
					myNetworkManager->ui_requests_lock.lock();
					myNetworkManager->ui_requests = NetworkManager::UI_REQUESTS::REQUEST_ACCEPT;
					myNetworkManager->ui_requests_lock.unlock();
				}
			}

			ImGui::SameLine();

			ImGui::SetCursorPos(ImVec2(0.53f * 0.4f * window_size_width, 0.18f * 0.6f * window_size_height));
			if (ImGui::Button("Decline", ImVec2(0.41f * 0.4f * window_size_width, 0.15f * 0.58f * window_size_height)))
			{
				if (myNetworkManager)
				{
					myNetworkManager->ui_requests_lock.lock();
					myNetworkManager->ui_requests = NetworkManager::UI_REQUESTS::REQUEST_DECLINE;
					myNetworkManager->ui_requests_lock.unlock();
				}
			}

			ImGui::SetCursorPos(ImVec2(0.06f * 0.4f * window_size_width, 0.45f * 0.5f * window_size_height));
			if (ImGui::Button("Stop", ImVec2(0.41f * 0.4f * window_size_width, 0.15f * 0.58f * window_size_height)))
			{
				if (myNetworkManager)
				{
					myNetworkManager->ui_requests_lock.lock();
					myNetworkManager->ui_requests = NetworkManager::UI_REQUESTS::REQUEST_STOP;
					myNetworkManager->ui_requests_lock.unlock();
				}
			}

			ImGui::SameLine();

			ImGui::SetCursorPos(ImVec2(0.53f * 0.4f * window_size_width, 0.45f * 0.5f * window_size_height));
			if (ImGui::Button("Clear", ImVec2(0.41f * 0.4f * window_size_width, 0.15f * 0.58f * window_size_height)))
				clearMessages();

			ImGui::PopStyleColor();
			ImGui::PopStyleColor();

			bool status_good_connection = false;
			bool status_good_camera = false;
			bool status_good_microphone = false;

			float fontSize = ImGui::GetFontSize();
			ImGui::PushStyleColor(ImGuiCol_Button, window_color_bt);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, window_color_bt_hover);
			ImGui::SetCursorPos(ImVec2(0.06f * 0.4f * window_size_width, 0.7f * 0.5f * window_size_height));
			if (ImGui::Button("Switch Camera", ImVec2(0.88f * 0.4f * window_size_width, 0.15f * 0.58f * window_size_height)))
			{
				if (myNetworkManager)
				{
					myNetworkManager->toggleCamera = true;
				}
			}
			ImGui::PopStyleColor();
			ImGui::PopStyleColor();

			ImGui::SetCursorPos(ImVec2(0.06f * 0.4f * window_size_width, 0.85f * 0.5f * window_size_height + fontSize));
			bool fake_connect = true;
			ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
			if (myNetworkManager)
				ImGui::PushStyleColor(ImGuiCol_CheckMark, (myNetworkManager->isConnectionGood()) ? window_color_status_good : window_color_status_bad);
			else
				ImGui::PushStyleColor(ImGuiCol_CheckMark, window_color_status_bad);
			ImGui::CheckboxCustomized("Connection", &fake_connect);
			ImGui::PopStyleColor();
			ImGui::PopItemFlag();

			bool fake_uncheck = false;
			ImGui::SetCursorPos(ImVec2(0.06f * 0.4f * window_size_width, 0.95f * 0.5f * window_size_height + 2 * fontSize));
			if (myNetworkManager)
			{
				ImGui::PushStyleColor(ImGuiCol_CheckMark, (myNetworkManager->isCameraGood()) ? window_color_status_good : window_color_status_bad);
				if (ImGui::CheckboxCustomized("Camera", &myNetworkManager->enable_camera) && myNetworkManager->enable_camera)
					myNetworkManager->test_start_local_camera(); // TODO: remove this after testing
			}
			else
			{
				ImGui::PushStyleColor(ImGuiCol_CheckMark, window_color_status_bad);
				ImGui::CheckboxCustomized("Camera", &fake_uncheck);
			}
			ImGui::PopStyleColor();
		}

		ImGui::PopFont();
		ImGui::PopStyleColor();
		ImGui::End();
	}

	ImGui::PopFont();
	ImGui::PopStyleColor();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar();
	ImGui::PopStyleVar();
}

void UIManager::pushMessage(std::string message, MESSAGE_SEVERITY severity)
{
	if (GLOB_PROGRAM_EXIT) return;
	myConnectionInfo_lock.lock();
	myConnectInfo.push_back(std::make_pair(message, severity));
	while (myConnectInfo.size() > connect_info_max_len)
	{
		myConnectInfo.pop_front();
	}
	myConnectInfoNew = true;
	myConnectionInfo_lock.unlock();
}

void UIManager::clearMessages()
{
	if (GLOB_PROGRAM_EXIT) return;
	myConnectionInfo_lock.lock();
	myConnectInfo.clear();
	myConnectionInfo_lock.unlock();
}

void UIManager::fps_control(Uint32& prev, Uint32& now)
{
	now = SDL_GetTicks();
	Uint32 delta = now - prev;
	Uint32 spf = static_cast<Uint32>(1000 / window_fps);
	if (delta < spf)
		std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<long>(spf - delta)));
	prev = SDL_GetTicks();
}

bool UIManager::initialize_sdl()
{
	// init SDL
	if (SDL_Init(SDL_INIT_VIDEO) != 0)
	{
		showWindowsMessageError(std::string("Error: SDL_Init failed\n") + SDL_GetError());
		return false;
	}
	// setup GL context
	// GL 3.0 + GLSL 130
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
	myWindow = SDL_CreateWindow(window_title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		window_size_width, window_size_height, SDL_WINDOW_OPENGL | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_SHOWN | SDL_WINDOW_ALWAYS_ON_TOP);
	if (!myWindow)
	{
		showWindowsMessageError(std::string("Error: SDL_CreateWindow failed\n") + SDL_GetError());
		return false;
	}
	myGLContext = SDL_GL_CreateContext(myWindow);
	if (!myGLContext)
	{
		showWindowsMessageError(std::string("Error: SDL_GL_CreateContext failed\n") + SDL_GetError());
		return false;
	}
	SDL_GL_MakeCurrent(myWindow, myGLContext);
	SDL_GL_SetSwapInterval(1);
	return true;
}

bool UIManager::initialize_glew()
{
	// init glew
	if (glewInit() != GLEW_OK)
	{
		showWindowsMessageError("Error: glewInit failed\n");
		return false;
	}
	return true;
}

bool UIManager::initialize_imgui()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = NULL; // disable ini file
	ImGui::StyleColorsClassic();
	ImGui_ImplSDL2_InitForOpenGL(myWindow, myGLContext);
	ImGui_ImplOpenGL3_Init(glsl_version);
	ImFont* pFont = io.Fonts->AddFontFromFileTTF(WINDOWS_FONT_SEGOE_UI, font_size_title);
	if (!pFont)
	{
		char message[200];
		sprintf_s(message, 200, "Cannot load Windows font file: %s\nDoes the file exist?", WINDOWS_FONT_SEGOE_UI);
		showWindowsMessageError(std::string(message));
		return false;
	}
	pFont = io.Fonts->AddFontFromFileTTF(WINDOWS_FONT_SEGOE_UI, font_size_content);
	if (!pFont)
	{
		char message[200];
		sprintf_s(message, 200, "Cannot load Windows font file: %s\nDoes the file exist?", WINDOWS_FONT_SEGOE_UI);
		showWindowsMessageError(std::string(message));
		return false;
	}
	return true;
}

void UIManager::quit_sdl()
{
	SDL_GL_DeleteContext(myGLContext);
	SDL_DestroyWindow(myWindow);
	SDL_Quit();
}

void UIManager::quit_imgui()
{
	ImGui_ImplOpenGL3_Shutdown();
	ImGui_ImplSDL2_Shutdown();
	ImGui::DestroyContext();
}

void UIManager::showWindowsMessageError(const std::string message)
{
	// This function is only used for displaying error message that would cause the program terminate
	MessageBoxA(NULL, message.c_str(), "Encom Service Error", MB_OK);
}