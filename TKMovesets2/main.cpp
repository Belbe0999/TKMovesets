﻿#ifdef BUILD_TYPE_DEBUG
#define _CRTDBG_MAP_ALLOC
#include <crtdbg.h>
#include <stdlib.h>
#endif

#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stdio.h>
#include <format>
#include <windows.h>
#include <locale>
#include <filesystem>
#include <fstream>

#include <Sig/Sig.hpp>
#include "MainWindow.hpp"
#include "Localization.hpp"
#include "GameAddressesFile.hpp"

#include "constants.h"

// -- Static helpers -- //

static void WriteToLogFile(const std::string& content, bool append=true)
{
	DEBUG_LOG("%s\n", content.c_str());
	/*
	std::ofstream file;

	if (append) {
		file.open(PROGRAM_DEBUG_LOG_FILE, std::ios_base::app);
	}
	else {
		file.open(PROGRAM_DEBUG_LOG_FILE);
	}

	if (!file.fail()) {
		file.write(content.c_str(), content.size());
		file.write("\n", 1);
	}
	*/
}

static void glfw_error_callback(int error, const char* description)
{
	fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

// Tries to find a translation file for the current system locale, return false on failure
static bool LoadLocaleTranslation()
{
	char name[LOCALE_NAME_MAX_LENGTH];
	{
		wchar_t w_name[LOCALE_NAME_MAX_LENGTH];
		if (LCIDToLocaleName(GetThreadLocale(), w_name, LOCALE_NAME_MAX_LENGTH, 0) == 0) {
			return false;
		}

		size_t retval = 0;
		if (wcstombs_s(&retval, name, LOCALE_NAME_MAX_LENGTH, w_name, LOCALE_NAME_MAX_LENGTH - 1) != 0) {
			return false;
		}
	}

	WriteToLogFile(std::format("Attempting to load locale {}", name));
	return Localization::LoadFile(name);
}

// Initialize the important members of mainwindow. I prefer doing it here because it is mostly a layout and GUI-related class
static void InitMainClasses(MainWindow& program)
{
	program.storage.ReloadMovesetList();

	GameAddressesFile* addrFile = new GameAddressesFile();
	program.addrFile = addrFile;

	program.extractor.Init(addrFile, &program.storage);
	program.importer.Init(addrFile, &program.storage);
	program.sharedMem.Init(addrFile, &program.storage);

	program.onlineMenu.gameHelper = &program.sharedMem;
	program.persistentPlayMenu.gameHelper = &program.sharedMem;

	{
		// Detect running games and latch on to them if possible

		bool attachedExtractor = false;
		bool attachedImporter = false;
		bool attachedOnline = false;

		auto processList = GameProcessUtils::GetRunningProcessList();

		// Loop through every game we support
		for (int gameIdx = 0; gameIdx < Games::GetGamesCount(); ++gameIdx)
		{
			auto gameInfo = Games::GetGameInfoFromIndex(gameIdx);
			const char* processName = gameInfo->processName;

			// Detect if the game is running
			{
				bool isRunning = false;
				for (auto& process : processList)
				{
					if (process.name == processName)
					{
						isRunning = true;
						break;
					}
				}
				if (!isRunning) {
					continue;
				}
			}

			if (!attachedExtractor && gameInfo->extractor != nullptr) {
				program.extractor.SetTargetProcess(gameInfo);
				attachedExtractor = true;
				DEBUG_LOG("Extraction-compatible game '%s' already running: attaching.\n", processName);
			}

			if (!attachedImporter && gameInfo->extractor != nullptr) {
				program.importer.SetTargetProcess(gameInfo);
				attachedImporter = true;
				DEBUG_LOG("Importation-compatible game '%s' already running: attaching.\n", processName);
			}

			if (!attachedOnline && gameInfo->onlineHandler != nullptr) {
				program.sharedMem.SetTargetProcess(gameInfo);
				attachedOnline = true;
				DEBUG_LOG("Online-compatible game '%s' already running: attaching.\n", processName);
			}

		}
	}

	program.storage.StartThread();
	program.extractor.StartThread();
	program.importer.StartThread();
	program.sharedMem.StartThread();
}

// Free up memory and stop threads cleanly before exiting the program
static void DestroyMainClasses(MainWindow& program)
{
	program.extractor.StopThreadAndCleanup();
	program.importer.StopThreadAndCleanup();
	program.sharedMem.StopThreadAndCleanup();

	for (EditorWindow* win : program.editorWindows) {
		delete win;
	}

	// Now that every thread that uses addrFile has stopped, we can free it
	delete program.addrFile;

	// Once every thread that may use storage has been stopped, we can finally stop storage
	program.storage.StopThreadAndCleanup();
}

// -- main -- //

#ifdef BUILD_TYPE_DEBUG
int main(int argc, wchar_t** argv, char** env)
#else
int WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd)
#endif
{
	{
		std::wstring oldWorkingDir = std::filesystem::current_path().wstring();

		// Make sure working dir is same as .exe
		wchar_t currPath[MAX_PATH] = { 0 };
		GetModuleFileNameW(nullptr, currPath, MAX_PATH);
		std::wstring ws(currPath);
		ws.erase(ws.find_last_of(L"\\"));

		if (ws != std::filesystem::current_path()) {
			std::filesystem::current_path(ws);
			WriteToLogFile(std::format("OLD CWD is {}", Helpers::wstring_to_string(oldWorkingDir)));
			WriteToLogFile(std::format("Set CWD to {}", Helpers::wstring_to_string(ws)));
		}

		WriteToLogFile("Started TKMovesets " PROGRAM_VERSION, false);
	}

	// Initialize GLFW library
	glfwSetErrorCallback(glfw_error_callback);
	if (!glfwInit()) {
		return 1;
	}
	WriteToLogFile("Initiated GLFW");

	// GL 3.0 + GLSL 130
	// Set the window hint. Not really that important to be honest.
	const char* c_glsl_version = "#version 130";
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
	//glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
	//glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only

	// Setup window title and create window
	GLFWwindow* window;
	{
		std::string windowTitle = std::format("{} {}", PROGRAM_TITLE, PROGRAM_VERSION);
		window = glfwCreateWindow(PROGRAM_WIN_WIDTH, PROGRAM_WIN_HEIGHT, windowTitle.c_str(), nullptr, nullptr);
	}
	WriteToLogFile("GLFW window created");

	if (window == nullptr) {
		return 2;
	}

	// Set window for current thread
	glfwMakeContextCurrent(window);
	// Enable vsync
	glfwSwapInterval(1);

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		DEBUG_LOG("Unable to context to OpenGL");
		return 3;
	}
	WriteToLogFile("GLL Loader done");

	{
		// Set viewport for OpenGL
		int screen_width, screen_height;
		glfwGetFramebufferSize(window, &screen_width, &screen_height);
		glViewport(0, 0, screen_width, screen_height);
	}

	// Load translation
	if (!LoadLocaleTranslation()) {
		Localization::LoadFile(PROGRAM_DEFAULT_LOCALE);
	}
	WriteToLogFile(std::format("Locale loaded - {}", Localization::GetCurrLangId()));

	{
		// Init main program. This will get most things going and create the important threads
		MainWindow program(window, c_glsl_version);
		ImGuiIO& io = ImGui::GetIO();
		InitMainClasses(program);

		WriteToLogFile("Initiated main classes");
		while (!glfwWindowShouldClose(window))
		{
			// Poll and handle events such as MKB inputs, window resize. Required
			glfwPollEvents();

			// Probably framebuffer related? Required
			program.NewFrame();

			// Main layout function : every we display is in there
			program.Update();

			// Let imgui do its stuff
			ImGui::Render();
			{
				int display_w, display_h;
				glfwGetFramebufferSize(window, &display_w, &display_h);
				glViewport(0, 0, display_w, display_h);
			}
			glClearColor(0.0f, 0.0f, 0.0f, 1.00f);
			glClear(GL_COLOR_BUFFER_BIT);
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			//if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
			{
				GLFWwindow* backup_current_context = glfwGetCurrentContext();
				ImGui::UpdatePlatformWindows();
				ImGui::RenderPlatformWindowsDefault();
				glfwMakeContextCurrent(backup_current_context);
			}
			glfwSwapBuffers(window);
		}

		DestroyMainClasses(program);
		WriteToLogFile("Destroyed main classes");
		// Cleanup what needs to be cleaned up
		program.Shutdown();
		WriteToLogFile("Shut down");
	}

	glfwDestroyWindow(window);
	glfwTerminate();

	Localization::Clear();

#ifdef BUILD_TYPE_DEBUG
	_CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_DEBUG);
	_CrtDumpMemoryLeaks();
#endif

	return 0;
}
