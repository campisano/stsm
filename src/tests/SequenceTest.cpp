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

#include <catch.hpp>
#include <string>

#include "Sequence.h"

TEST_CASE(
    "Sequence equality operator",
    "[Sequence]")
{
    // Arrange
    Sequence seq1, seq2;
    seq1.set("<abcdefghilm>");
    seq2.set("<abcdefghilm>");

    // Assert
    CHECK(seq1 == seq2);
}

TEST_CASE(
    "Sequence string representation",
    "[Sequence]")
{
    // Arrange
    Sequence seq1;
    std::string str_items = "abcdefghilm";
    std::string str_seq = "<" + str_items + ">";
    seq1.set(str_seq);

    // Assert
    CHECK(seq1.toString() == str_seq);
    CHECK(seq1.toStringOfItems() == str_items);
}
