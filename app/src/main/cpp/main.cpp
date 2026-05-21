#include "PayloadBootstrap.h"

__attribute__((constructor)) static void payload_main() {
  menu::StartPayloadThread();
}
