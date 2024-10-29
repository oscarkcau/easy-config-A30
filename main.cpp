#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <stdexcept>
#include <map>

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
using std::cout, std::cerr, std::endl;
using std::istream, std::ostream, std::getline;
using std::ifstream, std::ofstream;
using std::istringstream, std::ostringstream;
using std::quoted;
using std::vector;

// global variables used in main.cpp
string programName;
string configFileName;
int fontSize = 28;
vector<SettingGroup*> settingGroups = { new SettingGroup("Default") };
unsigned int selectedGroupIndex = 0;
SDL_Texture *messageBGTexture = nullptr;
SDL_Rect overlay_bg_render_rect;
string titleText = "";
string instructionText = "\u24B7 Cancel & Exit    \u24B6 Save & Exit";
string resourcePath = "res/";
TextTexture* titleTexture = nullptr;
TextTexture* instructionTexture = nullptr;
TextTexture* applyingSettingsTexture = nullptr;
TextTexture* prevTexture = nullptr;
TextTexture* nextTexture = nullptr;
TextTexture* buttonLTexture = nullptr;
TextTexture* buttonRTexture = nullptr;
TextTexture* groupNameTexture = nullptr;
TextTexture* itemIndexTexture = nullptr;
ImageTexture* toggleOnTexture = nullptr;
ImageTexture* toggleOffTexture = nullptr;
ImageTexture* runOnTexture = nullptr;
ImageTexture* runOffTexture = nullptr;
bool isShowTitle = false;
bool isShowSinglePage = false;

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
	inline void ltrim(string &s) {
		s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
			return !std::isspace(ch);
		}));
	}

	// trim from end (in place)
	inline void rtrim(string &s) {
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
			 << "Usage: easyConfig config_file [-t title] [-p index] [-o]" << endl
			 << endl
			 << "-t:\ttitle of the config window." << endl
			 << "-p:\tshow i-th group only (first group index = 1)." << endl
			 << "-o:\tgenerate options only" << endl
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
             << "[GROUP_NAME] [OPTIONAL_OUTPUT_FILENAME]" << endl
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

	void loadConfigFile(const char *filename)
    {
		// open file
		ifstream file;
		file.open(filename);

		if (!file.is_open()) printErrorAndExit("cannot open file: ", filename);

        // iterate all input line
        string line;
        SettingItem * lastItem = nullptr;
        while (getline(file, line))
        {
            // trim input line
            rtrim(line);
            ltrim(line);

            // skip empty line
            if (line.empty()) continue;

            // skip line start with '#'
            if (line.front() == '#') continue;

            // handle alias
            if (line.front() == '$') {
                istringstream iss(line);
                string pair;
                if (! (iss >> quoted(pair, '$'))) 
                    printErrorAndExit("cannot process line1: ", line);

                auto pos = pair.find("=");
                if (pos == string::npos || pos == 0 || pos >= pair.length() - 1)
                    printErrorAndExit("cannot process line2: ", line);

                string name = '$' + pair.substr(0, pos) + '$';
                string value = pair.substr(pos + 1);
                global::aliases[name] = value;
                cout << name << ' ' << value << endl;
                continue;
            }

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

            // handle minor text
            if (line.front() == '@') {
                // skip first character
                string s = line;
                s.erase(s.begin());

                istringstream iss(s);
                string minorText;
                iss >> quoted(minorText);

                if (minorText.empty() || lastItem == nullptr)
                    printErrorAndExit("cannot process line: ", line);

                lastItem->setMinorText(minorText);

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
            string commands, infoCommand;
            iss >> quoted(commands) >> quoted(infoCommand);

            // create setting item
            auto item = new SettingItem(
                id, 
                description, 
                options,
                displayValues, 
                selectedValue,
                commands,
                infoCommand
            );

            if (item->IsInitOK() == false) {
                printErrorAndExit(item->getErrorMessage() + ": ", line);
            }

            // add item to recent created group
            settingGroups.back()->getItems().push_back(item);

            // store last item
            lastItem = item;
        }

		// close file
		file.close();

        // remove default empty group
        if (settingGroups.front()->getSize() == 0)
        {
            settingGroups.erase(settingGroups.begin());
        }

        // adjust selectedGroupIndex
        if (selectedGroupIndex >= settingGroups.size()) 
        { 
            selectedGroupIndex = settingGroups.size() - 1;
        }
    }

    void saveConfigFile(const string &filename) {
		// open file
		ofstream file;
		file.open(filename);

        // exit if file cannot open
        if (!file.is_open()) printErrorAndExit("cannot open file: ", filename);

        // write all alias to file
        for (const auto& [key, value] : global::aliases)
        {
            string key_ = key;
            key_ = key.substr(1, key.length() - 2);

            file << quoted(key_ + '=' + value, '$') << endl;
        }

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
                    << quoted(item->getDisplayValuesString_()) << ' ';
                    
                if (!item->getSourceCommandString().empty())
                    file << quoted(item->getSourceCommandString());
                else
                    file << quoted(item->getSelectedValue());

                // if (!item->getCommandsString().empty())
                    file << ' ' << quoted(item->getCommandsString());

                // if (!item->getInfoCommandString().empty())
                    file << ' ' << quoted(item->getInfoCommandString());

                file << endl;

                if (!item->getMinorText().empty())
                    file << '@' << quoted(item->getMinorText()) << endl;
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
        // render setting items
        SDL_SetRenderDrawColor(global::renderer, 40, 40, 40, 255);
        SDL_RenderClear(global::renderer);
        applyingSettingsTexture->render();
        SDL_RenderPresent(global::renderer);

        for (auto &group : settingGroups)
        {
            for (auto &item : group->getItems())
            {   
                auto commands = item->getCommands();
                auto index = item->getSelectedIndex();

                // skip item if no command provided
                if (commands.size() == 0) continue;

                // skil item if item value does not changed
                if (index == item->getOldSelectedIndex()) continue;

                string cmd = commands[index];

                // replace aliases with corresponding value
                cmd = global::replaceAliases(cmd);

                // run the command
                system(cmd.c_str());
            }
        }
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
            else if (strcmp(option, "-p") == 0)
            {
				if (i == argc - 1) printErrorUsageAndExit("-p: Missing option value");
                int page = atoi(argv[i+1]);
                if (page <= 0) printErrorUsageAndExit("-p: Invalid page index");
                isShowSinglePage = true;
                selectedGroupIndex = static_cast<unsigned int>(page - 1);
                i += 2;
            }
            else if (strcmp(option, "-o") == 0)
            {
                loadConfigFile(argv[1]);
                saveOptionsFile();
                exit(0);
            }
			else
				printErrorUsageAndExit("Invalue option: ", option);
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
            else if (!isShowSinglePage) {
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

    void updateItemIndexTexture() {
        ostringstream oss;
        auto group = settingGroups[selectedGroupIndex];
        oss << group->getSelectedIndex() + 1 << '/' << group->getSize();
        itemIndexTexture = new TextTexture(
            oss.str(),
            global::font,
            global::text_color,
            TextureAlignment::bottomRight
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
        runOnTexture = new ImageTexture(resourcePath + "run-on.png");
        runOffTexture = new ImageTexture(resourcePath + "run-off.png");

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
            global::minor_text_color,
            TextureAlignment::bottomLeft
        );
        applyingSettingsTexture = new TextTexture(
            "Applying settings...", 
            global::font,
            global::text_color,
            TextureAlignment::center
        );
        //instructionTexture->FitScreenSize(10, 0);

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

        // render title and instruction
        if (isShowTitle) titleTexture->render(0, 10);
        if (isShowInstruction) {
             instructionTexture->render(10, 0);
             itemIndexTexture->render(-10, 0);
        }

        // render current group name
        if (settingGroups.size() > 1) {
            if (!isShowTitle) {
                marginTop += 10;
                groupNameTexture->render(0, 10);
                if (!isShowSinglePage) buttonLTexture->render(0, 10);
                if (!isShowSinglePage) buttonRTexture->render(0, 10);
            } else {
                marginTop += 60;
                groupNameTexture->render(0, 60);
                if (!isShowSinglePage) buttonLTexture->render(0, 60);
                if (!isShowSinglePage) buttonRTexture->render(0, 60);
            }            
        }

        // get some display parameters
        auto group = settingGroups[selectedGroupIndex];
        unsigned int selectedItemIndex = group->getSelectedIndex();
        unsigned int topItemIndex = group->getDisplayTopIndex();
        if (topItemIndex > selectedItemIndex) topItemIndex = selectedItemIndex;

        // adjust top item to display
        int totalHeight = marginTop;
        for (unsigned int i=topItemIndex; i<=selectedItemIndex; i++)
            totalHeight += group->getItems()[i]->getHeight();

        if (totalHeight > global::SCREEN_WIDTH - instructionTexture->getHeight()) topItemIndex++;
        group->setDisplayTopIndex(topItemIndex);

        // iterate and render all items within the screen
        unsigned int index = 0;
        int offsetY = marginTop;
        auto items = group->getItems();
        for (auto &item : items)
        {
            // skip items that are outside screen
            if (index < topItemIndex) { index++; continue; };
            if (offsetY + item->getHeight() > global::SCREEN_WIDTH - instructionTexture->getHeight()) break;

            // render background if it is selected
            if (index == selectedItemIndex && isShowHighlight)
            {
                auto rect = overlay_bg_render_rect;
                rect.x += offsetY;// - fontSize / 4;
                rect.w = item->getHeight();
                SDL_RenderCopy(global::renderer, messageBGTexture, nullptr, &rect);
            }

            // render setting description
            item->renderDescription(offsetX + marginLeft, offsetY);

            // render setting value
            if (item->isOnOffSetting())
            {
                int x = offsetX + global::SCREEN_HEIGHT - marginRight;
                x -= toggleOnTexture->getWidth();
                int y = offsetY + (item->getHeight() - toggleOnTexture->getHeight()) / 2;
                if (item->getSelectedIndex() == 0)
                    toggleOnTexture->render(x, y);
                else
                    toggleOffTexture->render(x, y);
            }
            else if (item->isRunOffSetting())
            {
                int x = offsetX + global::SCREEN_HEIGHT - marginRight;
                x -= runOnTexture->getWidth();
                int y = offsetY + (item->getHeight() - runOnTexture->getHeight()) / 2;
                if (item->getSelectedIndex() == 0)
                    runOnTexture->render(x, y);
                else
                    runOffTexture->render(x, y);
            }
            else
            {
                int x = offsetX + global::SCREEN_HEIGHT - marginRight;
                int y = offsetY + item->getValueOffsetY();
                if (index == selectedItemIndex) {
                    x -= nextTexture->getWidth();
                    nextTexture->render(x, y);
                    x -= valueSpece;
                }
                x -= item->getValueTexture()->getWidth();
                item->renderValue(x, offsetY);
                if (index == selectedItemIndex) {
                    x -= valueSpece + prevTexture->getWidth();
                    prevTexture->render(x, y);
                }
            }

            // update offset and index
            offsetY += item->getHeight();
            index++;
        }
    }

    void ScrollLeft() {
        double step = 1;
        while (step > 0) {
            SDL_RenderClear(global::renderer);

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
            SDL_RenderClear(global::renderer);

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

        auto group = settingGroups[selectedGroupIndex];
        unsigned int index = group->getSelectedIndex(); 

		const auto sym = event.key.keysym.sym;
		switch (sym)
		{
		// button A (Space key)
		case SDLK_SPACE:
            saveConfigFile(configFileName);
            saveOptionsFile();
            runCommands();
            exit(0);
        	break;
		// button UP (Up arrow key)
		case SDLK_UP:
            if (index > 0) {
                group->setSelectedIndex(index - 1);
                updateItemIndexTexture();
            }
            break;
		// button DOWN (Down arrow key)
		case SDLK_DOWN:
            if (index < group->getSize() - 1) {
                group->setSelectedIndex(index + 1);
                updateItemIndexTexture();
            }
            break;
		// button LEFT (Left arrow key)
		case SDLK_LEFT:
            group->getSelectedItem()->selectPreviousValue();
			break;
		// button RIGHT (Right arrow key)
		case SDLK_RIGHT:
            group->getSelectedItem()->selectNextValue();
			break;
		// button Y (Left Alt key)
		case SDLK_LALT:
			break;
		// button L1 (Tab key)
		case SDLK_TAB:
            if (selectedGroupIndex > 0 && !isShowSinglePage)
            { 
                selectedGroupIndex--;
                updateGroupNameTexture();
                updateItemIndexTexture();
                ScrollLeft();
            }
            break;
		// button R1 (Backspace key)
		case SDLK_BACKSPACE:
            if (selectedGroupIndex < settingGroups.size() - 1 && !isShowSinglePage)
            {
                selectedGroupIndex++;
                updateGroupNameTexture();
                updateItemIndexTexture();
                ScrollRight();
            }
			break;
		}

		// button B (Left control key)
		if (event.key.keysym.mod == KMOD_LCTRL)
		{
            exit(0);
		}
	}
}

int main(int argc, char *argv[])
{
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

	global::font = TTF_OpenFont((resourcePath + "./nunwen.ttf").c_str(), fontSize*2);
	if (global::font == nullptr)
		printErrorAndExit("Font loading failed: ", TTF_GetError());

	// Hide cursor before creating the output surface.
	SDL_ShowCursor(SDL_DISABLE);

	// Create window and renderer
	SDL_Window *window = SDL_CreateWindow("Main", 0, 0, global::SCREEN_WIDTH, global::SCREEN_HEIGHT, SDL_WINDOW_SHOWN);
	global::renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
	if (global::renderer == nullptr)
		printErrorAndExit("Renderer creation failed");

    // handle options 
    handleOptions(argc, argv);

	// load config file and create settingItem instances
	loadConfigFile(argv[1]);

    // prepare common textures
    prepareTextures();
    updateGroupNameTexture();
    updateItemIndexTexture();

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
        SDL_SetRenderDrawColor(global::renderer, 40, 40, 40, 255);
        SDL_RenderClear(global::renderer);
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