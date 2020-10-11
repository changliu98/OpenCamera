#pragma once

#include <GL/glew.h>

#include <SDL.h>

#include <imgui.h>
#include <imgui_impl_sdl.h>
#include <imgui_impl_opengl3.h>

#include <deque>
#include <string>
#include <utility>
#include <mutex>

namespace ImGui
{
	bool CheckboxCustomized(const char* label, bool* v, ImVec4& color);
	void RenderCheckMarkCustomized(ImDrawList* draw_list, ImVec2 pos, ImU32 col, float sz);
}

// This class manage the UI
class UIManager
{
public:
	enum class MESSAGE_SEVERITY
	{
		M_INFO = 0,
		M_WARNING = 1,
		M_ERROR = 2,
	};

	UIManager();
	~UIManager();
	void loop();
	static void showWindowsMessageError(const std::string message);
	void pushMessage(std::string message, MESSAGE_SEVERITY severity);
	bool isReady();
private:
	void draw_UI();
	void fps_control(Uint32& prev, Uint32& now);
	void clearMessages();

	bool initialize_sdl();
	bool initialize_glew();
	bool initialize_imgui();

	void quit_sdl();
	void quit_imgui();
public:
	const char* glsl_version = "#version 130";
	const char* window_title = "Encom Service";
	const int window_size_width = 600;
	const int window_size_height = 400;
	const int window_fps = 60;
	bool networkManager_called = false;
private:
	bool initialized = false;
	SDL_Window* myWindow = nullptr;
	SDL_GLContext myGLContext = nullptr;

	std::deque<std::pair<std::string, MESSAGE_SEVERITY>> myConnectInfo;
	std::mutex myConnectionInfo_lock;
	const unsigned connect_info_max_len = 400;
	bool myConnectInfoNew = false;

	// set font as segoe ui
	const char* WINDOWS_FONT_SEGOE_UI = "C:\\Windows\\Fonts\\seguisym.ttf";
	const float font_size_title = 24.0f;
	const float font_size_content = 18.0f;
	// set window border color
	const ImVec4 window_color_br = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	// set window foreground color
	const ImVec4 window_color_cf = ImVec4(0.78f, 0.78f, 0.73f, 1.0f);
	// set window title background color
	const ImVec4 window_color_title_bg = ImVec4(0.0f, 0.678f, 1.0f, 1.0f);
	// set title font color
	const ImVec4 window_color_font_title = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
	// set font color inside
	const ImVec4 window_color_font_inside = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	// set radio button color
	const ImVec4 window_color_radio_bt = ImVec4(0.4f, 0.4f, 1.0f, 1.0f);
	// set normal button color
	const ImVec4 window_color_bt = ImVec4(0.0f, 0.678f, 1.0f, 1.0f);
	// set normal button color when hovered
	const ImVec4 window_color_bt_hover = ImVec4(0.4392f, 0.819f, 1.0f, 1.0f);
	// set status indicator - bad
	const ImVec4 window_color_status_bad = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	// set status indicator - good
	const ImVec4 window_color_status_good = ImVec4(0.0f, 0.87f, 0.0f, 1.0f);
	// set message severity - info
	const ImVec4 window_color_message_info = ImVec4(0.0f, 0.0f, 0.0f, 1.0f);
	// set message severity - warning
	const ImVec4 window_color_message_warning = ImVec4(0.929f, 0.545f, 0.0745f, 1.0f);
	// set message severity - error
	const ImVec4 window_color_message_error = ImVec4(0.8f, 0.0f, 0.0f, 1.0f);
};