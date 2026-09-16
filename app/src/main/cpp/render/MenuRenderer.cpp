#include "MenuRenderer.h"

#include <cfloat>

#include "imgui.h"

namespace menu {
int test1 = 0;
char test2[256];
static int testSlider = 50;
static float testFloatSlider = 0.5f;
static int testCombo = 0;
static int selectedTestRow = -1;
static bool testChecks[32] = {};
static char testInputs[4][96] = {};

namespace {

constexpr const char *kComboItems[] = {
    "Default", "Performance", "Balanced", "Battery saver"};

}  // namespace

void RenderMenuWindow() {
  ImGui::SetNextWindowSize(ImVec2(760.0f, 640.0f), ImGuiCond_Once);
  ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f), ImGuiCond_Once);

  ImGui::Begin("Tester");

  ImGui::Text("Touch-scroll test menu");
  ImGui::TextWrapped(
      "Drag up or down inside the content area. This window intentionally "
      "contains more widgets than can fit on screen.");
  ImGui::Separator();

  ImGui::InputInt("Test 1", &test1);
  ImGui::SliderInt("Integer slider", &testSlider, 0, 100);
  ImGui::SliderFloat("Float slider", &testFloatSlider, 0.0f, 1.0f, "%.2f");
  ImGui::Combo("Profile", &testCombo, kComboItems,
               static_cast<int>(IM_ARRAYSIZE(kComboItems)));

  if (ImGui::Button("Test button")) {
    ++test1;
  }
  ImGui::SameLine();
  ImGui::Text("Button presses: %d", test1);

  ImGui::Spacing();
  ImGui::SeparatorText("Scrollable widget rows");

  for (int row = 0; row < 32; ++row) {
    ImGui::PushID(row);
    ImGui::Checkbox("Enabled", &testChecks[row]);
    ImGui::SameLine();
    if (ImGui::Selectable("Selectable row", selectedTestRow == row)) {
      selectedTestRow = row;
    }
    ImGui::SameLine();
    ImGui::TextDisabled("#%02d", row + 1);
    ImGui::PopID();
  }

  ImGui::Spacing();
  ImGui::SeparatorText("Text inputs");
  for (int field = 0; field < 4; ++field) {
    ImGui::PushID(field);
    ImGui::InputText("Field", testInputs[field], sizeof(testInputs[field]));
    ImGui::PopID();
  }

  ImGui::InputTextMultiline("Enter Text", test2, sizeof(test2),
                            ImVec2(-FLT_MIN, 120.0f));

  ImGui::End();
}

}  // namespace menu
