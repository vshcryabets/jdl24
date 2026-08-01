#include "CurrentEditorBase.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iomanip>

void CurrentEditorBase::ApplyInput() {
    if (input_buffer_.empty()) {
        is_editing_ = false;
        return;
    }
    try {
        float parsed = std::stof(input_buffer_);
        int new_val = static_cast<int>(std::round(parsed * 100.0f));
        *value_ = std::clamp(new_val, 1, 1000);
    } catch (...) {
        // ignore errors, just don't change the value
    }
    is_editing_ = false;
}

bool CurrentEditorBase::OnEvent(ftxui::Event event) {
    if (!Focused()) return false;

    if (is_editing_) {
        if (event == ftxui::Event::Return) {
            ApplyInput();
            return true;
        }
        if (event == ftxui::Event::Escape) {
            is_editing_ = false;
            return true;
        }
        if (event == ftxui::Event::Backspace) {
            if (!input_buffer_.empty()) {
                input_buffer_.pop_back();
            }
            return true;
        }
        if (event.is_character()) {
            char c = event.character()[0];
            // only allow digits and a single decimal point, and limit the length of the input
            if (isdigit(c) || c == '.') {
                if (c == '.' && input_buffer_.find('.') != std::string::npos) {
                    return true; // Ignore the second decimal point
                }
                if (input_buffer_.size() < 6) {
                    input_buffer_ += c;
                }
            }
            return true; // Block other keys while editing
        }
        return false;
        
    } else {
        // if (event == ftxui::Event::ArrowUp || event == ftxui::Event::Character('+')) {
        //     *value_ = std::min(1000, *value_ + 1);
        //     return true;
        // }
        // if (event == ftxui::Event::ArrowDown || event == ftxui::Event::Character('-')) {
        //     *value_ = std::max(1, *value_ - 1);
        //     return true;
        // }
        if (event == ftxui::Event::ArrowRight) {
            *value_ = std::min(1000, *value_ + 10);
            return true;
        }
        if (event == ftxui::Event::ArrowLeft) {
            *value_ = std::max(1, *value_ - 10);
            return true;
        }
        
        if (event == ftxui::Event::Return) {
            is_editing_ = true;
            input_buffer_ = ""; 
            return true;
        }
        
        if (event.is_character()) {
            char c = event.character()[0];
            if (isdigit(c) || c == '.') {
                is_editing_ = true;
                input_buffer_ = c;
                return true;
            }
        }
        }
    
    return false;
}

ftxui::Element CurrentEditorBase::OnRender() {
    bool is_focused = Focused();
    auto style = is_focused ? ftxui::inverted : ftxui::nothing;
    
    if (is_editing_) {
        // Editing mode: show what the user is typing and the cursor
        return ftxui::hbox({
            ftxui::text(" Current: "),
            ftxui::text(input_buffer_ + "_") | style | ftxui::bold,
        }) | ftxui::borderEmpty;
    } else {
        // Scrolling mode: show the formatted value
        std::stringstream ss;
        ss << std::fixed << std::setprecision(2) << (*value_ / 100.0f);
        
        return ftxui::hbox({
            ftxui::text(" Current: "),
            ftxui::text(ss.str()) | style | ftxui::bold,
        }) | ftxui::borderEmpty;
    }
}

CurrentEditorBase::CurrentEditorBase(int* value) : value_(value) {
    if (value_ == nullptr) {
        throw std::invalid_argument("value pointer cannot be null");
    }
}