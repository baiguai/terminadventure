#include "rightpane.hpp"

#include <utility>

#include <ftxui/dom/elements.hpp>

#include "../diceroller/diceroller.hpp"
#include "../editor/editor.hpp"
#include "../treeview/node_type.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::rightpane
{
    namespace
    {
        bool IsDiceRollerNode(const std::shared_ptr<EditorState>& state)
        {
            return state->active_node != nullptr
                && state->active_node->type == terminadventure::treeview::NodeType::ROLLER;
        }

        class RightPane : public ftxui::ComponentBase
        {
        public:
            RightPane(std::shared_ptr<EditorState> state,
                      ftxui::Component editor, ftxui::Component roller)
                : state_(std::move(state))
                , editor_(std::move(editor))
                , roller_(std::move(roller))
            {
            }

            bool Focusable() const override { return true; }

            ftxui::Component Active() const
            {
                return IsDiceRollerNode(state_) ? roller_ : editor_;
            }

            bool OnEvent(ftxui::Event event) override
            {
                return Active()->OnEvent(event);
            }

            ftxui::Element Render() override
            {
                return Active()->Render();
            }

        private:
            std::shared_ptr<EditorState> state_;
            ftxui::Component editor_;
            ftxui::Component roller_;
        };
    }

    ftxui::Component MakeRightPane(std::shared_ptr<EditorState> state)
    {
        auto editor = terminadventure::editor::MakeEditor(state);
        auto roller = terminadventure::diceroller::MakeDiceRoller(state);
        return ftxui::Make<RightPane>(std::move(state), std::move(editor), std::move(roller));
    }
}
