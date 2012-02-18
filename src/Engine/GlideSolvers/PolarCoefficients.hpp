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

#ifndef XCSOAR_POLAR_COEFF_HPP
#define XCSOAR_POLAR_COEFF_HPP

#include "Math/fixed.hpp"
#include "Compiler.h"

struct PolarCoefficients
{
  fixed a, b, c;

  PolarCoefficients() = default;
  PolarCoefficients(fixed _a, fixed _b, fixed _c):a(_a), b(_b), c(_c) {}

  /**
   * Construct an invalid object.
   */
  gcc_const
  static PolarCoefficients Invalid() {
    return PolarCoefficients(fixed_zero, fixed_zero, fixed_zero);
  }

  void SetInvalid() {
    a = b = c = fixed_zero;
  }

  gcc_pure
  bool IsValid() const;

  /**
   * Calculates the three polynomial polar coefficients from
   * three pairs of horizontal vs. vertical speed.
   */
  gcc_pure
  static PolarCoefficients From3VW(fixed v1, fixed v2, fixed v3,
                                  fixed w1, fixed w2, fixed w3);

  /**
   * Calculates the three polynomial polar coefficients from
   * two pairs of horizontal vs. vertical speed. The first pair defines
   * the point where the polar is flat (derivative equals zero)!
   */
  gcc_pure
  static PolarCoefficients From2VW(fixed v1, fixed v2, fixed w1, fixed w2);
};

#endif
