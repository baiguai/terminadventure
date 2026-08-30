#pragma once

#include <memory>
#include <string>
#include <vector>

struct EditorState;

namespace terminadventure::config
{
    bool LoadConfig(const std::string& path, std::shared_ptr<EditorState> state);

    // Read the app's init config. On success `last_file` holds the value of
    // the `last_file` setting (cleared when the setting is absent/empty).
    bool ReadInit(const std::string& path, std::string& last_file, std::vector<std::string>& recent_files);

    // Persist `last_file` (may be empty) to the app's init config.
    bool WriteInit(const std::string& path, const std::string& last_file, const std::vector<std::string>& recent_files);

    // persist the recent files order etc
    bool WriteRecentFiles(const std::string& path, const std::vector<std::string>& recent_files);
}
