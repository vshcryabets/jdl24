#pragma once

#include <string>
#include <ftxui/component/component.hpp>
#include <ftxui/component/component_base.hpp>
#include <ftxui/component/screen_interactive.hpp>
#include <ftxui/dom/elements.hpp>

class CurrentEditorBase : public ftxui::ComponentBase {
public:
    CurrentEditorBase(int* value);
    ftxui::Element OnRender() override;
    bool OnEvent(ftxui::Event event) override;
    bool Focusable() const override { return true; }

private:
    int* value_;
    bool is_editing_ = false;
    std::string input_buffer_;

    void ApplyInput();
};
