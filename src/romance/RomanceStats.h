#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace romantasy
{
    // Stable IDs and weights are shared by file profiles and the legacy adapter.
    // fallbackLocalID is used only by legacy faction integration/category ordering.
    struct StatRule
    {
        std::string_view editorID;
        std::string_view label;
        std::int32_t points;
        std::uint32_t fallbackLocalID;
    };

    inline constexpr std::array<StatRule, 58> StatRules{ {
        { "ROM_LocationsDiscovered", "Locations Discovered", 2, 0x801 },
        { "ROM_DungeonsCleared", "Dungeons Cleared", 2, 0x802 },
        { "ROM_DaysPassed", "Days Passed", 2, 0x803 },
        { "ROM_StandingStonesFound", "Standing Stones Found", 2, 0x804 },
        { "ROM_ChestsLooted", "Chests Looted", 1, 0x805 },
        { "ROM_SkillIncreases", "Skill Increases", 2, 0x806 },
        { "ROM_SkillBooksRead", "Skill Books Read", 1, 0x807 },
        { "ROM_Barters", "Barters", 1, 0x808 },
        { "ROM_Persuasions", "Persuasions", 1, 0x809 },
        { "ROM_Bribes", "Bribes", 1, 0x80A },
        { "ROM_Intimidations", "Intimidations", 1, 0x80B },
        { "ROM_DiseasesContracted", "Diseases Contracted", 1, 0x80C },
        { "ROM_DaysVampire", "Days as a Vampire", 2, 0x80D },
        { "ROM_DaysWerewolf", "Days as a Werewolf", 2, 0x80E },
        { "ROM_NecksBitten", "Necks Bitten", 2, 0x80F },
        { "ROM_VampirismCures", "Vampirism Cures", 2, 0x810 },
        { "ROM_WerewolfTransformations", "Werewolf Transformations", 5, 0x811 },
        { "ROM_Mauls", "Mauls", 2, 0x812 },
        { "ROM_QuestsCompleted", "Quests Completed", 3, 0x813 },
        { "ROM_MiscObjectivesCompleted", "Misc Objectives Completed", 3, 0x814 },
        { "ROM_MainQuestsCompleted", "Main Quests Completed", 5, 0x815 },
        { "ROM_SideQuestsCompleted", "Side Quests Completed", 5, 0x816 },
        { "ROM_CompanionsCompleted", "The Companions Quests Completed", 10, 0x817 },
        { "ROM_CollegeCompleted", "College of Winterhold Quests Completed", 10, 0x818 },
        { "ROM_ThievesCompleted", "Thieves' Guild Quests Completed", 10, 0x819 },
        { "ROM_DarkBrotherhoodCompleted", "The Dark Brotherhood Quests Completed", 10, 0x81A },
        { "ROM_CivilWarCompleted", "Civil War Quests Completed", 10, 0x81B },
        { "ROM_DaedricCompleted", "Daedric Quests Completed", 10, 0x81C },
        { "ROM_DawnguardCompleted", "Dawnguard Quests Completed", 10, 0x81D },
        { "ROM_DragonbornCompleted", "Dragonborn Quests Completed", 10, 0x81E },
        { "ROM_QuestlinesCompleted", "Questlines Completed", 20, 0x81F },
        { "ROM_PeopleKilled", "People Killed", 1, 0x820 },
        { "ROM_AnimalsKilled", "Animals Killed", 1, 0x821 },
        { "ROM_CreaturesKilled", "Creatures Killed", 1, 0x822 },
        { "ROM_UndeadKilled", "Undead Killed", 1, 0x823 },
        { "ROM_DaedraKilled", "Daedra Killed", 1, 0x824 },
        { "ROM_AutomatonsKilled", "Automatons Killed", 1, 0x825 },
        { "ROM_CriticalStrikes", "Critical Strikes", 1, 0x826 },
        { "ROM_SneakAttacks", "Sneak Attacks", 1, 0x827 },
        { "ROM_Backstabs", "Backstabs", 1, 0x828 },
        { "ROM_WeaponsDisarmed", "Weapons Disarmed", 1, 0x829 },
        { "ROM_BunniesSlaughtered", "Bunnies Slaughtered", 1, 0x82A },
        { "ROM_SpellsLearned", "Spells Learned", 2, 0x82B },
        { "ROM_DragonSoulsCollected", "Dragon Souls Collected", 2, 0x82C },
        { "ROM_ShoutsLearned", "Shouts Learned", 2, 0x82D },
        { "ROM_SoulsTrapped", "Souls Trapped", 1, 0x82E },
        { "ROM_MagicItemsMade", "Magic Items Made", 1, 0x82F },
        { "ROM_WeaponsMade", "Weapons Made", 1, 0x830 },
        { "ROM_ArmorMade", "Armor Made", 1, 0x831 },
        { "ROM_PotionsMixed", "Potions Mixed", 1, 0x832 },
        { "ROM_PoisonsMixed", "Poisons Mixed", 1, 0x833 },
        { "ROM_LocksPicked", "Locks Picked", 1, 0x834 },
        { "ROM_PocketsPicked", "Pockets Picked", 1, 0x835 },
        { "ROM_ItemsStolen", "Items Stolen", 1, 0x836 },
        { "ROM_Assaults", "Assaults", 2, 0x837 },
        { "ROM_Murders", "Murders", 5, 0x838 },
        { "ROM_HorsesStolen", "Horses Stolen", 2, 0x839 },
        { "ROM_Trespasses", "Trespasses", 2, 0x83A },
    } };
}
