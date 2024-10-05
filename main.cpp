#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <stdexcept>

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
using std::istream;
using std::ostream;
using std::ifstream;
using std::ofstream;
using std::istringstream;
using std::ostringstream;
using std::quoted;
using std::vector;

// global variables used in main.cpp
string programName;
string configFileName;
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
    class BracketedString {
    public:
        BracketedString(string &s) : str(s), const_str(s) {};
        BracketedString(const string &s) : const_str(s) {};

        friend istream & operator>>(istream &is, BracketedString bs) {
            if (&bs.str == &BracketedString::dummy) 
                throw std::invalid_argument("not lvalue");
            
            // if not start with open bracket, treat as usual input case
            char ch;
            is >> ch;
            if (ch != '[') { is.putback(ch); return is >> bs.str; }

            is >> std::noskipws; // disable skipping white space
            bs.str.clear(); // clear output string first

            // read characters until close bracket is found
            ostringstream oss;
            bool isEscaped = false;
            while (is >> ch) {
                if (isEscaped) { isEscaped = false; oss << ch; continue; }
                if (ch == '\\') { isEscaped = true; continue; }
                if (ch == ']') break;
                oss << ch;
            }
            bs.str = oss.str();

            return is >> std::skipws; // enable skipping white space and return
        }

        friend ostream & operator<<(ostream &os, BracketedString bs) {
            os << '[';
            for(const char& ch : bs.const_str) { 
                if (ch == ']') os << '\\'; 
                os << ch; 
            } 
            os << ']';
            return os;
        }

    private:
        static string dummy;
        string &str = dummy;
        const string &const_str;
    };

    string BracketedString::dummy;

    BracketedString bracketed(string & s) {
        return BracketedString(s);
    }

    BracketedString bracketed(const string & s) {
        return BracketedString(s);
    }

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

    // istream & operator>>(istream &is, string& s)

	double easeInOutQuart(double x)
	{
		return x < 0.5 ? 8 * x * x * x * x : 1 - pow(-2 * x + 2, 4) / 2;
	}

    void printUsage()
    {
		cout << endl
			 << "Usage: easyConfig config_file [-t title]" << endl
			 << endl
			 << "-t:\ttitle of the config window." << endl
			 << "-h,--help\tshow this help message." << endl
             << endl
			 << endl
			 << "UI control: L1/R1: Select group, Up/Down: Select item, Left/Right: change value, B: Exit" << endl
             << endl
             << "The config file should contains lines of config settings, in the following format:" << endl
             << "\"NAME\" \"DESCRIPTION\" \"POSSIBLE_VALUES\" \"DISPLAY_VALUES\" \"CURRENT_VALUE\" [\"COMMANDS\"]" << endl
             << endl
             << "NAME: short name used in options file." << endl
             << "DESCRIPTION: description shown in config window." << endl
             << "POSSIBLE_VALUES: possible values of setting." << endl
             << "DISPLAY_VALUES: corresponding display texts of possible values shown in config window." << endl
             << "CURRENT_VALUES: current value of setting, should be one of the display values." << endl
             << "COMMANDS: optional commands to be executed on exit if the setting value is changed." << endl
             << endl
             << "POSSIBLE_VALUES, DISPLAY_VALUES and COMMANDS are values seperated by '|'. "
             << "Example lines of config file:" << endl
             << endl
             << "\"-s\" \"Text scrolling speed\" \"10|20|30\" \"Slow|Normal|Fast\" \"Fast\"" << endl
             << "\"-t\" \"Display title at start\" \"on|off\" \"on|off\" \"on\"" << endl
             << endl
             << "settings can be organized into groups, which displayed as multiple tags in config window. "
             << "To define a group use insert line with the following format:" << endl
             << endl
             << "[GROUP_NAME] [OPTIONAL_OUTPUT_FILENAME]"
             << endl
             << "Note that the square brackets are part of input. Any setting items after the group definition will be assigned to the group. "
             << "OPTIONAL_OUTPUT_FILENAME is the filename of optional output file. "
             << "Output option file is generated when program exit, which containing pairs of NAME and VALUE, can be utilized as option list for calling another program. "
             << "Example output options:" << endl
             << endl
             << "-s 10 -b off -t on -ts 4 -n on" << endl
			 << endl
             << "Config file is updated with new values when program exit." << endl;
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

		if (!file.is_open()) printErrorAndExit("cannot open file: ", filename);

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
            if (line.front() == '[') {
                istringstream iss(line);
                string groupName, outputFilename;

                // read group name
                if (! (iss >> bracketed(groupName))) 
                    printErrorAndExit("cannot process line: ", line);

                // try read output filename
                iss >> bracketed(outputFilename);

                // create group item
                settingGroups.push_back(new SettingGroup(groupName, outputFilename));

                continue;
            }

            // process line with string stream
            // try read line as setting item
            string id, description, options, displayValues, selectedValue;
            istringstream iss(line);
            if (! (iss >> quoted(id) 
                >> quoted(description) 
                >> quoted(options)
                >> quoted(displayValues)
                >> quoted(selectedValue)))
            {
                printErrorAndExit("cannot process line: ", line);
            }

            // try read commands
            string commands;
            iss >> quoted(commands);

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
        }

		// close file
		file.close();

        // remove default empty group
        if (settingGroups.front()->getSize() == 0)
        {
            settingGroups.erase(settingGroups.begin());
        }
    }

    void saveConfigFile(const string &filename) {
		// open file
		ofstream file;
		file.open(filename);

        // exit if file cannot open
        if (!file.is_open()) printErrorAndExit("cannot open file: ", filename);

        // write all settings to file
        for (auto &group : settingGroups)
        {
            file << bracketed(group->getName());
            if (!group->getOutputFilename().empty())
               file << ' ' << bracketed(group->getOutputFilename());
            file << endl;
            for (auto &item : group->getItems())
            {
                file << quoted(item->getID()) << ' '
                    << quoted(item->getDescription()) << ' '
                    << quoted(item->getOptionsString()) << ' '
                    << quoted(item->getDisplayValuesString_()) << ' '
                    << quoted(item->getSelectedValue());
                if (!item->getCommandsString().empty())
                    file << ' ' << quoted(item->getCommandsString());
                file << endl;
            }
        }

		// close file
		file.close();
    }

    void saveOptionsFile()
    {
        // write all settings to file
        for (auto &group : settingGroups)
        {
            // get filename, skip this group if empty
            auto filename = group->getOutputFilename();
            if (filename.empty()) continue;

            // open file
            ofstream file;
            file.open(filename);

            // skip this group if file cannot open
            if (!file.is_open()) { 
                cerr << "cannot open file: " << filename << endl;
                continue;
            }

            for (auto &item : group->getItems())
            {
                auto id = item->getID();
                auto opt = item->getOptions()[item->getSelectedIndex()];
                if (id.empty() && opt.empty()) continue;
                file << id << ' ' << opt;
                if (item != group->getItems().back()) file << ' ';
            }

            // close file
            file.close();
        }
    }

    void runCommands() {
        for (auto &group : settingGroups)
        {
            for (auto &item : group->getItems())
            {   
                auto commands = item->getCommands();
                auto index = item->getSelectedIndex();

                if (commands.size() == 0) continue;
                if (index == item->getOldSelectedIndex()) continue;

                system(commands[index].c_str());
            }
        }
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
        int rowHeight = fontSize * 2 + 2;

        // render title and instruction
        if (isShowTitle) titleTexture->render(0, 10);
        if (isShowInstruction) instructionTexture->render();

        // render current group name
        if (settingGroups.size() > 1) {
            if (!isShowTitle) {
                marginTop += 10;
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
        if (totalHeight + rowHeight * 2 > global::SCREEN_WIDTH) topItemIndex++;

        int index = 0;
        auto group = settingGroups[selectedGroupIndex];
        auto items = group->getItems();
        for (auto &item : items)
        {
            // skip items that are outside screen
            if (index < topItemIndex) { index++; continue; };
            if (offsetY + rowHeight * 2 > global::SCREEN_WIDTH) break;

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
                //x -= nextTexture->getWidth() + valueSpece;
                x -= toggleOnTexture->getWidth();
                if (item->getSelectedIndex() == 0)
                    toggleOnTexture->render(x, offsetY);
                else
                    toggleOffTexture->render(x, offsetY);
            }
            else
            {
                int x = offsetX + global::SCREEN_HEIGHT - marginRight;
                if (index == selectedItemIndex) {
                    x -= nextTexture->getWidth();
                    nextTexture->render(x, offsetY);
                    x -= valueSpece;
                }
                x -= item->getValueTexture()->getWidth();
                item->renderValue(x, offsetY);
                if (index == selectedItemIndex) {
                    x -= valueSpece + prevTexture->getWidth();
                    prevTexture->render(x, offsetY);
                }
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
            saveConfigFile(configFileName);
            saveOptionsFile();
            runCommands();
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