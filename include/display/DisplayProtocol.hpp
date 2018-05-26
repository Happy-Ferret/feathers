#pragma once

#include <wayland-server.h>

namespace display
{
  class DisplayProtocol
  {
    class WlDisplay {
      struct wl_display *data;

    public:
      WlDisplay()
	: data(wl_display_create())
      {
      }
      
      ~WlDisplay()
      {
	//	wl_display_destroy_clients(data);
	wl_display_destroy(data);
      }
      
      WlDisplay(WlDisplay const &) = delete;
      WlDisplay(WlDisplay &&) = delete;
      WlDisplay& operator=(WlDisplay const &) = delete;
      WlDisplay& operator=(WlDisplay &&) = delete;

      operator struct wl_display *() const
      {
	return data;
      }
    };

    WlDisplay const wlDisplay;

  public:
    DisplayProtocol(char const *socketName);

    DisplayProtocol() = delete;
    DisplayProtocol(DisplayProtocol const &) = delete;
    DisplayProtocol(DisplayProtocol &&) = delete;
    DisplayProtocol& operator=(DisplayProtocol const &) = delete;
    DisplayProtocol& operator=(DisplayProtocol &&) = delete;
  };
}
