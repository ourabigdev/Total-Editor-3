/**
 * Copyright (c) 2022-present Alexander Lunsford
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 **/

#include "dialogs.hpp"

#include "imgui/imgui.h"

#include "../app.hpp"
#include "../map_man/map_man.hpp"

ExpandMapDialog::ExpandMapDialog()
    : _spinnerActive(false),
      _chooserActive(false),
      _amount(0),
      _direction(Direction::Z_NEG)
{
}

bool ExpandMapDialog::Draw()
{
    bool open = true;
    ImGui::OpenPopup("EXPAND GRID");
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("EXPAND GRID", &open, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Combo("Direction", (int*)(&_direction), "Back (+Z)\0Front (-Z)\0Right (+X)\0Left (-X)\0Top (+Y)\0Bottom (-Y)\0");

        ImGui::InputInt("# of grid cels", &_amount, 1, 10);
        
        if (ImGui::Button("EXPAND"))
        {
            App::Get()->ExpandMap(_direction, _amount);
            ImGui::EndPopup();
            return false;
        }

        ImGui::EndPopup();
        return true;
    }
    return open;
}