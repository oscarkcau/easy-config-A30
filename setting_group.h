#ifndef SETTING_GROUP_H
#define SETTING_GROUP_H

#include <string>
#include <vector>

using std::string;
using std::vector;

#include "setting_item.h"

class SettingGroup {
public:
    SettingGroup(const string & name, const string & outputFilename = "") 
        : name_(name), outputFilename_(outputFilename) {}

    const string & getName() const { return name_; }
    const string & getOutputFilename() const { return outputFilename_; }
    vector<SettingItem*> & getItems() { return items_; }
    unsigned int getSize() const { return items_.size(); }

private:
    const string name_, outputFilename_;
    vector<SettingItem*> items_;
};

#endif // SETTING_GROUP_H