// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.

#pragma once
#include <noggit/database/ClientDatabase.h>

#include <cstdint>
#include <string>

// experimental

struct MapDbRow : BlizzardDatabaseLib::Structures::BlizzardDatabaseRow
{
  MapDbRow() = default;
  explicit MapDbRow(BlizzardDatabaseLib::Structures::BlizzardDatabaseRow const& row)
    : BlizzardDatabaseLib::Structures::BlizzardDatabaseRow(row)
  {}

  std::uint32_t AreaType() const
  {
    return static_cast<std::uint32_t>(std::stoul(Columns.at("InstanceType").Value));
  }
};

class MapDb : public Noggit::ClientDatabaseTable
{
public:
  MapDb() :
    ClientDatabaseTable("Map")
  {}

  std::optional<MapDbRow> mapRecordById(unsigned int id)
  {
    auto record = RecordById(id);

    if (!record)
      return std::nullopt;

    return MapDbRow(record.value());
  }

  /// Fields
  static const size_t MapID = 0;        // uint
  static const size_t InternalName = 1;    // string
  static const size_t AreaType = 2;      // uint
  static const size_t Flags = 3;      // uint
  static const size_t IsBattleground = 4;    // uint
  static const size_t Name = 5;        // loc
  static const size_t AreaTableID = 22;    // uint
  static const size_t MapDescriptionAlliance = 23;    // loc
  static const size_t MapDescriptionHorde = 40;    // loc
  static const size_t LoadingScreen = 57;    // uint [LoadingScreen]
  static const size_t minimapIconScale = 58;    // uint [LoadingScreen]
  static const size_t corpseMapID = 59; //	iRefID	Points to column 1, -1 if none
  static const size_t corpseX = 60; //	Float	The X - Coord of the instance entrance
  static const size_t corpseY = 61; //	Float	The Y - Coord of the instance entrance
  static const size_t TimeOfDayOverride = 62; //	Integer	Set to - 1 for everything but Orgrimmar and Dalaran arena.For those, the time of day will change to this.
  static const size_t ExpansionID = 63; //	Integer	Vanilla : 0, BC : 1, WotLK : 2
  static const size_t RaidOffset = 64; //	Integer
  static const size_t NumberOfPlayers = 65; //	Integer	Used for reset time?
};
