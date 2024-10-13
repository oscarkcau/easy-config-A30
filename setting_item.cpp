#include "setting_item.h"

#include <cstdlib>
#include <string>
#include <algorithm>

#include "global.h"

namespace {
    // split string with delimiter into vector of tokens
    vector<string> split(const string& s, const string& delimiter) {
        string copy_s = s;
        vector<string> tokens;
        size_t pos = 0;
        string token;
        while ((pos = copy_s.find(delimiter)) != string::npos) {
            token = copy_s.substr(0, pos);
            tokens.push_back(token);
            copy_s.erase(0, pos + delimiter.length());
        }
        tokens.push_back(copy_s);

        return tokens;
    }

    // search string in string vector and return zero-based index if found
    // return -1 if no element is find
    int find(vector<string> v, string s)
    {
        auto it = find(v.begin(), v.end(), s); 

        if (it != v.end()) 
            return it - v.begin(); 
        else 
            return -1;
    }

    // run command and get output from stdout 
    // return empty string if error occurs
    string exec(const string & cmd) {
        char buffer[1024];
        std::string result = "";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) return "";
        try {
            while (fgets(buffer, sizeof buffer, pipe) != NULL) {
                result += buffer;
            }
        } catch (...) {

        }
        pclose(pipe);
        return result;
    }
}

SettingItem::SettingItem(
        const string & id, 
        const string & description, 
        const string & optionsString,
        const string & displayValuesString,
        const string & selectedValue,
        const string & commandsString,
        const string & infoCommandString
        )
    :id_(id), description_(description), optionsString_(optionsString),
     displayValuesString_(displayValuesString), selectedValue_(selectedValue),
     commandsString_(commandsString), infoCommandString_(infoCommandString)
{
    options_ = split(optionsString_, "|");
    displayValues_ = split(displayValuesString_, "|");
    if (!commandsString_.empty()) commands_ = split(commandsString_, "|");
    
    if (options_.size() < 2 || displayValues_.size() < 2 ||
        options_.size() != displayValues_.size() ||
        (commands_.size() > 0 && commands_.size() != options_.size())) {
        errorMessage_ = "invalid number of options or commands " + commands_.size();
        return;
    } 

    // try to find index of the selected value 
    int index = find(displayValues_, selectedValue_);
    // if selected value not found in displayValues, assume it is command for source value 
    if (index < 0) {
        // try run command and get result as selected value
        sourceCommandString_ = selectedValue_;
        string cmd = global::replaceAliases(sourceCommandString_);
        selectedValue_ = exec(cmd);
        index = find(displayValues_, selectedValue_);

        if (selectedValue_.empty() || index < 0) {
            errorMessage_ = "invalid option value";
            return;
        }
    }

    // store selected inde to private fields
    oldSelectedIndex_ = selectedIndex_ = static_cast<unsigned int>(index);

    // create texture for description text
    descriptionTexture_ = new TextTexture(
        description_, 
        global::font,
        global::text_color
    );

    // create other textures
    updateTextures();

    isInitOK_ = true;
}

void SettingItem::setMinorText(const string & text) {
    minorText_ = text;

    if (minorTextTexture_ != nullptr) {
        delete minorTextTexture_;
        minorTextTexture_ = nullptr;
    }

    if (!minorText_.empty()) {
        minorTextTexture_ = new TextTexture(
            minorText_, 
            global::font,
            global::minor_text_color,
            TextureAlignment::topLeft,
            (global::SCREEN_HEIGHT - 120) * 2
        );
    }
}

void SettingItem::updateTextures() {

    // delete old value texture    
    if (valueTexture_ != nullptr) delete valueTexture_;

    // create new texture
    valueTexture_ = new TextTexture(
        displayValues_[selectedIndex_], 
        global::font,
        global::text_color
    );

    // try run command to get minor info message
    if (!infoCommandString_.empty()) {
        // delete old texture
        if (minorTextTexture_ != nullptr) {
            delete minorTextTexture_;
            minorTextTexture_ = nullptr;
        }

        // get new info text and create texture
        string cmd = global::replaceAliases(infoCommandString_);
        minorText_ = exec(cmd + " " + std::to_string(selectedIndex_));
        if (!minorText_.empty()) {
            minorTextTexture_ = new TextTexture(
                minorText_, 
                global::font,
                global::minor_text_color,
                TextureAlignment::topLeft,
                (global::SCREEN_HEIGHT - 120) * 2
            );
        }
    }
}

void SettingItem::renderDescription(int x, int y) const
{
    if (minorTextTexture_ != nullptr) {
        descriptionTexture_->render(x, y + 4);
        minorTextTexture_->render(x, y + descriptionTexture_->getHeight() - 4);
    } else {
        descriptionTexture_->render(x, y + 10);
    }
}

void SettingItem::renderValue(int x, int y) const
{
    if (minorTextTexture_ != nullptr) {
        int offsetY = (getHeight() - valueTexture_->getHeight()) / 2;
        valueTexture_->render(x, y + offsetY);
    } else {
        valueTexture_->render(x, y + 10);
    }
}

void SettingItem::selectPreviousValue()
{
    if (selectedIndex_ == 0) selectedIndex_ = options_.size();
    selectedIndex_--;

    selectedValue_ = displayValues_[selectedIndex_];

    updateTextures();
}

void SettingItem::selectNextValue()
{
    selectedIndex_++;
    if (selectedIndex_ >= options_.size()) selectedIndex_ = 0;

    selectedValue_ = displayValues_[selectedIndex_];

    updateTextures();
}

bool SettingItem::isOnOffSetting() const 
{
    return displayValues_.size() == 2 &&
        displayValues_[0] == "on" &&
        displayValues_[1] == "off";
}

