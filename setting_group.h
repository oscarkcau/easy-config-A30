#ifndef SETTING_GROUP_H
#define SETTING_GROUP_H

#include <string>
#include <vector>

using std::string;
using std::vector;

#include "setting_item.h"

class SettingGroup {
public:
    SettingGroup(string name) :name_(name) {}

    string getName() const { return name_; }
    vector<SettingItem*> & getItems() { return items_; }
    unsigned int getSize() const { return items_.size(); }

private:
    string name_;
    vector<SettingItem*> items_;
};

#endif // SETTING_GROUP_H