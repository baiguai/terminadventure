#pragma once

#include <string>
#include <vector>

#include "../bookmark/bookmark.hpp"
#include "../players/dnd_data.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::io
{
    using treeview::TreeNode;

    std::string Serialize(const std::vector<TreeNode>& roots, int tree_width,
                          const std::vector<bookmark::Bookmark>& bookmarks,
                          const std::vector<std::string>& history,
                          const std::vector<std::string>& presets,
                          const std::vector<players::Player>& players);
    // On success `tree_width` is set only if the document carried one;
    // `bookmarks`/`history`/`presets`/`players` are replaced only if the
    // document carried any. Otherwise they are left untouched (callers should
    // pre-set defaults).
    bool Deserialize(const std::string& json, std::vector<TreeNode>& roots,
                     int* tree_width = nullptr,
                     std::vector<bookmark::Bookmark>* bookmarks = nullptr,
                     std::vector<std::string>* history = nullptr,
                     std::vector<std::string>* presets = nullptr,
                     std::vector<players::Player>* players = nullptr);

    std::string JsonEscape(const std::string& s);

    bool WriteFile(const std::string& path, const std::string& content);
    bool ReadFile(const std::string& path, std::string& content);

    // On-disk Terminadventure documents carry a magic header line before the
    // serialized JSON, so the app can recognize its own files and tell the
    // user when an opened file is something else.
    inline constexpr const char* kDocumentMagic = "# Terminadventure v1\n";

    enum class LoadStatus { Ok, NotFound, NotTerminadventure };

    // Writes a document file: the magic header followed by `json`.
    bool SaveDocumentFile(const std::string& path, const std::string& json);
    // Reads a document file. On Ok, `json` receives the payload after the
    // magic header; otherwise `json` is left untouched and the caller can tell
    // the user the file was not found or is not a Terminadventure file.
    LoadStatus LoadDocumentFile(const std::string& path, std::string& json);
}
