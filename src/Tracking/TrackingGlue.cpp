/*
Copyright_License {

  XCSoar Glide Computer - http://www.xcsoar.org/
  Copyright (C) 2000-2012 The XCSoar Project
  A detailed list of copyright holders can be found in the file "AUTHORS".

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
}
*/

#include "TrackingGlue.hpp"
#include "NMEA/MoreData.hpp"
#include "NMEA/Derived.hpp"
#include "Units/System.hpp"
#include "Util/Macros.hpp"


static LiveTrack24::VehicleType
MapVehicleTypeToLifetrack24(TrackingSettings::VehicleType vt)
{
  static const LiveTrack24::VehicleType vehicleTypeMap[] = {
    LiveTrack24::VehicleType::GLIDER,
    LiveTrack24::VehicleType::PARAGLIDER,
    LiveTrack24::VehicleType::POWERED_AIRCRAFT,
    LiveTrack24::VehicleType::HOT_AIR_BALLOON
  };

  unsigned vti = (unsigned) vt;
  if (vti >= ARRAY_SIZE(vehicleTypeMap))
    vti = 0;

  return vehicleTypeMap[vti];
}

TrackingGlue::TrackingGlue()
{
  settings.SetDefaults();
  LiveTrack24::SetServer(settings.livetrack24.server);
}

void
TrackingGlue::StopAsync()
{
  ScopeLock protect(mutex);
  StandbyThread::StopAsync();
}

void
TrackingGlue::WaitStopped()
{
  ScopeLock protect(mutex);
  StandbyThread::WaitStopped();
}

void
TrackingGlue::SetSettings(const TrackingSettings &_settings)
{
  ScopeLock protect(mutex);

  if (_settings.livetrack24.server != settings.livetrack24.server) {
    state.ResetSession();
    LiveTrack24::SetServer(_settings.livetrack24.server);
  }

  if (_settings.livetrack24.username != settings.livetrack24.username ||
      _settings.livetrack24.password != settings.livetrack24.password)
    state.ResetSession();

  settings = _settings;
}

void
TrackingGlue::OnTimer(const MoreData &basic, const DerivedInfo &calculated)
{
  if (!settings.livetrack24.enabled)
    /* disabled by configuration */
    /* note that we are allowed to read "settings" without locking the
       mutex, because the background thread never writes to this
       attribute */
    return;

  if (!basic.time_available || !basic.location_available)
    /* can't track without a valid GPS fix */
    return;

  if (!clock.CheckUpdate(settings.interval * 1000))
    /* later */
    return;

  ScopeLock protect(mutex);
  if (IsBusy())
    /* still running, skip this submission */
    return;

  date_time = basic.date_time_utc;
  if (!basic.date_available)
    /* use "today" if the GPS didn't provide a date */
    (BrokenDate &)date_time = (BrokenDate)BrokenDateTime::NowUTC();

  location = basic.location;
  /* XXX use nav_altitude? */
  altitude = basic.NavAltitudeAvailable() && positive(basic.nav_altitude)
    ? (unsigned)basic.nav_altitude
    : 0u;
  ground_speed = basic.ground_speed_available
    ? (unsigned)Units::ToUserUnit(basic.ground_speed, Unit::KILOMETER_PER_HOUR)
    : 0u;
  track = basic.track_available
    ? basic.track
    : Angle::Zero();

  flying = calculated.flight.flying;

  Trigger();
}

void
TrackingGlue::Tick()
{
  if (!settings.livetrack24.enabled)
    /* settings have been cleared meanwhile, bail out */
    return;

  unsigned tracking_interval = settings.interval;
  LiveTrack24Settings copy = this->settings.livetrack24;

  mutex.Unlock();

  const int64_t current_timestamp = date_time.ToUnixTimeUTC();

  if (state.HasSession() && current_timestamp + 60 < last_timestamp) {
    /* time warp: create a new session */
    LiveTrack24::EndTracking(state.session_id, state.packet_id);
    state.ResetSession();
  }

  last_timestamp = current_timestamp;

  if (!state.HasSession()) {
    LiveTrack24::UserID user_id = 0;
    if (!copy.username.empty() && !copy.password.empty())
      user_id = LiveTrack24::GetUserID(copy.username, copy.password);

    if (user_id == 0) {
      copy.username.clear();
      copy.password.clear();
      state.session_id = LiveTrack24::GenerateSessionID();
    } else {
      state.session_id = LiveTrack24::GenerateSessionID(user_id);
    }

    if (!LiveTrack24::StartTracking(state.session_id, copy.username,
                                    copy.password, tracking_interval,
                                    MapVehicleTypeToLifetrack24(settings.vehicleType))) {
      state.ResetSession();
      mutex.Lock();
      return;
    }

    state.packet_id = 2;
  }

  LiveTrack24::SendPosition(state.session_id, state.packet_id++,
                            location, altitude, ground_speed, track,
                            current_timestamp);

  mutex.Lock();
}
