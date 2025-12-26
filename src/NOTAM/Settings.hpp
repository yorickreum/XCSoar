// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

#include "util/StaticString.hxx"

/**
 * Settings for NOTAM (Notice to Airmen) support
 */
struct NOTAMSettings {
  /** Enable/disable NOTAM loading */
  bool enabled = false;
  
  /** Radius around current location to fetch NOTAMs for */
  unsigned radius_km = 50;
  
  /** Maximum number of NOTAMs to fetch */
  unsigned max_notams = 500;
  
  /** Refresh interval - 0 = manual only */
  unsigned refresh_interval_min = 30;
  
  /** Base URL for the NOTAM API */
  const char *api_base_url = "https://enroute-data.akaflieg-freiburg.de/enrouteProxy/notam.php";

  /** Show only currently effective NOTAMs */
  bool show_only_effective = true;
  
  /** Show IFR-only NOTAMs ("traffic" == "I") */
  bool show_ifr = false;
  
  /** 
   * Maximum NOTAM radius to show (in meters). NOTAMs with larger radius are filtered out.
   * Default: 100000 meters (100 km, hides very large area NOTAMs)
   * Set to 0 to disable this filter.
   * Displayed to user in their preferred distance unit.
   */
  unsigned max_radius_m = 100000;
  
  /** 
   * Comma-separated list of Q-code prefixes to hide.
   * Default: "QA QK QN QOA QOBTT QOL" (hide administrative, checklist/admin, navaids, obstacles, obstacle lights)
   * 
   * Q-code meanings (selection, see https://www.faa.gov/air_traffic/publications/atpubs/notam_html/appendix_b.html):
   * - QA: Aerodrome / Administrative large area operational info
   * - QF: Facilities & services
   * - QK: Checklist / Admin
   * - QM: Movement area / runway / taxiways
   * - QN: NAVAIDs
   * - QO: Obstacles (excluding QOL)
   * - QOA: Obstacles - general
   * - QOBTT: Obstacles - temporary
   * - QOL: Obstacle Lights
   * - QR: Runway / ops status
   * - QW: Airspace warnings / hazards
   */
  StaticString<64> hidden_qcodes{"QA QK QN QOA QOBTT QOL"};
};