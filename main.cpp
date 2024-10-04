#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include "global.h"
#include "fileutils.h"
#include "setting_item.h"
#include "setting_group.h"
#include "image_texture.h"
#include "text_texture.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::ifstream;
using std::ofstream;
using std::istringstream;
using std::ostringstream;
using std::quoted;
using std::vector;

// global variables used in main.cpp
string programName;
string configFileName;
string optionsFileName;
int fontSize = 28;
vector<SettingGroup*> settingGroups = { new SettingGroup("Default") };
int topItemIndex = 0;
int selectedItemIndex = 0;
unsigned int selectedGroupIndex = 0;
SDL_Texture *messageBGTexture = nullptr;
SDL_Rect overlay_bg_render_rect;
string titleText = "";
string instructionText = "\u2191 / \u2193 Select item   \u2190/\u2192 Change value   \u24B7 Exit";;
string resourcePath = "res/";
TextTexture* titleTexture = nullptr;
TextTexture* instructionTexture = nullptr;
TextTexture* prevTexture = nullptr;
TextTexture* nextTexture = nullptr;
TextTexture* buttonLTexture = nullptr;
TextTexture* buttonRTexture = nullptr;
TextTexture* groupNameTexture = nullptr;
ImageTexture* toggleOnTexture = nullptr;
ImageTexture* toggleOffTexture = nullptr;
bool isShowTitle = false;

namespace {
	// trim from start (in place)
	inline void ltrim(std::string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
	}

	// trim from end (in place)
	inline void rtrim(std::string &s) {
		s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
			return !std::isspace(ch);
		}).base(), s.end());
	}

	double easeInOutQuart(double x)
	{
		return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2;
	}

    void printUsage()
    {
		cout << endl
			 << "Usage: easyConfig config_file [-t title] [-o options_file]" << endl
			 << endl
			 << "-t:\ttitle of the config window." << endl
			 << "-o:\toutput options file, contains single line of options generated from config file." << endl
			 << "-h,--help\tshow this help message." << endl
             << endl
			 << endl
			 << "UI control: Up/Down: Select item, Left/Right: change value, B: Exit" << endl
             << endl
             << "The config file should contains lines of config settings, in the following format:" << endl
             << "\"NAME\" \"DESCRIPTION\" \"POSSIBLE_VALUES\" \"DISPLAY_VALUES\" \"CURRENT_VALUE\" [\"COMMANDS\"]" << endl
             << endl
             << "NAME: short name used in options file." << endl
             << "DESCRIPTION: description shown in config window." << endl
             << "POSSIBLE_VALUES: possible values of setting." << endl
             << "DISPLAY_VALUES: corresponding display texts of possible values shown in config window." << endl
             << "CURRENT_VALUES: current value of setting, should be one of the display values." << endl
             << "COMMANDS: optional commands to be executed after the setting value is updated." << endl
             << endl
             << "POSSIBLE_VALUES, DISPLAY_VALUES and COMMANDS are values seperated by '|'. "
             << "Example lines of config file:" << endl
             << endl
             << "\"-s\" \"Text scrolling speed\" \"10|20|30\" \"Slow|Normal|Fast\" \"Fast\"" << endl
             << "\"-t\" \"Display title at start\" \"on|off\" \"on|off\" \"on\"" << endl
             << endl
             << "Config file is updated when program exit." << endl
             << endl
             << "Output option file is generated when program exit, which containing pairs of NAME and VALUE, can be utilized as option list for calling another program. "
             << "Example output option file:" << endl
             << endl
             << "-s 10 -b off -t on -ts 4 -n on" << endl
			 << endl;
    }

    void printErrorAndExit(string message, string extraMessage = "")
	{
		cerr << programName << ": " << message;
		if (!extraMessage.empty())
			cerr << extraMessage;
		cerr << endl
			 << endl;
		exit(0);
	}

	void printErrorUsageAndExit(string message, string extraMessage = "")
    {
		cerr << programName << ": " << message;
		if (!extraMessage.empty())
			cerr << extraMessage;
		cerr << endl;
		printUsage();
		exit(0);
    }

	void handleOptions(int argc, char *argv[])
	{
        // get program name
		programName = File_utils::getFileName(argv[0]);

		// ensuer enough number of arguments
		if (argc < 2)
			printErrorUsageAndExit("Arguments missing");

        // get config filename
        configFileName = argv[1];

        int i = 2;
		while (i < argc)
        {
			auto option = argv[i];
            if (strcmp(option, "-t") == 0)
            {
				if (i == argc - 1) printErrorUsageAndExit("-t: Missing option value");
                titleText = argv[i+1];
                if (titleText.empty()) printErrorUsageAndExit("-t: Title can't ne empty");
                isShowTitle = true;
                i += 2;
            }
            else if (strcmp(option, "-o") == 0)
            {
				if (i == argc - 1)
					printErrorUsageAndExit("-o: Missing option value");
                optionsFileName = argv[i+1];
                i += 2;
            }
			else if (strcmp(option, "-h") == 0 || strcmp(option, "--help") == 0)
			{
				printUsage();
				exit(0);
			}
			else
				printErrorUsageAndExit("Invalue option: ", option);
        }
    }

	void loadConfigFile(const char *filename)
    {
		// open file
		ifstream file;
		file.open(filename);

		if (file.is_open())
		{
			// iterate all input line
			string line;
			while (std::getline(file, line))
			{
				// trim input line
				rtrim(line);
				ltrim(line);

				// skip empty line
				if (line.empty()) continue;

                // skip line start with '#'
                if (line.front() == '#') continue;

                // try read line as setting group
                if (line.front() == '[' && line.back() == ']')
                {
                    // get group name
                    string groupName = line.substr(1, line.size()-2);
                    // add new group
                    settingGroups.push_back(new SettingGroup(groupName));
                    continue;
                }

                // process line with string stream
                // try read line as setting item
                string id, description, options, displayValues, selectedValue, commands;
                istringstream iss(line);
                if (! (iss >> quoted(id) 
                    >> quoted(description) 
                    >> quoted(options)
                    >> quoted(displayValues)
                    >> quoted(selectedValue)))
                {
                    printErrorAndExit("cannot process line: ", line);
                }

                // try read commands, set comands to empty string if failed
                if (! (iss >> quoted(commands)))
                {
                    commands = "";
                }

                // create setting item
                auto item = new SettingItem(
                    id, 
                    description, 
                    options,
                    displayValues, 
                    selectedValue,
                    commands
                );

                if (item->IsInitOK() == false) {
                    printErrorAndExit(item->getErrorMessage() + ": ", line);
                }

                // add item to recent created group
                settingGroups.back()->getItems().push_back(item);
                
                cout << line << endl;
            }
        }
        else
		{
			printErrorAndExit("cannot open file: ", filename);
		}

		// close file
		file.close();

        // remove default empty group
        if (settingGroups.front()->getSize() == 0)
        {
            settingGroups.erase(settingGroups.begin());
        }
        /*
        settingGroups.erase(
            std::remove_if(
                settingGroups.begin(),
                settingGroups.end(),
                [](const SettingGroup & g) { return g.getSize(); }
                )
            );
        */
    }

    void saveConfigFile(const char *filename) {
		// open file
		ofstream file;
		file.open(filename);

        // exit if file cannot open
        if (!file.is_open()) printErrorAndExit("cannot open file: ", filename);

        // write all settings to file
        for (auto &group : settingGroups)
        {
            file << '[' << group->getName() << ']' << endl;
            for (auto &item : group->getItems())
            {
                file << quoted(item->getID()) << " "
                    << quoted(item->getDescription()) << " "
                    << quoted(item->getOptionsString()) << " "
                    << quoted(item->getDisplayValuesString_()) << " "
                    << quoted(item->getSelectedValue()) << " "
                    << quoted(item->getCommandsString()) << endl;

            }
        }

		// close file
		file.close();
    }

    void saveOptionsFile(const char *filename)
    {
		// open file
		ofstream file;
		file.open(filename);

        // exit if file cannot open
        if (!file.is_open()) printErrorAndExit("cannot open file: ", filename);

        // write all settings to file
        for (auto &group : settingGroups)
        {
            for (auto &item : group->getItems())
            {
                auto options = item->getOptions();
                file << item->getID() << ' ' << options[item->getSelectedIndex()];
                //if (item != group.getItems().back()) 
                    file << ' ';
            }
        }

		// close file
		file.close();
    }

    void updateGroupNameTexture()
    {
        ostringstream oss;
        for (auto &group : settingGroups)
        {
            if (group == settingGroups[selectedGroupIndex]) {
                oss << group->getName();
            }
            else {
                oss << " \u2022 ";
            }
        }

        groupNameTexture = new TextTexture(
            oss.str(),
            global::font,
            global::text_color,
            TextureAlignment::topCenter
        );
    }

	void prepareTextures()
	{
		// create message overlay background texture
		int overlay_height = fontSize * 2;
		SDL_Rect overlay_bg_rect = {0, 0, overlay_height, global::SCREEN_HEIGHT};
		overlay_bg_render_rect.x = 0;
		overlay_bg_render_rect.y = 0;
		overlay_bg_render_rect.w = overlay_height;
		overlay_bg_render_rect.h = global::SCREEN_HEIGHT;
		SDL_Surface *surfacebg = SDL_CreateRGBSurface(
			0,
			overlay_height,
			global::SCREEN_HEIGHT,
			32, 0, 0, 0, 0);
		SDL_FillRect(
			surfacebg,
			&overlay_bg_rect,
			SDL_MapRGB(surfacebg->format, 80, 80, 80));
		SDL_SetSurfaceBlendMode(surfacebg, SDL_BLENDMODE_BLEND);
		messageBGTexture = SDL_CreateTextureFromSurface(
			global::renderer,
			surfacebg);
		SDL_FreeSurface(surfacebg);

        // create texture for on/off toggle button
        toggleOnTexture = new ImageTexture(resourcePath + "toggle-on.png");
        toggleOffTexture = new ImageTexture(resourcePath + "toggle-off.png");

        // create title and instruction texture
        if (isShowTitle) {
            titleTexture = new TextTexture(
                titleText, 
                global::font,
                global::text_color,
                TextureAlignment::topCenter
            );
        }
        instructionTexture = new TextTexture(
            instructionText, 
            global::font,
            global::text_color,
            TextureAlignment::bottomCenter
        );
		SDL_SetTextureAlphaMod(instructionTexture->getTexture(), 200);

        // create left and right arrow textures
        prevTexture = new TextTexture(
            "<", 
            global::font,
            global::text_color,
            TextureAlignment::topLeft
        );

        nextTexture = new TextTexture(
            ">", 
            global::font,
            global::text_color,
            TextureAlignment::topLeft
        );

        // create L and R button textures
        buttonLTexture = new TextTexture(
            " \u24C1", 
            global::font,
            global::text_color,
            TextureAlignment::topLeft
        );
        buttonRTexture = new TextTexture(
            "\u24C7 ", 
            global::font,
            global::text_color,
            TextureAlignment::topRight
        );
    }

    void renderAllSettings(int offsetX=0, bool isShowHighlight=true, bool isShowInstruction=true)
    {
        int marginTop = 60;
        int marginLeft = 20;
        int marginRight = 20;
        int valueSpece = 50;
        int rowHeight = fontSize * 2;

        // render title and instruction
        if (isShowTitle) titleTexture->render(0, 10);
        if (isShowInstruction) instructionTexture->render();

        // render current group name
        if (settingGroups.size() > 1) {
            if (!isShowTitle) {
                groupNameTexture->render(0, 10);
                buttonLTexture->render(0, 10);
                buttonRTexture->render(0, 10);
            } else {
                marginTop += 60;
                groupNameTexture->render(0, 60);
                buttonLTexture->render(0, 60);
                buttonRTexture->render(0, 60);
            }            
        }

        int offsetY = marginTop;
        int totalHeight = marginTop + (selectedItemIndex - topItemIndex ) * rowHeight;

        // adjust top item to display
        if (totalHeight < marginTop) topItemIndex--;
        if (totalHeight + rowHeight > global::SCREEN_WIDTH) topItemIndex++;

        int index = 0;
        auto group = settingGroups[selectedGroupIndex];
        auto items = group->getItems();
        for (auto &item : items)
        {
            // skip items that are outside screen
            if (index < topItemIndex) { index++; continue; };
            if (offsetY + rowHeight > global::SCREEN_WIDTH) break;

            // render background if it is selected
            if (index == selectedItemIndex && isShowHighlight)
            {
                auto rect = overlay_bg_render_rect;
                rect.x += offsetY - fontSize / 4;
                SDL_RenderCopy(global::renderer, messageBGTexture, nullptr, &rect);
            }

            // render setting description
            item->renderDescription(offsetX + marginLeft, offsetY);

            // render setting value
            if (item->isOnOffSetting())
            {
                int x = offsetX + global::SCREEN_HEIGHT - marginRight;
                x -= nextTexture->getWidth() + valueSpece;
                x -= toggleOnTexture->getWidth();
                if (item->getSelectedIndex() == 0)
                    toggleOnTexture->render(x, offsetY);
                else
                    toggleOffTexture->render(x, offsetY);
            }
            else
            {
                int x = offsetX + global::SCREEN_HEIGHT - marginRight - nextTexture->getWidth();
                if (index == selectedItemIndex) nextTexture->render(x, offsetY);
                x -= valueSpece + item->getValueTexture()->getWidth();
                item->renderValue(x, offsetY);
                x -= valueSpece + prevTexture->getWidth();
                if (index == selectedItemIndex) prevTexture->render(x, offsetY);
            }

            // update offset and index
            offsetY += rowHeight;
            index++;
        }
    }

    void ScrollLeft() {
        double step = 1;
        while (step > 0) {
            // render setting items
            double easing = easeInOutQuart(step); 
            int offsetX = static_cast<int>(-global::SCREEN_HEIGHT * easing);
            renderAllSettings(offsetX, false);

            selectedGroupIndex++;
            renderAllSettings(offsetX + global::SCREEN_HEIGHT, false, false);
            selectedGroupIndex--;

            SDL_RenderPresent(global::renderer);
            SDL_Delay(30);

            step -= 1.0 / 15;
        }
    }

    void ScrollRight() {
        double step = 1;
        while (step > 0) {
            // render setting items
            double easing = easeInOutQuart(step); 
            int offsetX = static_cast<int>(global::SCREEN_HEIGHT * easing);
            renderAllSettings(offsetX, false);

            selectedGroupIndex--;
            renderAllSettings(offsetX - global::SCREEN_HEIGHT, false, false);
            selectedGroupIndex++;

            SDL_RenderPresent(global::renderer);
            SDL_Delay(30);

            step -= 1.0 / 15;
        }
    }

	void keyPress(const SDL_Event &event)
	{
		if (event.type != SDL_KEYDOWN)
			return;

        unsigned int index = static_cast<unsigned int>(selectedItemIndex); 

		const auto sym = event.key.keysym.sym;
		switch (sym)
		{
		// button A (Space key)
		case SDLK_SPACE:
			break;
		// button UP (Up arrow key)
		case SDLK_UP:
            if (index > 0) 
                selectedItemIndex--;
			break;
		// button DOWN (Down arrow key)
		case SDLK_DOWN:
            if (index < settingGroups[selectedGroupIndex]->getItems().size() - 1)
                selectedItemIndex++;
			break;
		// button LEFT (Left arrow key)
		case SDLK_LEFT:
            settingGroups[selectedGroupIndex]->getItems()[index]->selectPreviousValue();
			break;
		// button RIGHT (Right arrow key)
		case SDLK_RIGHT:
            settingGroups[selectedGroupIndex]->getItems()[index]->selectNextValue();
			break;
		// button Y (Left Alt key)
		case SDLK_LALT:
			break;
		// button L1 (Tab key)
		case SDLK_TAB:
            if (selectedGroupIndex > 0)
            { 
                selectedGroupIndex--;
                selectedItemIndex = 0;
                topItemIndex = 0;
                updateGroupNameTexture();
                ScrollLeft();
            }
            break;
		// button R1 (Backspace key)
		case SDLK_BACKSPACE:
            if (selectedGroupIndex < settingGroups.size() - 1)
            {
                selectedGroupIndex++;
                selectedItemIndex = 0;
                topItemIndex = 0;
                updateGroupNameTexture();
                ScrollRight();
            }
			break;
		}

		// button B (Left control key)
		if (event.key.keysym.mod == KMOD_LCTRL)
		{
            if (!configFileName.empty()) saveConfigFile(configFileName.c_str());
            if (!optionsFileName.empty()) saveOptionsFile(optionsFileName.c_str());
            exit(0);
		}
	}
}

int main(int argc, char *argv[])
{
    handleOptions(argc, argv);

	// Init SDL
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
	if (IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_TIF | IMG_INIT_WEBP) == 0)
	{
		printErrorAndExit("IMG_Init failed");
	}
	else
	{
		// Clear the errors for image libraries that did not initialize.
		SDL_ClearError();
	}

	// Init font
	if (TTF_Init() == -1)
		printErrorAndExit("TTF_Init failed: ", SDL_GetError());

	global::font = TTF_OpenFont((resourcePath + "./nunwen.ttf").c_str(), fontSize);
	if (global::font == nullptr)
		printErrorAndExit("Font loading failed: ", TTF_GetError());

	// Hide cursor before creating the output surface.
	SDL_ShowCursor(SDL_DISABLE);

	// Create window and renderer
	SDL_Window *window = SDL_CreateWindow("Main", 0, 0, global::SCREEN_WIDTH, global::SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	global::renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (global::renderer == nullptr)
		printErrorAndExit("Renderer creation failed");


	// load config file and create settingItem instances
	loadConfigFile(argv[1]);

    // prepare common textures
    prepareTextures();
    updateGroupNameTexture();

	// Execute main loop of the window
	while (true)
	{
		// handle input events
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			switch (event.type)
			{
			case SDL_KEYDOWN:
				keyPress(event);
				break;
			case SDL_QUIT:
				return 0;
				break;
			}
		}

        // render setting items
        renderAllSettings();
        SDL_RenderPresent(global::renderer);

		// delay for around 30 fps
		SDL_Delay(30);
	}

	SDL_DestroyRenderer(global::renderer);
	TTF_CloseFont(global::font);
	SDL_DestroyWindow(window);
	SDL_Quit();
	return 0;
}