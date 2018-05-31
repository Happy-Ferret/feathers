#include "display/DisplayProtocol.hpp"

namespace display {

  DisplayProtocol::DisplayProtocol(char const *socketName)
  {
    wl_display_add_socket(wlDisplay, socketName);
  }
  
}
