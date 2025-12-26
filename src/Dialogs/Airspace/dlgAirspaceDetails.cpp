// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#include "Airspace.hpp"
#include "Dialogs/WidgetDialog.hpp"
#include "Widget/RowFormWidget.hpp"
#include "Airspace/AbstractAirspace.hpp"
#include "Airspace/ProtectedAirspaceWarningManager.hpp"
#include "Formatter/UserUnits.hpp"
#include "Formatter/AirspaceFormatter.hpp"
#include "Formatter/TimeFormatter.hpp"
#include "time/BrokenDateTime.hpp"
#include "UIGlobals.hpp"
#include "Interface.hpp"
#include "Components.hpp"
#include "NetComponents.hpp"
#include "NOTAM/NOTAMGlue.hpp"
#include "ActionInterface.hpp"
#include "Language/Language.hpp"
#include "TransponderMode.hpp"
#include "util/StaticString.hxx"

#include <cassert>

class AirspaceDetailsWidget
  : public RowFormWidget {
protected:
  ConstAirspacePtr airspace;
  ProtectedAirspaceWarningManager *warnings;

public:
  /**
   * Hack to allow the widget to close its surrounding dialog.
   */
  WndForm *dialog;

  AirspaceDetailsWidget(ConstAirspacePtr _airspace,
                        ProtectedAirspaceWarningManager *_warnings)
    :RowFormWidget(UIGlobals::GetDialogLook()),
     airspace(std::move(_airspace)), warnings(_warnings) {}

  void AckDayOrEnable() noexcept;

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void
AirspaceDetailsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                               [[maybe_unused]] const PixelRect &rc) noexcept
{
  const NMEAInfo &basic = CommonInterface::Basic();

  StaticString<64> buffer;

  AddMultiLine(airspace->GetName());

  const TransponderCode transponderCode = airspace->GetTransponderCode();
  TCHAR buffer2[5];

  transponderCode.Format(buffer2, sizeof(buffer2));

  if (transponderCode.IsDefined()) {
    AddReadOnly(_("Squawk code"), nullptr, buffer2);
    AddButton(_("Set Squawk Code"), [transponderCode]() {
      ActionInterface::SetTransponderCode(transponderCode);
    });
  }

  if (airspace->GetRadioFrequency().Format(buffer.data(), buffer.capacity()) !=
      nullptr) {
    buffer += _T(" MHz");
    AddReadOnly(_("Radio"), nullptr, buffer);

    const TCHAR *frequencyName = airspace->GetName();
    const TCHAR *stationName = airspace->GetStationName();

    if (stationName != nullptr && stationName[0] != '\0') {
      AddReadOnly(_("Station"), nullptr, stationName);
      frequencyName = stationName;
    }

    AddButton(_("Set Active Frequency"), [this, frequencyName]() {
      ActionInterface::SetActiveFrequency(airspace->GetRadioFrequency(),
                                          frequencyName);
    });

    AddButton(_("Set Standby Frequency"), [this, frequencyName]() {
      ActionInterface::SetStandbyFrequency(airspace->GetRadioFrequency(),
                                           frequencyName);
    });
  }

  AddReadOnly(_("Class"), nullptr, AirspaceFormatter::GetClassShort(*airspace));
  AddReadOnly(_("Type"), nullptr, AirspaceFormatter::GetType(*airspace));

  AirspaceFormatter::FormatAltitude(buffer.data(), airspace->GetTop());
  AddReadOnly(_("Top"), nullptr, buffer);

  AirspaceFormatter::FormatAltitude(buffer.data(), airspace->GetBase());
  AddReadOnly(_("Base"), nullptr, buffer);

  if (warnings != nullptr) {
    const GeoPoint closest =
      airspace->ClosestPoint(basic.location, warnings->GetProjection());
    const auto distance = closest.Distance(basic.location);

    AddReadOnly(_("Distance"), nullptr, FormatUserDistance(distance));
  }
}

void
AirspaceDetailsWidget::AckDayOrEnable() noexcept
{
  assert(warnings != nullptr);

  const bool acked = warnings->GetAckDay(*airspace);
  warnings->AcknowledgeDay(airspace, !acked);

  dialog->SetModalResult(mrOK);
}

/**
 * Extended widget for displaying NOTAM-specific information
 */
class NOTAMDetailsWidget final : public AirspaceDetailsWidget {
public:
  NOTAMDetailsWidget(ConstAirspacePtr _airspace,
                     ProtectedAirspaceWarningManager *_warnings)
    : AirspaceDetailsWidget(std::move(_airspace), _warnings) {}

  /* virtual methods from class Widget */
  void Prepare(ContainerWindow &parent, const PixelRect &rc) noexcept override;
};

void
dlgAirspaceDetails(ConstAirspacePtr airspace,
                   ProtectedAirspaceWarningManager *warnings)
{
  // Use specialized widget for NOTAMs
  const bool is_notam = airspace->GetType() == AirspaceClass::NOTAM;
  
  AirspaceDetailsWidget *widget = is_notam
    ? (AirspaceDetailsWidget *)new NOTAMDetailsWidget(airspace, warnings)
    : new AirspaceDetailsWidget(airspace, warnings);
  
  const TCHAR *title = is_notam ? _("NOTAM Details") : _("Airspace Details");
  
  WidgetDialog dialog(WidgetDialog::Auto{}, UIGlobals::GetMainWindow(),
                      UIGlobals::GetDialogLook(),
                      title, widget);

  if (warnings != nullptr) {
    widget->dialog = &dialog;
    dialog.AddButton(warnings->GetAckDay(*airspace)
                     ? _("Enable") : _("Ack Day"),
                     [widget](){ widget->AckDayOrEnable(); });
  }
  dialog.AddButton(_("Close"), mrOK);

  dialog.ShowModal();
}

void
NOTAMDetailsWidget::Prepare([[maybe_unused]] ContainerWindow &parent,
                            [[maybe_unused]] const PixelRect &rc) noexcept
{
  const NMEAInfo &basic = CommonInterface::Basic();
  StaticString<128> buffer;
  
  // Look up the NOTAM data using the number stored in station_name
  const TCHAR *notam_number = airspace->GetStationName();
  const struct NOTAM *notam = nullptr;
  
#ifdef HAVE_HTTP
  if (net_components && net_components->notam && notam_number && notam_number[0] != '\0') {
    // Convert TCHAR to std::string
    std::string number_str;
    for (const TCHAR *p = notam_number; *p; ++p) {
      number_str += static_cast<char>(*p);
    }
    notam = net_components->notam->FindNOTAMByNumber(number_str);
  }
#endif
  
  // Display NOTAM number
  if (notam_number && notam_number[0] != '\0') {
    AddReadOnly(_("NOTAM"), nullptr, notam_number);
  }
  
  // Display ICAO location if we found the NOTAM
  if (notam && !notam->location.empty()) {
    tstring location(notam->location.begin(), notam->location.end());
    AddReadOnly(_("Location"), nullptr, location.c_str());
  }
  
  // Display Q-code (feature type)
  if (notam && !notam->feature_type.empty()) {
    tstring qcode(notam->feature_type.begin(), notam->feature_type.end());
    AddReadOnly(_("Q-Code"), nullptr, qcode.c_str());
  }
  
  // NOTAM text (stored in name field)
  AddMultiLine(airspace->GetName());
  
  // Airspace class and type
  AddReadOnly(_("Type"), nullptr, _T("NOTAM"));
  
  // Validity section
  if (notam) {
    TCHAR time_buffer[64];
    
    // Effective start
    BrokenDateTime start_dt(notam->start_time);
    FormatISO8601(time_buffer, start_dt);
    buffer = _T("From: ");
    buffer += time_buffer;
    AddReadOnly(_("Valid"), nullptr, buffer);
    
    // Effective end
    BrokenDateTime end_dt(notam->end_time);
    // Check if it's a far future date (PERM = permanent)
    auto end_seconds = std::chrono::duration_cast<std::chrono::seconds>(
      notam->end_time.time_since_epoch()).count();
    if (end_seconds > 2147483647) { // Year 2038+ = PERM
      buffer = _T("Until: PERM");
    } else {
      FormatISO8601(time_buffer, end_dt);
      buffer = _T("Until: ");
      buffer += time_buffer;
    }
    AddReadOnly(nullptr, nullptr, buffer);
    
    // Status indicator
    auto now = std::chrono::system_clock::now();
    if (now < notam->start_time) {
      // Not yet active
      auto starts_in = std::chrono::duration_cast<std::chrono::hours>(
        notam->start_time - now);
      if (starts_in.count() < 48) {
        buffer.Format(_T("Starts in %dh"), static_cast<int>(starts_in.count()));
      } else {
        auto days = starts_in.count() / 24;
        buffer.Format(_T("Starts in %dd"), static_cast<int>(days));
      }
    } else if (now > notam->end_time) {
      // Expired
      auto expired_ago = std::chrono::duration_cast<std::chrono::hours>(
        now - notam->end_time);
      if (expired_ago.count() < 48) {
        buffer.Format(_T("Expired %dh ago"), static_cast<int>(expired_ago.count()));
      } else {
        auto days = expired_ago.count() / 24;
        buffer.Format(_T("Expired %dd ago"), static_cast<int>(days));
      }
    } else {
      // Currently active
      buffer = _T("Active");
    }
    AddReadOnly(_("Now"), nullptr, buffer);
  }
  
  // Altitude information
  AirspaceFormatter::FormatAltitude(buffer.data(), airspace->GetTop());
  AddReadOnly(_("Top"), nullptr, buffer);
  
  AirspaceFormatter::FormatAltitude(buffer.data(), airspace->GetBase());
  AddReadOnly(_("Base"), nullptr, buffer);
  
  // Distance calculation
  if (warnings != nullptr) {
    const GeoPoint closest =
      airspace->ClosestPoint(basic.location, warnings->GetProjection());
    const auto distance = closest.Distance(basic.location);
    AddReadOnly(_("Distance"), nullptr, FormatUserDistance(distance));
  }
}
