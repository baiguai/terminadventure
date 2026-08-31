#include "keymap.hpp"

#include <algorithm>

void Keymap::Bind(ftxui::Event event, Binding binding)
{
    single_bindings_[event.input()] = std::move(binding);
}

void Keymap::Bind(std::vector<ftxui::Event> sequence, Binding binding)
{
    std::vector<std::string> keys;
    keys.reserve(sequence.size());
    for (auto& e : sequence) keys.push_back(e.input());
    sequence_bindings_[keys] = std::move(binding);
}

Keymap::Result Keymap::Handle(ftxui::Event event)
{
    auto& input = event.input();

    if (enable_count_ && input.size() == 1 && input[0] >= '0' && input[0] <= '9')
    {
        int digit = input[0] - '0';
        if (digit >= 1 || count_ > 0)
        {
            count_ = count_ * 10 + digit;
            return {"", "", "", true};
        }
    }

    auto finish = [&](Result r) -> Result
    {
        int c = count_;
        count_ = 0;
        if (c > 0) r.count = c;
        return r;
    };

    if (!pending_.empty())
    {
        auto candidate = pending_;
        candidate.push_back(input);
        if (auto ib = sequence_bindings_.find(candidate); ib != sequence_bindings_.end())
        {
            pending_.clear();
            return finish({ib->second.op, ib->second.command, ib->second.args});
        }

        // Keep waiting if the candidate is a prefix of a longer sequence
        // (e.g. "d i" while "d i w" is bound), so single keys like `i` don't
        // fire in the middle of an operator sequence.
        for (auto& [seq, _] : sequence_bindings_)
        {
            if (candidate.size() < seq.size()
                && std::equal(candidate.begin(), candidate.end(), seq.begin()))
            {
                pending_ = candidate;
                return {"", "", "", true};
            }
        }

        pending_.clear();
    }

    if (auto ib = single_bindings_.find(input); ib != single_bindings_.end())
        return finish({ib->second.op, ib->second.command, ib->second.args});

    for (auto& [seq, _] : sequence_bindings_)
    {
        if (!seq.empty() && seq[0] == input)
        {
            pending_ = {input};
            return {"", "", "", true};
        }
    }

    count_ = 0;
    return {"", "", "", false};
}
