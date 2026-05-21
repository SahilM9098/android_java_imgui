#include "MenuRenderer.h"

#include "imgui.h"

namespace menu {
int test1 = 0;
char test2[256];

void RenderMenuWindow() {
  ImGui::SetNextWindowSize(ImVec2(760.0f, 640.0f), ImGuiCond_Once);
  ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f), ImGuiCond_Once);

  ImGui::Begin("Tester");

  ImGui::InputInt("Test 1",&test1);

  ImGui::Spacing();
  ImGui::Spacing();

  ImGui::InputTextMultiline("Enter Text",test2,sizeof(test2));

  ImGui::End();
}

}  // namespace menu
