#pragma once
#include <vector>
#include <string>

struct LevelEntry { std::string rimPrefix; std::string displayName; };
struct LevelSubFolder { std::string name; std::vector<LevelEntry> entries; };
struct LevelSection {
    std::string name;
    std::vector<LevelEntry> entries;
    std::vector<LevelSubFolder> subFolders;
};
struct LevelGame { std::string name; std::vector<LevelSection> sections; };

inline std::vector<LevelGame> buildLevelDatabase() {
    std::vector<LevelGame> db;
    { LevelGame game; game.name = "Dragon Age: Origins";
      { LevelSection sec; sec.name = "Global";
        sec.entries.push_back({"oth999d", "Companion Selection Stage"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Background - Dwarf Commoner";
        sec.entries.push_back({"orz100d", "Orzammar Commons"});
        sec.entries.push_back({"orz102d", "Beraht's Shop"});
        sec.entries.push_back({"orz103d", "Beraht's Hideout"});
        sec.entries.push_back({"orz101d", "Tapster's Tavern"});
        sec.entries.push_back({"orz500d", "Grounds"});
        sec.entries.push_back({"orz200d", "Dust Town"});
        sec.entries.push_back({"orz201d", "Player's Home"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Background - Dwarf Noble";
        sec.entries.push_back({"orz300d", "Diamond Quarter"});
        sec.entries.push_back({"orz500d", "Orzammar Proving Grounds"});
        sec.entries.push_back({"orz303d", "Orzammar Royal Palace"});
        sec.entries.push_back({"orz701d", "Ruined Thaig"});
        sec.entries.push_back({"orz704d", "Thaig Chamber"});
        sec.entries.push_back({"orz601d", "Orzammar Prison"});
        sec.entries.push_back({"orz800d", "Deep Roads Outskirts"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Background - Elf City";
        sec.entries.push_back({"den200d", "Elven Alienage"});
        sec.entries.push_back({"den201d", "Player's Home"});
        sec.entries.push_back({"den202d", "Alarith's Store"});
        sec.entries.push_back({"den012d", "Arl of Denerim's Estate - Exterior"});
        sec.entries.push_back({"den005d", "Arl of Denerim's Estate - Interior"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Background - Elf Dalish";
        sec.entries.push_back({"brc300d", "Forest Clearing"});
        sec.entries.push_back({"brc200d", "Elven Ruins"});
        sec.entries.push_back({"brc997d", "Dalish Camp"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Background - Human and Elf Mage";
        sec.entries.push_back({"lak303d", "Apprentice Quarters"});
        sec.entries.push_back({"lak304d", "Senior Mage Quarters"});
        sec.entries.push_back({"lak308d", "Storage Caves"});
        sec.entries.push_back({"lak307d", "Harrowing Chamber"});
        sec.entries.push_back({"oth000d", "The Fade"});
        sec.entries.push_back({"lak302d", "Mage Tower - Basement"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Background - Human Noble";
        sec.entries.push_back({"hrt201d", "Castle Cousland"});
        sec.entries.push_back({"hrt201n", "Castle Cousland - Night"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Ostagar";
        sec.entries.push_back({"ost000t", "Kings Camp"});
        sec.entries.push_back({"ost000a", "Kings Camp - Night"});
        sec.entries.push_back({"ost100d", "Korcari Wilds"});
        sec.entries.push_back({"ost101d", "Flemeth's Hut - Exterior"});
        sec.entries.push_back({"ost102d", "Flemeth's Hut - Interior"});
        sec.entries.push_back({"ost001d", "Tower of Ishal - First Floor"});
        sec.entries.push_back({"ost002d", "Tower of Ishal - Second Floor"});
        sec.entries.push_back({"ost003d", "Tower of Ishal - Third Floor"});
        sec.entries.push_back({"ost004d", "Tower of Ishal - Upper Floor"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Lothering";
        sec.entries.push_back({"hrt000d", "Lothering"});
        sec.entries.push_back({"hrd001d", "Chantry"});
        sec.entries.push_back({"hrt002d", "Dane's Refuge"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Party Camp";
        sec.entries.push_back({"brc999d", "Party Camp"});
        sec.entries.push_back({"lgt602d", "On the Road"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Redcliffe Village";
        sec.entries.push_back({"lak100d", "Redcliffe Village"});
        sec.entries.push_back({"1ak100n", "Redcliffe Village - Night"});
        sec.entries.push_back({"lak106d", "Village Chantry"});
        sec.entries.push_back({"lak105d", "Blacksmith's Store"});
        sec.entries.push_back({"lak108d", "Dwyn's Home"});
        sec.entries.push_back({"lak102d", "Kaitlyn's Home - First Floor"});
        sec.entries.push_back({"lak110d", "Kaitlyn's Home - Second Floor"});
        sec.entries.push_back({"lak103d", "Tavern"});
        sec.entries.push_back({"lak104d", "Wilhelm's Cottage"});
        sec.entries.push_back({"lak101d", "General Store"});
        sec.entries.push_back({"lak107d", "Generic Cottage"});
        sec.entries.push_back({"lak109d", "Windmill"});
        sec.entries.push_back({"lak200d", "Redcliffe Castle"});
        sec.entries.push_back({"lak203d", "Redcliffe Castle - Basement"});
        sec.entries.push_back({"lak201d", "Redcliffe Castle - Main Floor"});
        sec.entries.push_back({"lak202d", "Redcliffe Castle - Upper Floor"});
        sec.entries.push_back({"lak250d", "The Fade"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Broken Circle";
        sec.entries.push_back({"lak300d", "Lake Calenhad Docks"});
        sec.entries.push_back({"lgt101d", "The Spoiled Princess"});
        sec.entries.push_back({"lak503d", "Apprentice Quarters"});
        sec.entries.push_back({"lak504d", "Senior Mage Quarters"});
        sec.entries.push_back({"lak505d", "Great Hall"});
        sec.entries.push_back({"lak506d", "Templar Quarters"});
        sec.entries.push_back({"lak507d", "Harrowing Chamber"});
        sec.entries.push_back({"oth001d", "The Raw Fade"});
        sec.entries.push_back({"lak511d", "Burning Tower"});
        sec.entries.push_back({"lak512d", "Darkspawn Invasion"});
        sec.entries.push_back({"lak513d", "Mage Asunder"});
        sec.entries.push_back({"lak514d", "Templar's Nightmare"});
        sec.entries.push_back({"lak501d", "Weisshaupt"});
        sec.entries.push_back({"lak515d", "Sloth Demon's Sanctum"});
        sec.entries.push_back({"lak526d", "Companion's Nightmare"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Nature of the Beast";
        sec.entries.push_back({"brc100d", "West Brecilian Forest"});
        sec.entries.push_back({"brc000d", "Dalish Camp"});
        sec.entries.push_back({"brc101d", "East Brecilian Forest"});
        sec.entries.push_back({"brc204d", "Ruins Upper Level"});
        sec.entries.push_back({"brc202d", "Lower Ruins"});
        sec.entries.push_back({"brc203d", "Lair of the Werewolves"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Paragon of Her Kind";
        sec.entries.push_back({"orz900d", "The Dead Trenches"});
        sec.entries.push_back({"lgt300d", "Frostback Mountain Pass"});
        sec.entries.push_back({"orz000d", "Orzammar Hall of Heroes"});
        sec.entries.push_back({"orz100d", "Orzammar Commons"});
        sec.entries.push_back({"orz101d", "Tapster's Tavern"});
        sec.entries.push_back({"orz105d", "Figor's Imports"});
        sec.entries.push_back({"orz103d", "Carta Hideout"});
        sec.entries.push_back({"orz102d", "Janar Armorers"});
        sec.entries.push_back({"orz107d", "Orzammar Chantry"});
        sec.entries.push_back({"orz5003", "Orzammar Proving"});
        sec.entries.push_back({"orz300d", "Orzammar Diamond Quarter"});
        sec.entries.push_back({"orz304d", "Orzammar Shaperate"});
        sec.entries.push_back({"orz303d", "Orzammar Royal Palace"});
        sec.entries.push_back({"orz302d", "Harrowmant's Estate"});
        sec.entries.push_back({"orz301d", "Chamber of the Assembly"});
        sec.entries.push_back({"orz200d", "Dust Town"});
        sec.entries.push_back({"orz201d", "Dust Town Home"});
        sec.entries.push_back({"orz203d", "Alimar's Emporium"});
        sec.entries.push_back({"orz700d", "Caridin's Cross"});
        sec.entries.push_back({"orz701d", "Aeducan Thaig"});
        sec.entries.push_back({"orz702d", "Ortan Thaig"});
        sec.entries.push_back({"orz901d", "Anvil of the Void"});
        sec.entries.push_back({"orz703d", "Cadash Thaig"});
        sec.entries.push_back({"orz800d", "Deep Roads Outskirts"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Sacred Urn";
        sec.entries.push_back({"lak400d", "The Village of Haven"});
        sec.entries.push_back({"den308d", "Haven Chantry"});
        sec.entries.push_back({"orz106d", "Villager House"});
        sec.entries.push_back({"den009d", "Village Store"});
        sec.entries.push_back({"lak401d", "Ruined Temple"});
        sec.entries.push_back({"lak405d", "Caverns"});
        sec.entries.push_back({"lak403d", "Mountain Top"});
        sec.entries.push_back({"lak404d", "The Gauntlet"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Landsmeet (Denerim)";
        sec.entries.push_back({"den312d", "The Pearl"});
        sec.entries.push_back({"den505d", "Fort Drakon"});
        sec.entries.push_back({"den401d", "Arl Eamon's Estate"});
        sec.entries.push_back({"den012d", "Arl of Denerim's Estate - Exterior"});
        sec.entries.push_back({"den005d", "Arl of Denerim's Estate - Interior"});
        sec.entries.push_back({"den011d", "Arl of Denerim's Estate - Dungeon"});
        sec.entries.push_back({"den001d", "Landsmeet Chamber"});
        sec.entries.push_back({"den900d", "Back Alley"});
        sec.entries.push_back({"den901d", "Back Alley 2"});
        { LevelSubFolder sf; sf.name = "Elven Alienage";
          sf.entries.push_back({"den200d", "Elven Alienage"});
          sf.entries.push_back({"den201d", "House"});
          sf.entries.push_back({"den202d", "Alarith's Store"});
          sf.entries.push_back({"den207d", "Velandrian's Home"});
          sf.entries.push_back({"den204d", "Tevinter Hospice"});
          sf.entries.push_back({"den203d", "Run Down Apartments"});
          sf.entries.push_back({"den206d", "Tevinter Warehouse"});
          sec.subFolders.push_back(sf); }
        { LevelSubFolder sf; sf.name = "Side Content";
          sf.entries.push_back({"den600d", "Abandoned Orphanage"});
          sf.entries.push_back({"den601d", "Deserted Building"});
          sf.entries.push_back({"brc504d", "Kadan-Fe Hideout"});
          sf.entries.push_back({"den602d", "South Wing of Bann Franderel's Estate"});
          sf.entries.push_back({"den404d", "Quaint Hovel"});
          sf.entries.push_back({"den408d", "D's Hideout"});
          sec.subFolders.push_back(sf); }
        { LevelSubFolder sf; sf.name = "Market District";
          sf.entries.push_back({"den400d", "Denerim Market District"});
          sf.entries.push_back({"den401d", "Arl Eamon's Estate"});
          sf.entries.push_back({"den020d", "Gnawed Noble Tavern"});
          sf.entries.push_back({"den403d", "Wonder's of Thedas"});
          sf.entries.push_back({"den404d", "Goldanna's Home"});
          sf.entries.push_back({"den405d", "Marjolaine's Home"});
          sf.entries.push_back({"den998d", "Market Warehouse"});
          sf.entries.push_back({"den101d", "Genitivi's Home"});
          sf.entries.push_back({"den407d", "Smithy"});
          sec.subFolders.push_back(sf); }
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Climax";
        sec.entries.push_back({"lak100d", "Redcliffe Village"});
        sec.entries.push_back({"lak200d", "Redcliffe Castle"});
        sec.entries.push_back({"den500d", "Fort Drakon - Main Floor"});
        sec.entries.push_back({"den501d", "Forst Drakon - Second Floor"});
        sec.entries.push_back({"den502d", "Fort Drakon - Roof"});
        sec.entries.push_back({"lak201d", "Redcliffe Castle - Main Floor"});
        sec.entries.push_back({"lak202d", "Redcliffe Castle - Upper Floor"});
        sec.entries.push_back({"den510d", "Denerim - City Gates"});
        sec.entries.push_back({"den200n", "Denerim - Elven Alienage"});
        sec.entries.push_back({"den520d", "Denerim - Market District"});
        sec.entries.push_back({"den000d", "Denerim - Palace District"});
        sec.entries.push_back({"den504d", "Fort Drakon - Exterior"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Epilogue";
        sec.entries.push_back({"den001d", "Coronation"});
        sec.entries.push_back({"lak100d", "Player's Funeral"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Random Encounters";
        sec.entries.push_back({"lgt601d", "Roadside"});
        sec.entries.push_back({"lgt600d", "Abandoned Meadow"});
        sec.entries.push_back({"lgt603d", "Hillside Path"});
        sec.entries.push_back({"lgt602d", "Taoran's Camp"});
        sec.entries.push_back({"brc504d", "Wooden Glenn"});
        sec.entries.push_back({"brc501d", "Out of the Way"});
        sec.entries.push_back({"brc502d", "Twisted Forest"});
        sec.entries.push_back({"brc503d", "Dark Forest"});
        sec.entries.push_back({"brc505d", "River Crossing"});
        sec.entries.push_back({"lgt604d", "The Long Road"});
        sec.entries.push_back({"lgt605d", "The Crater"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Debug";
        sec.entries.push_back({"arena2", "Banter Test Area"});
        sec.entries.push_back({"arena", "Default Start Area"});
        sec.entries.push_back({"rmtst", "Head Morphs Test"});
        sec.entries.push_back({"combat2", "AI Test Area"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    { LevelGame game; game.name = "Dragon Age: Awakening";
      { LevelSection sec; sec.name = "Vigil's Keep";
        sec.entries.push_back({"vgk100d", "Vigil's Keep"});
        sec.entries.push_back({"vgk101d", "Magazines"});
        sec.entries.push_back({"vgk102d", "On the Wall"});
        sec.entries.push_back({"vgk200d", "Vigil's Keep - Interior"});
        sec.entries.push_back({"vgk210d", "Great Hall"});
        sec.entries.push_back({"vgk220d", "Nathanial's Prison"});
        sec.entries.push_back({"vgk310d", "Caverns"});
        sec.entries.push_back({"vgk320d", "Cavern Upper Floor"});
        sec.entries.push_back({"vgk330d", "Deep Roads"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Amaranthine";
        sec.entries.push_back({"amn100d", "Amaranthine City"});
        sec.entries.push_back({"amn110d", "Tavern"});
        sec.entries.push_back({"amn120d", "Smuggler's Lair"});
        sec.entries.push_back({"amn130d", "Warehouse"});
        sec.entries.push_back({"amn140d", "Chantry"});
        sec.entries.push_back({"amn150d", "House"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Blackmarsh";
        sec.entries.push_back({"stb000d", "Village"});
        sec.entries.push_back({"stb001d", "Fade Village"});
        sec.entries.push_back({"stb210d", "Fade Caverns"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Wending Woods";
        sec.entries.push_back({"trp100d", "Wending Woods"});
        sec.entries.push_back({"trp200d", "Silverite Mines"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Kal'Hirol";
        sec.entries.push_back({"ltl100d", "Deep Roads Entrance"});
        sec.entries.push_back({"ltl200d", "Kal'Hirol Entrance"});
        sec.entries.push_back({"ltl300d", "Kal'Hirol First Floor"});
        sec.entries.push_back({"ltl400d", "Lyrium Mines"});
        sec.entries.push_back({"ltl500d", "Brood Mother's Lair"});
        game.sections.push_back(sec); }
      { LevelSection sec; sec.name = "Dragonbone Wastes";
        sec.entries.push_back({"ltm100d", "Dragonbone Wastes"});
        sec.entries.push_back({"ltm200d", "Mother's Lair Entrance"});
        sec.entries.push_back({"ltm300d", "Mother's Lair"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    { LevelGame game; game.name = "The Stone Prisoner";
      { LevelSection sec; sec.name = "The Stone Prisoner";
        sec.entries.push_back({"shl000d", "Honnleath Village"});
        sec.entries.push_back({"shl100d", "Vygelm Dungeon"});
        sec.entries.push_back({"shl200d", "Kadash Thaig"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    { LevelGame game; game.name = "Return to Ostagar";
      { LevelSection sec; sec.name = "Return to Ostagar";
        sec.entries.push_back({"kcc000d", "Ostagar"});
        sec.entries.push_back({"kcc200d", "Tower of Ishal"});
        sec.entries.push_back({"kcc300d", "Caves Beneath Ostagar"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    { LevelGame game; game.name = "Leliana's Song";
      { LevelSection sec; sec.name = "Leliana's Song";
        sec.entries.push_back({"lel100n", "Denerim Market"});
        sec.entries.push_back({"lel200n", "Arl of Denerim's Estate - Exterior"});
        sec.entries.push_back({"lel210n", "Arl of Denerim's Estate - Interior"});
        sec.entries.push_back({"lel220n", "Arl of Denerim's Estate - Dungeon"});
        sec.entries.push_back({"lel300d", "Marjolaine's Safehouse"});
        sec.entries.push_back({"lel400d", "Chantry"});
        sec.entries.push_back({"lel500d", "Storm Coast Peak"});
        sec.entries.push_back({"lel510d", "Storm Coast"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    { LevelGame game; game.name = "Warden's Keep";
      { LevelSection sec; sec.name = "Warden's Keep";
        sec.entries.push_back({"gwb000d", "Warden's Keep"});
        sec.entries.push_back({"gwb100d", "Warden's Keep - First Floor"});
        sec.entries.push_back({"gwb110d", "Warden's Keep - Second Floor"});
        sec.entries.push_back({"gwb120d", "Warden's Keep - Upper Floor"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    { LevelGame game; game.name = "Golem's of Amgarrak";
      { LevelSection sec; sec.name = "Golem's of Amgarrak";
        sec.entries.push_back({"gib100d", "Amgarrak Thaig"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    { LevelGame game; game.name = "Witch Hunt";
      { LevelSection sec; sec.name = "Witch Hunt";
        sec.entries.push_back({"str400d", "Cadash Thaig"});
        game.sections.push_back(sec); }
      db.push_back(game); }
    return db;
}