#include "setting_item.h"

#include <cstdlib>
#include <string>
#include <algorithm>

#include "global.h"

namespace {
    // split string with delimiter into vector of tokens
    vector<std::string> split(const string& s, const string& delimiter) {
        string copy_s = s;
        vector<std::string> tokens;
        size_t pos = 0;
        string token;
        while ((pos = copy_s.find(delimiter)) != std::string::npos) {
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
        auto it = std::find(v.begin(), v.end(), s); 

        if (it != v.end()) 
            return it - v.begin(); 
        else 
            return -1;
    }
}

SettingItem::SettingItem(
        string id, 
        string description, 
        string optionsString,
        string displayValuesString,
        string selectedValue,
        string commandsString
        )
    :id_(id), description_(description), optionsString_(optionsString),
     displayValuesString_(displayValuesString), selectedValue_(selectedValue),
     commandsString_(commandsString)
{
    options_ = split(optionsString_, "|");
    displayValues_ = split(displayValuesString_, "|");
    if (!commandsString_.empty()) commands_ = split(commandsString_, "|");
    int index = find(displayValues_, selectedValue_);
    
    if (options_.size() < 2 || displayValues_.size() < 2 ||
        options_.size() != displayValues_.size() ||
        (commands_.size() > 0 && commands_.size() != options_.size())) {
        errorMessage_ = "invalid number of options or commands " + commands_.size();
        return;
    } else if (index < 0) {
        errorMessage_ = "invalid option value";
        return;
    }

    oldSelectedIndex_ = selectedIndex_ = static_cast<unsigned int>(index);

    descriptionTexture_ = new TextTexture(
        description_, 
        global::font,
        global::text_color
    );

    createValueTexture();

    isInitOK_ = true;
}

void SettingItem::createValueTexture() {
    valueTexture_ = new TextTexture(
        displayValues_[selectedIndex_], 
        global::font,
        global::text_color
    );
}

void SettingItem::renderDescription(int x, int y) const
{
    descriptionTexture_->render(x, y);
}

void SettingItem::renderValue(int x, int y) const
{
    valueTexture_->render(x, y);
}

void SettingItem::selectPreviousValue()
{
    if (selectedIndex_ == 0) selectedIndex_ = options_.size();
    selectedIndex_--;

    selectedValue_ = displayValues_[selectedIndex_];

    createValueTexture();
}

void SettingItem::selectNextValue()
{
    selectedIndex_++;
    if (selectedIndex_ >= options_.size()) selectedIndex_ = 0;

    selectedValue_ = displayValues_[selectedIndex_];

    createValueTexture();
}

bool SettingItem::isOnOffSetting() const 
{
    return displayValues_.size() == 2 &&
        displayValues_[0] == "on" &&
        displayValues_[1] == "off";
}

