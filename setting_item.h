#ifndef SETTING_ITEM_H
#define SETTING_ITEM_H

#include <string>
#include <vector>

#include "text_texture.h"

using std::string;
using std::vector;

class SettingItem
{
public:
    explicit SettingItem(
        string id, 
        string description, 
        string optionsString,
        string displayValuesString,
        string selectedValue,
        string commandsString
        );

    void renderDescription(int offsetX, int offsetY) const;
    void renderValue(int offsetX, int offsetY) const;
    void selectPreviousValue();
    void selectNextValue();
    bool isOnOffSetting() const;

    string getID() const { return id_; }
    string getDescription() const { return description_; }
    string getOptionsString() const { return optionsString_; }
    string getDisplayValuesString_() const { return displayValuesString_; }
    string getSelectedValue() const { return selectedValue_; }
    string getCommandsString() const { return commandsString_; }
    vector<string> getOptions() const { return options_; }
    vector<string> getDisplayValues() const { return displayValues_; }
    vector<string> getCommands() const { return commands_; }
    void setCommands(vector<string> &c) { commands_ = c; }
    unsigned int getSelectedIndex() const { return selectedIndex_; }
    bool IsInitOK() const { return isInitOK_; }
    string getErrorMessage() const { return errorMessage_; }
    TextTexture* getDescriptionTexture() const { return descriptionTexture_; }
    TextTexture* getValueTexture() const { return valueTexture_; }
private:
    string id_, description_, optionsString_, displayValuesString_, selectedValue_, commandsString_;
    vector<string> options_;
    vector<string> displayValues_;
    vector<string> commands_;
    unsigned int selectedIndex_;
    bool isInitOK_ = false;
    string errorMessage_;
    TextTexture* descriptionTexture_;
    TextTexture* valueTexture_;

    void createValueTexture();
};

#endif // SETTING_ITEM_H
