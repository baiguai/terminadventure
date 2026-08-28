#include "treeview.hpp"

namespace terminadventure::treeview
{
    class TreeView : public ftxui::ComponentBase
    {
        public:
            TreeView(std::shared_ptr<AppState> state) : state_(std::move(state))
            {};

        private:
            std::shared_ptr<AppState> state_;
    };

    ftxui::Component MakeTreeView(std::shared_ptr<AppState> state)
    {
        return ftxui::Make<TreeView>(std::move(state));
    }
}
