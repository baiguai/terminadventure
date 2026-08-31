# Adding a New Command — Step-by-Step Guide

Every command and key binding in terminadventure has two parts:

1. A **C++ operation** registered in `state->operations` — where the actual
   work happens.
2. A **config line** in `config/commands.conf` — which binds a key and/or a
   `:command` name to that operation.

`build.sh` copies `commands.conf` next to the app executable (`build/bin/`
on Linux, `build-windows/bin/` on Windows).

> Two guides here: the one below (a plain command) and
> [Adding a New Dialog](#adding-a-new-dialog--worked-example-a-todos-dialog)
> further down (a key binding that pops up a modal overlay). A dialog binding
> is a command whose operation only sets a `bool` flag.

---

## Step 1 — Register the operation in the `.cpp` file

Operations live in `state->operations`, a map of name → handler:

    using Operation = std::function<void(const std::string& args, int count)>;

Register yours in the constructor of the component that owns the data:

| Operation belongs to | Register it in                                 |
|----------------------|------------------------------------------------|
| tree (nodes)         | `src/treeview/treeview.cpp` — `TreeView::TreeView` |
| text (editor)        | `src/editor/editor.cpp` — `Editor::Editor`     |
| app-level (e.g. quit)| `src/main.cpp`                                 |

For example, adding a `sort_nodes` operation to the tree:

    state->operations["sort_nodes"] = [this](const std::string&, int)
    {
        std::sort(selected_->children.begin(), selected_->children.end(),
                  [](const auto& a, const auto& b) { return a.name < b.name; });
    };

Rules:

- The op name must match the `op` column of the config line **exactly**
  (case-sensitive). `LoadConfig` validates every row against this map and
  fails the **whole** config if an op is missing — so an unregistered op
  disables all bindings, not just its own row.
- Components register in their constructors, which run before `main` calls
  `LoadConfig`, so ordering is safe.
- If the op should only act in one mode, guard it:

      if (state->mode == Mode::TREE) { ... }

- `args` is `""` for `args = -`, the typed argument for `prompt`, or the
  literal string. `count` is the repeat prefix (`3 j`); always `1` when
  `repeat` is `no`.

## Step 2 — Add a line to `config/commands.conf`

One entry per line:

    mode  key  repeat  command  args  op  description

Blank lines and lines starting with `#` are ignored.

| Column       | Meaning                                                                                          |
|--------------|--------------------------------------------------------------------------------------------------|
| `mode`       | `TREE`, `NORMAL`, `INSERT`, `VISUAL` — binds `key` in that mode. `GLOBAL` — registers a command only (no key). |
| `key`        | The key that triggers the binding. `-` means no key.                                             |
| `repeat`     | `yes`/`no` — accepts a count prefix (`3 j`).                                                     |
| `command`    | Optional `:name` usable from the command line. `-` means no command-line access.                 |
| `args`       | `-`, `prompt`, or a literal string. See below.                                                   |
| `op`         | The operation name from Step 1. `-` means no dispatch.                                           |
| `description`| User-facing text shown in the `?` help dialog. Quote it when it contains spaces (`"Move up"`).    |

`key` values:

- A single character: `j`, `:`, `0`, `$`, `#`, ...
- A special token: `Esc`, `Return`, `Tab`, `Backspace`, `Space`, `ArrowUp`,
  `ArrowDown`, `ArrowLeft`, `ArrowRight`, `PageUp`, `PageDown`, `Home`, `End`.
- A quoted multi-key sequence: `"g g"` (keys separated by spaces).

Any character works as a key, including `#`. A line is only a comment when
`#` is its **first non-whitespace character** — there are no inline comments,
so a row like `TREE  #  no  ...` binds `#`.

`args` values:

- `-` — no arguments; the op receives `""`.
- `prompt` — pressing the key opens the command line with `:command `
  prefilled so the user can type the argument; `Enter` runs the op with it.
  Requires `command` to be set.
- `<text>` — a literal string passed verbatim to the op.

Examples — the same op bound four ways:

    # key-only
    TREE   s   no   -   -   sort_nodes

    # key + :command
    TREE   s   no   sort_nodes   -   sort_nodes

    # command-only (no key) — run as :sort_nodes from any mode
    GLOBAL   -   -   sort_nodes   -   sort_nodes

    # key that prompts for an argument before running
    TREE   a   no   create_node   prompt   new_node

For the last one: press `a` → the command line opens with `:create_node `
prefilled → type a name → `Enter` creates the node. The same op is also
reachable directly as `:create_node Name`. (With nothing selected, `a`
adds a new top-level node; `A` uses `:create_child` / `InsertChild` to add
a child of the selected node, or a top-level node when nothing is selected.)

## Step 3 — Rebuild

    ./build.sh

The config is copied next to the binary automatically — no extra step.

---

## How dispatch works (when you press a key or run `:command`)

- **Key press** → the active keymap returns the binding's `op` → `Dispatch`
  opens the prompt (if `args = prompt`) or calls `operations[op]`.
- **`:command ...`** → `ExecuteCommand` looks up the `commands` map
  (populated from the `command` column of every row) → calls `operations[op]`.

---

# Adding a New Dialog — Worked Example: a "Todos" dialog

A **dialog** is a modal overlay drawn on top of the app. The real ones are
`help` (`Esc`), `search` (`/`), `bookmarks` (`` ` ``), `links` (`#`),
`history` (`<`) and the file browser; they live in `src/help/`, `src/search/`,
`src/bookmarks/`, `src/links/`, `src/history/`, `src/browser/`.
The file browser is special: it is not bound to a key, it opens when a path
command (`open`/`saveas`/`import_html`/`export_html`) is given a **directory**
argument. `j`/`k` move, `h` goes up, `l`/`Enter` enter a directory, `Enter`
picks the selected file, `gg`/`G` jump to the first/last row, `/` opens a
case-insensitive filter field that narrows the list as you type (`Enter` keeps
the filtered list and hides the field, `Esc` in the field clears it), and `Esc`
clears an applied filter before cancelling the dialog. The picked file's path
is handed back to the invoking operation. For the save/export-style commands
(`saveas`/`export_html`) pressing `Enter` on a directory closes the browser and
reopens the command line prefilled with `:command <folder>/` so a filename can
be typed; `l` still navigates into the directory.
Below we build a small new one end to end: pressing `t` in TREE mode lists
every `TODO:` line in the active node's text; `j`/`k` move, `Enter` jumps to
the line, `Esc` cancels.

A dialog binding has **four parts**:

1. A `bool` flag in `src/main.cpp` + an operation that sets it.
2. A factory: `src/todos/todos.hpp` with `MakeTodosDialog(state, &flag)`.
3. The component: `src/todos/todos.cpp` — one class that renders the dialog
   and handles keys (and optionally the mouse).
4. Wiring: a `commands.conf` row, a `Modal(...)` wrapper in `main.cpp`, and a
   `config.sh` `SOURCES` entry.

## Step A — The flag and the operation (`src/main.cpp`)

The operation does nothing but flip a flag; the dialog is what reacts:

```cpp
bool show_todos = false;
state->operations["show_todos"] = [&show_todos](const std::string&, int)
{
    show_todos = true;
};
```

You can also make the dialog reachable as `:todos` by registering the command
in code (`state->commands["todos"] = "show_todos";`) or — preferred — by
giving the `commands.conf` row a `command` column (Step D), which does the
same thing.

## Step B — The header (`src/todos/todos.hpp`)

Every dialog header follows the same shape: forward-declare `EditorState`,
declare one factory function, and document the keys in a comment.

```cpp
#pragma once

#include <memory>

#include <ftxui/component/component.hpp>

struct EditorState;

namespace terminadventure::todos
{
    // Build the todos dialog. While *show is true it consumes every event, so
    // no app key bindings fire. j/k (and ArrowUp/ArrowDown) move the
    // selection, Enter jumps the editor cursor to the TODO line, Escape
    // cancels. Nodes without text show "No TODOs".
    ftxui::Component MakeTodosDialog(std::shared_ptr<EditorState> state, bool* show);
}
```

## Step C — The component (`src/todos/todos.cpp`)

A dialog is a class derived from `ftxui::ComponentBase`. It must do three
things:

- **Construct** with the state pointer and the `bool* show` flag.
- **`OnEvent`** — handle keys; **return `true` for every event**, even ones
  you ignore, so nothing below the modal (tree, editor, status bar) sees it.
- **`Render`** — rebuild the entry list from current state and draw it.

```cpp
#include "todos.hpp"

#include <string>
#include <vector>

#include <ftxui/component/event.hpp>
#include <ftxui/dom/elements.hpp>

#include "../editor/editor_state.hpp"
#include "../treeview/tree_node.hpp"

namespace terminadventure::todos
{
    class TodosDialog : public ftxui::ComponentBase
    {
    public:
        TodosDialog(std::shared_ptr<EditorState> state, bool* show)
            : state_(std::move(state)), show_(show) {}

        bool Focusable() const override { return true; }

        bool OnEvent(ftxui::Event event) override
        {
            if (event == ftxui::Event::Escape)
            {
                Close();
                return true;
            }
            if (event == ftxui::Event::Return)
            {
                Jump();
                return true;
            }
            if (event == ftxui::Event::ArrowDown
                || (event.is_character() && event.character() == "j"))
            {
                MoveSelection(+1);
                return true;
            }
            if (event == ftxui::Event::ArrowUp
                || (event.is_character() && event.character() == "k"))
            {
                MoveSelection(-1);
                return true;
            }
            return true;   // consume everything else
        }

        ftxui::Element Render() override
        {
            Recompute();

            ftxui::Elements rows;
            if (lines_.empty())
            {
                rows.push_back(ftxui::text("  No TODOs") | ftxui::dim);
            }
            else
            {
                for (int i = 0; i < static_cast<int>(lines_.size()); ++i)
                {
                    ftxui::Element row = ftxui::text(" " + lines_[i] + " ");
                    if (i == selection_) row = row | ftxui::inverted;
                    rows.push_back(row);
                }
            }

            return ftxui::window(ftxui::text(" t Todos "),
                                 ftxui::vbox({
                                     ftxui::separator(),
                                     ftxui::vbox(std::move(rows)),
                                     ftxui::separator(),
                                     ftxui::text("  j/k move  Enter jump  Esc cancel  ") | ftxui::dim,
                                 })) |
                   ftxui::size(ftxui::WIDTH, ftxui::LESS_THAN, 92) |
                   ftxui::size(ftxui::HEIGHT, ftxui::LESS_THAN, 24);
        }

    private:
        void Close() { *show_ = false; selection_ = 0; }

        void Jump()
        {
            if (selection_ < static_cast<int>(line_numbers_.size())
                && state_->reveal_line)
            {
                state_->reveal_line(line_numbers_[selection_]);
                state_->mode = Mode::NORMAL;
                if (state_->focus_editor) state_->focus_editor();
            }
            Close();
        }

        void MoveSelection(int dir)
        {
            if (lines_.empty()) return;
            selection_ = std::max(0, std::min(
                static_cast<int>(lines_.size()) - 1, selection_ + dir));
        }

        void Recompute()
        {
            lines_.clear();
            line_numbers_.clear();
            if (!state_->active_node) return;
            // Split active_node->text on '\n' and keep "TODO: ..." lines
            // with their 0-based line number. (Real dialogs reuse this
            // pattern: rebuild from live state on every render.)
        }

        std::shared_ptr<EditorState> state_;
        bool* show_;
        std::vector<std::string> lines_;
        std::vector<int> line_numbers_;
        int selection_ = 0;
    };

    ftxui::Component MakeTodosDialog(std::shared_ptr<EditorState> state, bool* show)
    {
        return ftxui::Make<TodosDialog>(std::move(state), show);
    }
}
```

## Step D — The config row (`config/commands.conf`)

```conf
TREE   t   no   todos   -   show_todos   ShowTodos()
```

Columns (see the table in Step 2): `t` is the key, `todos` is the `:command`
name, `-` means no arguments, `show_todos` must match the operation name from
Step A **exactly**, and `ShowTodos()` is documentation only. Because
`command` is `todos`, you can also open the dialog with `:todos`.

## Step E — Wire into the UI (`src/main.cpp` and `config.sh`)

Create the component next to the other dialogs and wrap the root one level
deeper in `Modal`:

```cpp
#include "todos/todos.hpp"
// ...
auto todos_comp = terminadventure::todos::MakeTodosDialog(state, &show_todos);
auto root = Modal(Modal(Modal(Modal(Modal(container, help_comp, &show_help),
                                     search_comp, &show_search),
                              bookmarks_comp, &show_bookmarks),
                        links_comp, &show_links),
                  todos_comp, &show_todos);
```

Each new dialog adds one more `Modal(...)` layer. The outermost `Modal` is
drawn **on top** if two flags are ever true at once.

Add the source to the build in `config.sh`:

```bash
SOURCES=(
    # ...
    "src/links/links.cpp"
    "src/todos/todos.cpp"    # <-- new
)
```

## Step F — Rebuild

    ./build.sh

---

## The dialog component, explained

**Consuming events.** While the flag is true the modal is the active child of
a `Tab` container, so every event — key or mouse — is delivered to `OnEvent`
first. Returning `true` from all paths is what makes the dialog modal:
otherwise a stray `a` would fall through and pop the `create_node` prompt.

**Getting data from the app.** `EditorState` (see `src/editor/editor_state.hpp`)
exposes everything a dialog needs, set up by the tree/editor before the
`Modal` stack is built:

| Field / callback        | What it gives you                                   |
|-------------------------|-----------------------------------------------------|
| `state_->active_node`   | The selected `TreeNode` (name, text, children).     |
| `state_->collect_all_nodes()` | Flat list of `(node*, depth)` over the whole tree. |
| `state_->reveal_node(node)`   | Select `node` in the tree, expanding ancestors.   |
| `state_->reveal_line(line)`   | Move the editor cursor to a 0-based line.         |
| `state_->focus_editor` / `focus_treeview` | Give keyboard focus to a pane.        |
| `state_->status`         | A message shown in the status bar (`"Copied: x"`).  |
| `state_->mode`           | Current `Mode`; set it to switch (e.g. `Mode::NORMAL`). |
| `state_->bookmarks`      | The bookmark list (see `bookmarks.cpp`).            |
| `state_->show_file_browser` / `browser_start_dir` / `browser_pick` / `browser_command` | Pointer to the Modal flag, the starting directory, and a `std::function<void(const std::string&)>` invoked with the picked file's path (see `browser.cpp`). `browser_command` is the invoking save/export command name (`"saveas"`/`"X"`), empty for open/import. |

**Jumping.** For a node jump call `reveal_node(node)` and leave the mode as
is. For a text jump call `reveal_line(line)` (0-based), switch to
`Mode::NORMAL`, and `focus_editor()`. See `Jump()` in `bookmarks.cpp` for the
combined pattern. Set `state_->status = ""` on success to clear stale
messages.

**Rebuilding every render.** `Recompute()` runs at the top of `Render()`,
rebuilding the entry list from live state each frame. That keeps the list
correct after the active node changes and costs almost nothing.

**Mouse support (optional).** FTXUI sends mouse events to the modal's active
child with **screen** coordinates — the `Tab` container does no translation —
so the dialog records its own box with `| ftxui::reflect(box_)` in `Render()`
and hit-tests against it. The links dialog (`src/links/links.cpp`) is the
reference: a left press inside the box maps `mouse.y - (box_.y_min + 2)` to a
list row (`+2` skips the window border and separator), two presses on the
same row within `kDoubleClickMs` activate it, and a press outside the box
dismisses the dialog.

**Sizing.** Dialogs use
`| size(WIDTH, LESS_THAN, 92) | size(HEIGHT, LESS_THAN, 24)` plus a `footer`
line that reports `N/M`, so they never cover the whole terminal.

## Wiring checklist

| # | Where                | What                                                              |
|---|----------------------|-------------------------------------------------------------------|
| 1 | `src/main.cpp`       | `bool show_x = false;` + `state->operations["x"]` setting it.     |
| 2 | `src/x/x.hpp`        | `MakeXDialog(state, bool* show)` factory.                         |
| 3 | `src/x/x.cpp`        | `ComponentBase` subclass: constructor, `OnEvent` (consume all), `Render` (window). |
| 4 | `config/commands.conf` | Row binding the key to op `x` (optionally a `command` column).   |
| 5 | `src/main.cpp`       | Create `x_comp` and nest one more `Modal(...)`.                   |
| 6 | `config.sh`          | Add `"src/x/x.cpp"` to `SOURCES`.                                 |
| 7 | —                    | `./build.sh` (copies `commands.conf` automatically).              |
