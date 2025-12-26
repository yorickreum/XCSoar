
// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "LogFile.hpp"
#include "NOTAMConfigPanel.hpp"
#include "ConfigPanel.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Form/DataField/Listener.hpp"
#include "Form/DataField/Boolean.hpp"
#include "Profile/Keys.hpp"
#include "Profile/Profile.hpp"
#include "Language/Language.hpp"
#include "Airspace/AirspaceComputerSettings.hpp"
#include "Interface.hpp"
#include "UIGlobals.hpp"
#include "net/http/Features.hpp"
#include "NetComponents.hpp"
#include "Components.hpp"
#include "NOTAM/NOTAMGlue.hpp"
#include "Formatter/UserUnits.hpp"
#include "Units/Units.hpp"
#include "Units/Descriptor.hpp"
#include "ui/event/PeriodicTimer.hpp"

#include <ctime>

enum ControlIndex {
#ifdef HAVE_HTTP
  EnableNOTAM,
  NOTAMRadius,
  RefreshInterval,
  LastUpdate,
  DistanceFromLastUpdate,
  TotalNOTAMs,
  FinalCount,
  ShowIFR,
  IFRFiltered,
  ShowOnlyEffective,
  TimeFiltered,
  MaxRadius,
  RadiusFiltered,
  HiddenQCodes,
  QCodeFiltered
#endif
};

class NOTAMConfigPanel : public RowFormWidget, DataFieldListener {
  UI::PeriodicTimer timer{[this]{ OnTimer(); }};

public:
  NOTAMConfigPanel()
    :RowFormWidget(UIGlobals::GetDialogLook()) {}

public:
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
  void Show(const PixelRect &rc) noexcept override;
  void Hide() noexcept override;
  bool Save(bool &changed) noexcept override;

private:
  void OnUpdateButton() noexcept;
  void UpdateVisibility() noexcept;
  void RefreshDisplayFields() noexcept;
  void OnTimer() noexcept;
  
  /* methods from DataFieldListener */
  void OnModified(DataField &df) noexcept override;
};

void
NOTAMConfigPanel::Prepare([[maybe_unused]] ContainerWindow &parent, 
                          [[maybe_unused]] const PixelRect &rc) noexcept
{
#ifdef HAVE_HTTP
  const AirspaceComputerSettings &computer =
    CommonInterface::GetComputerSettings().airspace;

  AddBoolean(_("NOTAM Support"),
             _("Enable downloading and display of NOTAMs from aviation authorities."),
             computer.notam.enabled, this);

  AddInteger(_("Search Radius (km)"),
             _("Radius around current location to fetch NOTAMs."),
             _T("%d km"), _T("%d"), 1, 500, 10,
             computer.notam.radius_km);

  AddInteger(_("Auto-Refresh (minutes)"),
             _("Automatically refresh NOTAMs every X minutes. Set to 0 to disable."),
             _T("%d min"), _T("%d"), 0, 240, 15,
             computer.notam.refresh_interval_min);

  // Display last update time & distance
  std::time_t last_update = 0;
  GeoPoint last_loc = GeoPoint::Invalid();
  if (net_components && net_components->notam) {
    last_update = net_components->notam->GetLastUpdateTime();
    last_loc = net_components->notam->GetLastUpdateLocation();
  }

  if (last_update > 0) {
    TCHAR time_buffer[32];
    const auto *tm = std::localtime(&last_update);
    _sntprintf(time_buffer, 32, _T("%04d-%02d-%02d %02d:%02d"),
               tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
               tm->tm_hour, tm->tm_min);
    AddReadOnly(_("Last Update"), nullptr, time_buffer);
  } else {
    AddReadOnly(_("Last Update"), nullptr, _("Never"));
  }

  const auto &basic = CommonInterface::Basic();
  if (basic.location.IsValid() && last_loc.IsValid()) {
    double dist_m = basic.location.Distance(last_loc);
    TCHAR dist_buffer[32];
    FormatUserDistanceSmart(dist_m, dist_buffer, true, 1000.0, 9.999);
    AddReadOnly(_("Distance From Last Update"), nullptr, dist_buffer);
  } else {
    AddReadOnly(_("Distance From Last Update"), nullptr, _("Unknown"));
  }

  // Get NOTAM statistics
  NOTAMGlue::FilterStats stats = {};
  if (net_components && net_components->notam) {
    stats = net_components->notam->GetFilterStats();
  }
  
  TCHAR buffer[64];
  _stprintf(buffer, _T("%u total"), stats.total);
  AddReadOnly(_("NOTAMs"), nullptr, buffer);

  _stprintf(buffer, _T("%u visible"), stats.final_count);
  AddReadOnly(_("After Filtering"), nullptr, buffer);

  // Filter settings with counts
  AddBoolean(_("Show IFR-Only NOTAMs"),
             _("Include NOTAMs for IFR traffic only."),
             computer.notam.show_ifr);
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_ifr);
  AddReadOnly(_T(""), nullptr, buffer);

  AddBoolean(_("Show Only Currently Effective"),
             _("Filter out NOTAMs not currently in effect."),
             computer.notam.show_only_effective);
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_time);
  AddReadOnly(_T(""), nullptr, buffer);

  // Radius filter with user units
  Unit distance_unit = Units::GetUserDistanceUnit();
  double max_radius_user = Units::ToUserDistance(computer.notam.max_radius_m);
  const TCHAR *unit_name = Units::GetUnitName(distance_unit);
  
  TCHAR format_display[32], format_edit[32];
  _stprintf(format_display, _T("%%.0f %s"), unit_name);
  _stprintf(format_edit, _T("%%.0f"));
  
  AddFloat(_("Maximum NOTAM Radius"),
           _("Filter out NOTAMs with radius larger than this. Set to 0 to disable."),
           format_display, format_edit,
           0, 1000, 10, 0,
           max_radius_user, this);
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_radius);
  AddReadOnly(_T(""), nullptr, buffer);

  AddText(_("Hidden Q-Codes"),
          _("Space-separated Q-code prefixes to hide (e.g., QA QK QN QOA QOL)."),
          computer.notam.hidden_qcodes.c_str());
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_qcode);
  AddReadOnly(_T(""), nullptr, buffer);

  UpdateVisibility();
#endif
}

void
NOTAMConfigPanel::Show(const PixelRect &rc) noexcept
{
#ifdef HAVE_HTTP
  ConfigPanel::BorrowExtraButton(1, _("Update Now"), [this](){
    OnUpdateButton();
  });
  
  // Start periodic timer to refresh display fields (every 2 seconds)
  timer.Schedule(std::chrono::seconds(2));
#endif

  RowFormWidget::Show(rc);
}

void
NOTAMConfigPanel::Hide() noexcept
{
#ifdef HAVE_HTTP
  timer.Cancel();
  RowFormWidget::Hide();
  ConfigPanel::ReturnExtraButton(1);
#else
  RowFormWidget::Hide();
#endif
}

void
NOTAMConfigPanel::OnUpdateButton() noexcept
{
  LogFormat("NOTAM: Manual update triggered from settings panel");
#ifdef HAVE_HTTP
  if (net_components && net_components->notam) {
    // Invalidate cache to force fresh fetch
    net_components->notam->InvalidateCache();
    
    const auto &basic = CommonInterface::Basic();
    if (basic.location.IsValid()) {
      // Trigger NOTAM update for current location
      net_components->notam->UpdateLocation(basic.location);
      // Display fields will be refreshed by the periodic timer
    }
  }
#endif
}

void
NOTAMConfigPanel::UpdateVisibility() noexcept
{
#ifdef HAVE_HTTP
  const DataFieldBoolean &df = (const DataFieldBoolean &)GetDataField(EnableNOTAM);
  const bool enabled = df.GetValue();
  
  SetRowAvailable(NOTAMRadius, enabled);
  SetRowAvailable(RefreshInterval, enabled);
  SetRowAvailable(LastUpdate, enabled);
  SetRowAvailable(DistanceFromLastUpdate, enabled);
  SetRowAvailable(TotalNOTAMs, enabled);
  SetRowAvailable(ShowIFR, enabled);
  SetRowAvailable(IFRFiltered, enabled);
  SetRowAvailable(ShowOnlyEffective, enabled);
  SetRowAvailable(TimeFiltered, enabled);
  SetRowAvailable(MaxRadius, enabled);
  SetRowAvailable(RadiusFiltered, enabled);
  SetRowAvailable(HiddenQCodes, enabled);
  SetRowAvailable(QCodeFiltered, enabled);
  SetRowAvailable(FinalCount, enabled);
#endif
}

void
NOTAMConfigPanel::RefreshDisplayFields() noexcept
{
#ifdef HAVE_HTTP
  if (!net_components || !net_components->notam)
    return;

  // Refresh last update time
  std::time_t last_update = net_components->notam->GetLastUpdateTime();
  if (last_update > 0) {
    TCHAR time_buffer[32];
    const auto *tm = std::localtime(&last_update);
    _sntprintf(time_buffer, 32, _T("%04d-%02d-%02d %02d:%02d"),
               tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday,
               tm->tm_hour, tm->tm_min);
    SetText(LastUpdate, time_buffer);
  } else {
    SetText(LastUpdate, _("Never"));
  }

  // Refresh distance from last update location
  const auto &basic = CommonInterface::Basic();
  GeoPoint last_loc = net_components->notam->GetLastUpdateLocation();
  
  // Debug logging
  LogFormat("NOTAM Panel: current_loc=%.6f,%.6f (available=%d, valid=%d), last_loc=%.6f,%.6f (valid=%d)",
            basic.location.latitude.Degrees(), basic.location.longitude.Degrees(),
            (int)basic.location_available.IsValid(), (int)basic.location.IsValid(),
            last_loc.latitude.Degrees(), last_loc.longitude.Degrees(),
            (int)last_loc.IsValid());
  
  // Check both that location is valid AND that it's actually available (has GPS fix)
  if (basic.location_available && basic.location.IsValid() && last_loc.IsValid()) {
    double dist_m = basic.location.Distance(last_loc);
    LogFormat("NOTAM Panel: distance = %.2f m (%.2f km)", dist_m, dist_m / 1000.0);
    TCHAR dist_buffer[32];
    FormatUserDistanceSmart(dist_m, dist_buffer, true, 1000.0, 9.999);
    SetText(DistanceFromLastUpdate, dist_buffer);
  } else {
    SetText(DistanceFromLastUpdate, _("Unknown"));
  }

  // Update NOTAM statistics
  NOTAMGlue::FilterStats stats = {};
  if (net_components && net_components->notam) {
    stats = net_components->notam->GetFilterStats();
  }
  
  TCHAR buffer[64];
  _stprintf(buffer, _T("%u total"), stats.total);
  SetText(TotalNOTAMs, buffer);
  
  _stprintf(buffer, _T("%u visible"), stats.final_count);
  SetText(FinalCount, buffer);
  
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_ifr);
  SetText(IFRFiltered, buffer);
  
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_time);
  SetText(TimeFiltered, buffer);
  
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_radius);
  SetText(RadiusFiltered, buffer);
  
  _stprintf(buffer, _T("%u filtered"), stats.filtered_by_qcode);
  SetText(QCodeFiltered, buffer);
#endif
}

void
NOTAMConfigPanel::OnTimer() noexcept
{
  RefreshDisplayFields();
}

void
NOTAMConfigPanel::OnModified(DataField &df) noexcept
{
#ifdef HAVE_HTTP
  if (IsDataField(EnableNOTAM, df)) {
    UpdateVisibility();
  }
#endif
}

bool
NOTAMConfigPanel::Save(bool &_changed) noexcept
{
  bool changed = false;

#ifdef HAVE_HTTP
  AirspaceComputerSettings &computer =
    CommonInterface::SetComputerSettings().airspace;

  changed |= SaveValue(EnableNOTAM, ProfileKeys::NOTAMEnabled, computer.notam.enabled);
  changed |= SaveValueInteger(NOTAMRadius, ProfileKeys::NOTAMRadius, computer.notam.radius_km);
  changed |= SaveValueInteger(RefreshInterval, ProfileKeys::NOTAMRefreshInterval, computer.notam.refresh_interval_min);

  // Filter settings
  changed |= SaveValue(ShowIFR, ProfileKeys::NOTAMShowIFR, computer.notam.show_ifr);
  changed |= SaveValue(ShowOnlyEffective, ProfileKeys::NOTAMShowOnlyEffective, computer.notam.show_only_effective);
  
  // Radius filter - convert from user units to meters
  double max_radius_user = GetValueFloat(MaxRadius);
  unsigned max_radius_m = (unsigned)Units::ToSysDistance(max_radius_user);
  if (computer.notam.max_radius_m != max_radius_m) {
    computer.notam.max_radius_m = max_radius_m;
    Profile::Set(ProfileKeys::NOTAMMaxRadius, max_radius_m);
    changed = true;
  }
  
  changed |= SaveValue(HiddenQCodes, ProfileKeys::NOTAMHiddenQCodes, computer.notam.hidden_qcodes);
#endif

  _changed |= changed;

  return true;
}

std::unique_ptr<Widget>
CreateNOTAMConfigPanel()
{
  return std::make_unique<NOTAMConfigPanel>();
}
