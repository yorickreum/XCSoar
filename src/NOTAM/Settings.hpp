// SPDX-License-Identifier: GPL-2.0-or-later
// Copyright The XCSoar Project

#pragma once

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
   * Comma-separated list of Q-code prefixes to hide.
   * Default: "QK,QN,QOL" (hide checklist/admin, navaids, obstacle lights)
   * 
   * Q-code meanings (selection, see https://www.faa.gov/air_traffic/publications/atpubs/notam_html/appendix_b.html):
   * - QA: Aerodrome / large area operational info
   * - QF: Facilities & services
   * - QK: Checklist / Admin
   * - QM: Movement area / runway / taxiways
   * - QN: NAVAIDs
   * - QO: Obstacles (excluding QOL)
   * - QOL: Obstacle Lights
   * - QR: Runway / ops status
   * - QW: Airspace warnings / hazards
   */
  std::string hidden_qcodes = "QK,QN,QOL";
};