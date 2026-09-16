#include "PayloadBootstrap.h"
#include "common/Log.h"
#include "shadowhook.h"

namespace {

void InitializeShadowHook() {
  const int result = shadowhook_init(SHADOWHOOK_MODE_SHARED, true);
  if (result != SHADOWHOOK_ERRNO_OK) {
    const int init_errno = shadowhook_get_init_errno();
    const char *message = shadowhook_to_errmsg(init_errno);
    menu::LogError("ShadowHook init failed: result=%d errno=%d (%s)", result,
                   init_errno, message != nullptr ? message : "unknown");
    return;
  }

  menu::LogInfo("ShadowHook initialized: version=%s mode=%d",
                shadowhook_get_version(),
                static_cast<int>(shadowhook_get_mode()));
}

}  // namespace

__attribute__((constructor)) static void payload_main() {
  InitializeShadowHook();
  menu::StartPayloadThread();
}
