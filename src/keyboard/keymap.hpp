#pragma once

#include <ftxui/component/event.hpp>
#include <map>
#include <utility>
#include <vector>
#include <string>

struct Binding
{
    std::string op;
    std::string command;
    std::string args;
    bool can_repeat = false;
};

class Keymap
{
    public:
        void Bind(ftxui::Event event, Binding binding);
        void Bind(std::vector<ftxui::Event> sequence, Binding binding);

        struct Result
        {
            std::string op;
            std::string command;
            std::string args;
            bool pending = false;
            int count = 1;
        };

        Result Handle(ftxui::Event event);
        void ResetPending() { pending_.clear(); }
        void EnableCounts() { enable_count_ = true; }

    private:
        std::map<std::string, Binding> single_bindings_;
        std::map<std::vector<std::string>, Binding> sequence_bindings_;
        std::vector<std::string> pending_;
        bool enable_count_ = false;
        int count_ = 0;
};
