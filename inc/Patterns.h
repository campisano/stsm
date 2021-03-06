// Copyright (C) 2017 Riccardo Campisano <riccardo.campisano@gmail.com>
//               2017 Fabio Porto
//               2017 Fabio Perosi
//               2017 Esther Pacitti
//               2017 Florent Masseglia
//               2017 Eduardo Ogasawara
//
// This file is part of STSM (Spatio-Temporal Sequence Miner).
//
// STSM is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// STSM is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with STSM.  If not, see <http://www.gnu.org/licenses/>.

#ifndef Patterns__H__
#define Patterns__H__

#include <map>

#include "BlockedSequence.h"
#include "Position.h"
#include "RangedSequence.h"

class Patterns
{
public:
    MapRangedSequencesByLength m_solid_ranged_sequences;

    typedef std::map <
    const RangedSequence *,
          ListPositions
          > MapPositionsBySeq;            // mapping positions by sequence
    MapPositionsBySeq m_ranged_sequence_positions;

    MapBlockedSequencesByLength m_solid_blocked_sequences;
};

#endif
