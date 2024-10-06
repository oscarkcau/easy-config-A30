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
        const string & id, 
        const string & description, 
        const string & optionsString,
        const string & displayValuesString,
        const string & selectedValue,
        const string & commandsString
        );

    void renderDescription(int offsetX, int offsetY) const;
    void renderValue(int offsetX, int offsetY) const;
    void selectPreviousValue();
    void selectNextValue();
    bool isOnOffSetting() const;

    const string & getID() const { return id_; }
    const string & getDescription() const { return description_; }
    const string & getOptionsString() const { return optionsString_; }
    const string & getDisplayValuesString_() const { return displayValuesString_; }
    const string & getSelectedValue() const { return selectedValue_; }
    const string & getCommandsString() const { return commandsString_; }
    const vector<string> & getOptions() const { return options_; }
    const vector<string> & getDisplayValues() const { return displayValues_; }
    const vector<string> & getCommands() const { return commands_; }
    unsigned int getSelectedIndex() const { return selectedIndex_; }
    unsigned int getOldSelectedIndex() const { return oldSelectedIndex_; }
    bool IsInitOK() const { return isInitOK_; }
    const string & getErrorMessage() const { return errorMessage_; }
    TextTexture* getDescriptionTexture() const { return descriptionTexture_; }
    TextTexture* getValueTexture() const { return valueTexture_; }
private:
    const string id_;
    const string description_;
    const string optionsString_;
    const string displayValuesString_;
    string selectedValue_;
    const string commandsString_;
    vector<string> options_;
    vector<string> displayValues_;
    vector<string> commands_;
    unsigned int selectedIndex_, oldSelectedIndex_;
    bool isInitOK_ = false;
    string errorMessage_;
    TextTexture* descriptionTexture_;
    TextTexture* valueTexture_;

    void createValueTexture();
};

#endif // SETTING_ITEM_H
