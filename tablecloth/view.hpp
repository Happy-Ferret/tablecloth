#pragma once

#include <cassert>

#include <optional>
#include <variant>

#include "util/ptr_vec.hpp"
#include "wlroots.hpp"

namespace cloth {

  struct Output;
  struct Desktop;
  struct View;
  struct Workspace;

  struct ViewChild {
    ViewChild(View& view, wlr::surface_t* wlr_surface);
    virtual ~ViewChild() noexcept;
    void finish();

    void handle_commit(void* data);
    void handle_new_subsurface(void* data);

    View& view;

    wlr::surface_t* wlr_surface;

  protected:
    wl::Listener on_commit;
    wl::Listener on_new_subsurface;
  };

  struct Subsurface : ViewChild {
    Subsurface(View& view, wlr::subsurface_t* wlr_subsurface);
    wlr::subsurface_t* wlr_subsurface;

  protected:
    wl::Listener on_destroy;
  };

  struct WlShellPopup : ViewChild {
    WlShellPopup(View&, wlr::wl_shell_surface_t* wlr_popup);
    wlr::wl_shell_surface_t* wlr_popup;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_set_state;
    wl::Listener on_new_popup;
  };

  struct XdgPopupV6 : ViewChild {
    XdgPopupV6(View&, wlr::xdg_popup_v6_t* wlr_popup);
    wlr::xdg_popup_v6_t* wlr_popup;

    void unconstrain();

  protected:
    wl::Listener on_destroy;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_new_popup;
  };

  struct XdgPopup : ViewChild {
    XdgPopup(View&, wlr::xdg_popup_t* wlr_popup);
    wlr::xdg_popup_t* wlr_popup;

    void unconstrain();

  protected:
    wl::Listener on_destroy;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_new_popup;
  };

  enum struct ViewType {
    wl_shell,
    xdg_shell_v6,
    xdg_shell,
#ifdef WLR_HAS_XWAYLAND
    xwayland,
#endif
  };

  enum struct DecoPart {
    none = 0,
    top_border = (1 << 0),
    bottom_border = (1 << 1),
    left_border = (1 << 2),
    right_border = (1 << 3),
    titlebar = (1 << 4),
  };
  CLOTH_ENABLE_BITMASK_OPS(DecoPart);


  struct View {
    View(Workspace& workspace);
    virtual ~View() noexcept;

    wlr::box_t get_box() const;
    void activate(bool active);
    void move(double x, double y);
    void resize(int width, int height);
    void move_resize(double x, double y, int width, int height);
    void move_resize(wlr::box_t);
    void maximize(bool maximized);
    void set_fullscreen(bool fullscreen, wlr::output_t* output);
    void rotate(float rotation);
    void cycle_alpha();
    void close();
    bool center();
    void setup();
    void teardown();

    void apply_damage();
    void damage_whole();
    void update_position(double x, double y);
    void update_size(uint32_t width, uint32_t height);
    void update_decorated(bool decorated);
    void initial_focus();
    void map(wlr::surface_t& surface);
    void unmap();
    void arrange_maximized();

    /// Is this view the currently focused view in its workspace
    bool is_focused();

    wlr::box_t get_deco_box() const;
    DecoPart get_deco_part(double sx, double sy);

    ViewChild& create_child(wlr::surface_t&);
    Subsurface& create_subsurface(wlr::subsurface_t& wlr_subsurface);

    bool at(double lx, double ly, wlr::surface_t*& surface, double& sx, double& sy);

    ViewType type() noexcept;

    util::non_null_ptr<Workspace> workspace;
    Desktop& desktop;

    bool mapped = false;
    bool active = false;
    double x, y;
    uint32_t width, height;
    float rotation = 0;
    float alpha = 1;

    bool decorated = false;
    int border_width = 0;
    int titlebar_height = 0;

    bool maximized;

    Output* fullscreen_output = nullptr;
    wlr::surface_t* wlr_surface = nullptr;

    util::ptr_vec<ViewChild> children;

    struct : wlr::box_t {
      float rotation;
    } saved;

    struct {
      bool update_x, update_y;
      double x, y;
      uint32_t width, height;
    } pending_move_resize;

    struct {
      wl::Signal unmap;
      wl::Signal destroy;
    } events;

    virtual auto get_name() -> std::string = 0;

  protected:
    wl::Listener on_new_subsurface;

    virtual void do_activate(bool active) {}
    virtual void do_move(double x, double y);
    virtual void do_resize(int width, int height) {}
    virtual void do_move_resize(double x, double y, int width, int height);
    virtual void do_maximize(bool maximized) {}
    virtual void do_set_fullscreen(bool fullscreen) {}
    virtual void do_close() {}
    virtual void do_destroy() {}

  private:
    void update_output(std::optional<wlr::box_t> before = std::nullopt) const;
    wlr::output_t* get_output();
    void child_handle_commit(void* data);
    void child_handle_new_subsurface(void* data);
    void handle_new_subsurface(void* data);
  };

  struct WlShellSurface : View {
    WlShellSurface(Workspace& workspace, wlr::wl_shell_surface_t* wlr_surface);
    wlr::wl_shell_surface_t* wl_shell_surface;

    WlShellPopup& create_popup(wlr::wl_shell_surface_t& wlr_popup);

    auto get_name() -> std::string override;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_new_popup;
    wl::Listener on_request_move;
    wl::Listener on_request_resize;
    wl::Listener on_request_maximize;
    wl::Listener on_request_fullscreen;
    wl::Listener on_set_state;

    wl::Listener on_surface_commit;

    void do_resize(int width, int height) override;
    void do_close() override;
  };

  struct XdgSurfaceV6 : View {
    XdgSurfaceV6(Workspace& workspace, wlr::xdg_surface_v6_t* wlr_surface);
    wlr::xdg_surface_v6_t* xdg_surface;

    uint32_t pending_move_resize_configure_serial;

    XdgPopupV6& create_popup(wlr::xdg_popup_v6_t& wlr_popup);

    auto get_name() -> std::string override;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_new_popup;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_request_move;
    wl::Listener on_request_resize;
    wl::Listener on_request_maximize;
    wl::Listener on_request_fullscreen;

    wl::Listener on_surface_commit;

    void do_activate(bool active) override;

    void do_resize(int width, int height) override;
    void do_move_resize(double x, double y, int width, int height) override;
    void do_maximize(bool maximized) override;
    void do_set_fullscreen(bool fullscreen) override;
    void do_close() override;

  private:
    wlr::box_t get_size();
    void apply_size_constraints(int width, int height, int& dest_width, int& dest_height);
  };

  struct XdgToplevelDecoration;

  struct XdgSurface : View {
    XdgSurface(Workspace& workspace, wlr::xdg_surface_t* wlr_surface);
    ~XdgSurface() noexcept;

    wlr::xdg_surface_t* xdg_surface;

    std::unique_ptr<XdgToplevelDecoration> xdg_toplevel_decoration;

    uint32_t pending_move_resize_configure_serial;

    XdgPopup& create_popup(wlr::xdg_popup_t& wlr_popup);
    auto get_name() -> std::string override;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_new_popup;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_request_move;
    wl::Listener on_request_resize;
    wl::Listener on_request_maximize;
    wl::Listener on_request_fullscreen;

    wl::Listener on_surface_commit;

    void do_activate(bool active) override;

    void do_resize(int width, int height) override;
    void do_move_resize(double x, double y, int width, int height) override;
    void do_maximize(bool maximized) override;
    void do_set_fullscreen(bool fullscreen) override;
    void do_close() override;

  private:
    wlr::box_t get_size();
    void apply_size_constraints(int width, int height, int& dest_width, int& dest_height);
  };

  struct XdgToplevelDecoration {
    XdgToplevelDecoration(XdgSurface& , wlr::xdg_toplevel_decoration_v1_t& xdg_deco);

    XdgSurface& surface;
    wlr::xdg_toplevel_decoration_v1_t& wlr_decoration;

    wl::Listener on_destroy;
    wl::Listener on_request_mode;
    wl::Listener on_surface_commit;
  };

  struct XwaylandSurface : View {
    XwaylandSurface(Workspace& workspace, wlr::xwayland_surface_t* wlr_surface);
    wlr::xwayland_surface_t* xwayland_surface;

    void do_activate(bool active) override;
    void do_resize(int width, int height) override;
    void do_move(double x, double y) override;
    void do_move_resize(double x, double y, int width, int height) override;
    void do_maximize(bool maximized) override;
    void do_set_fullscreen(bool fullscreen) override;
    void do_close() override;

    ViewChild& create_popup(wlr::surface_t& wlr_popup)
    {
      assert(false);
    }

    auto get_name() -> std::string override;

  protected:
    wl::Listener on_destroy;
    wl::Listener on_request_configure;
    wl::Listener on_request_move;
    wl::Listener on_request_resize;
    wl::Listener on_request_maximize;
    wl::Listener on_request_fullscreen;
    wl::Listener on_map;
    wl::Listener on_unmap;
    wl::Listener on_set_title;

    wl::Listener on_surface_commit;

    void apply_size_constraints(int width, int height, int& dest_width, int& dest_height);
  };

} // namespace cloth
