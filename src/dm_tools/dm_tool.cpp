#include "dm_tools.hpp"
#include <ftxui/dom/elements.hpp>

namespace terminadventure::dm_tools
{
    class DmTools : public ftxui::ComponentBase
    {
        public:
            DmTools(std::shared_ptr<AppState> state) : state_(std::move(state))

        private:
            std::shared_ptr<AppState> state_;
    };

    ftxui::Component MakeDmTools(std::shared_ptr<AppState> state)
    {
        return ftxui::Make<DmTools>(std::move(state));
    }
}
