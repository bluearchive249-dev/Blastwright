// SoH: "Play as Ganondorf", the Poe minion's words.
//
// Navi is replaced by a Poe minion of Ganondorf's (drawn in z_en_elf.c). Her advice (C-Up) and her remarks in
// dungeons say the same things Navi's did, so the hints still work, but in the voice of a fawning servant of the King
// of Evil who calls him Master. Only used while the Poe minion option is on.

#include <string>
#include <unordered_map>

#include "soh/Enhancements/game-interactor/GameInteractor.h"
#include "soh/Enhancements/custom-message/CustomMessageManager.h"
#include "soh/ShipInit.hpp"

extern "C" {
#include "z64.h"
#include "macros.h"
#include "functions.h"
#include "variables.h"
#include "PlayAsGanondorf.h"
}

// & starts a new line, ^ a new text box
static const std::unordered_map<uint16_t, const char*> sMinionLines = {
    // Remarks in dungeons and fields
    { 0x0100, "Master, what is that thing? Shall I fetch it for you?" },
    { 0x0101, "Look down, Master! Something lurks beneath this web... Perhaps your royal weight could break "
              "right through it?" },
    { 0x0102, "Vines grow all over this wall, Master. Their rough surface could be climbed... even by a king, if "
              "the throne of the world lies at the top!" },
    { 0x0103, "Doors bow before you, Master. Simply stand in front of one and press A.^Watch the Action Icon, the "
              "blue one at the top of the screen. It tells you what your Majesty may do." },
    { 0x0104, "Master! An old ladder hangs up there. Bring it down and the way is yours!" },
    { 0x0105, "Master... isn't that the same crest as the one on the Door of Time?" },
    { 0x0106, "That torch was burning not long ago, Master. Someone has been here... hehehe." },
    { 0x0107, "Narrow passages ahead, Master. Move slowly and your prey won't hear you coming.^Use Z Targeting to "
              "keep your eyes on your victims at all times!" },
    { 0x0108, "Stand beside this block and seize it with A, Master. Hold A to push or pull it.^Stand next to it and "
              "press A while moving to climb on top. Nothing stands in your way for long!" },
    { 0x010C, "Once you're in the water, hold A to dive, Master! There must be treasure down there worth stealing." },
    { 0x0114, "So many Bomb Flowers, Master!^Is there a way to set them all off at once? Oh, what glorious "
              "destruction that would be!" },
    { 0x0115, "Lava pits everywhere, Master. Mind your step... not that a little fire could ever trouble you." },
    { 0x0116, "With that switch pressed, the moving platform rises higher. Now you can reach the second floor, "
              "Master!" },
    { 0x0119, "Who knows what waits around the corner in these narrow paths, Master...^Use Z Targeting to always "
              "face your enemies. Let them see who is coming for them!" },
    { 0x011F, "Master, what are you staring at?" },
    { 0x0124, "The face of the Desert Colossus... It looks almost as evil as you, Master. Almost." },
    { 0x0126, "The spirits are whispering in this room, Master...^\"Look for the eye of truth...\" That's what they "
              "keep saying!" },
    { 0x0128, "The spirits whisper here too, Master...^\"Those with sacred feet should let the wind guide them, and "
              "it will lead them to the hidden path.\"" },
    { 0x0129, "Something is written on this wall, Master...&\"Danger above...\"" },
    { 0x012A, "Something is written on this wall, Master...&\"Danger below...\"" },
    { 0x012B, "The water pouring out of this statue is flooding the whole floor, Master." },
    { 0x012F, "Careful, Master! Lightning is crackling through that green slimy thing!" },
    { 0x0131, "Careful, Master! Lightning is crackling through that red slimy thing!" },
    { 0x0132, "Careful, Master! Lightning is crackling through that blue slimy thing!" },
    { 0x0133, "This switch won't go down under your weight alone, Master... and you are not a small man." },
    { 0x0137, "The red slimy thing is gone, Master! Cutting its red tail did it. Perhaps the others fall the same "
              "way?" },
    { 0x0139, "There's a switch on the other side of this wall, Master!" },
    { 0x013A, "Something is waiting on top of that platform, Master!" },
    { 0x013D, "BWAHAHA! Look at all those flags, Master!^Can you tell which ones are real?" },

    // Advice (C-Up)
    { 0x0140, "The Great Deku Tree is calling for you, Master. Let's go and see what the old stump wants." },
    { 0x0141, "Onward, Master! Into the Great Deku Tree! Its insides are yours for the taking." },
    { 0x0142, "The Great Deku Tree wanted us to visit the princess at Hyrule Castle, Master. Shall we pay her a "
              "royal visit?" },
    { 0x0143, "The girl from the ranch asked us to find her father, Master. I wonder where that lazy oaf is "
              "hiding?" },
    { 0x0144, "Where could the princess be hiding in this big old castle, Master?" },
    { 0x0145, "What would the forest girl Saria say if she knew who you really are, Master? Hehehe..." },
    { 0x0146, "Impa said the Spiritual Stone of Fire is somewhere on Death Mountain, Master." },
    { 0x0147, "Use a Bomb Flower to blast your way into Dodongo's Cavern, Master!" },
    { 0x0148, "Didn't Darunia say a fairy lives at the top of Death Mountain, Master?" },
    { 0x0149, "Perhaps Saria knows something about the last Spiritual Stone, Master?" },
    { 0x014A, "Princess Ruto somehow ended up inside Jabu-Jabu's belly, Master. How careless of her." },
    { 0x014B, "All three Spiritual Stones are yours, Master! To Hyrule Castle, at once!" },
    { 0x014C, "Those two on the white horse... Zelda and Impa, fleeing from you, Master!^They threw something into "
              "the moat!" },
    { 0x014D, "Let's take a look inside the Temple of Time, Master." },
    { 0x014E, "Should we trust what Sheik said and go to Kakariko Village, Master?" },
    { 0x0150, "Something is happening in the forest, Master... and the forest girl is there too." },
    { 0x0151, "That cloud over Death Mountain, Master... there is something strange about it." },
    { 0x0152, "An icy wind is blowing from Zora's River, Master. Do you feel it?" },
    { 0x0153, "Those Iron Boots weigh a ton, Master! With them on, you could walk along the bottom of a lake." },
    { 0x0154, "We should find someone who knows about the other Sages, Master. Then we'll know where they're "
              "hiding." },
    { 0x0155, "That monster crawled out of the well in the village, Master! Let's take a look inside the well." },
    { 0x0156, "Who built the Spirit Temple, Master, and for what? Your own people, perhaps..." },
    { 0x0157, "Have you played the Nocturne of Shadow that Sheik taught you, Master?" },
    { 0x0158, "The desert... that is where you were born, Master. If we go there, we might find something." },
    { 0x015A, "Put on the Silver Gauntlets, Master, and lift what you couldn't budge before!" },
    { 0x015B, "The one waiting for us at the Temple of Time, Master... could it be...?" },
    { 0x015C, "Princess Zelda is locked away in your castle, Master!^...Your castle. Hehehe. Let's go home!" },
    { 0x015F, "...&Keep moving, Master!!" },

    // Remarks in the later dungeons
    { 0x0180, "I hear a voice from somewhere, Master...^It says: \"Collect five silver Rupees...\"" },
    { 0x0181, "This wall is saying something, Master!^It says: \"If you want to see a ferry to the other world, "
              "come here...\"" },
    { 0x0183, "Careful on that boat, Master! It looks ancient. Who knows when it might sink?" },
    { 0x0184, "There's a door over there, Master... Is there any way across?" },
    { 0x0186, "That red ice... how strange, Master!" },
    { 0x0189, "This blue fire isn't natural, Master. Maybe you can use it for something?" },
    { 0x018C, "The fires on the torches are gone, Master. The ghosts took them away!&...My sisters, perhaps." },
    { 0x018D, "Look, Master! A torch is lit! That's because you beat a ghost, isn't it?&...Sorry, sister." },
    { 0x018F, "There are arrows painted on the floor, Master!" },
    { 0x0190, "This whole corridor is twisted, Master!" },
    { 0x0191, "Watch for the shadows of monsters hanging from the ceiling, Master." },
    { 0x0192, "A treasure chest, Master! Plunder it!" },
    { 0x0194, "Isn't this the same torch we saw at the entrance to the temple, Master?" },
    { 0x0195, "This torch is lit, Master... that means..." },
    { 0x0197, "This switch is frozen solid, Master!" },
    { 0x0198, "Master, look out! The ceiling is coming down!" },
    { 0x01A3, "Master, I hear Goron voices down below." },
    { 0x01A5, "You can see down from here, Master... Isn't that the room where we saw Darunia?" },
    { 0x01A7, "This statue... haven't we seen it somewhere before, Master?" },
    { 0x01A9, "This switch looks rusted, Master." },
    { 0x01AB, "Master! Be careful! Don't get swallowed by those whirlpools!" },
};

// The opening, rewritten: the Deku Tree banishes the Poe from his forest for haunting it, and she seeks out the
// dark power she senses in the Kokiri village... Ganondorf, "sleeping" in Link's house.
struct MinionScene {
    const char* text;
    TextBoxType type;
    TextBoxPosition position;
};

static const std::unordered_map<uint16_t, MinionScene> sMinionOpening = {
    // The Deku Tree calls out (no box)
    { 0x1099, { "Poe...&Wretched little Poe, where art thou?&Come hither....", TEXTBOX_TYPE_NONE_NO_SHADOW,
                TEXTBOX_POS_MIDDLE } },
    // ...and passes judgement
    { 0x109A, { "So, Poe... thou hast come at last.&Hear the words of the Deku Tree...^"
                "For far too long hast thou haunted this sacred grove... Thy lantern flickers in the night and "
                "frightens the Kokiri children.^"
                "Thou dost moan in the treetops, snuff out the fairies' light, and play thy wicked tricks upon "
                "the lost...^"
                "A climate of evil is descending upon this realm... I will not have a creature of darkness lurking "
                "within my forest.^"
                "The Kokiri Forest is the source of life. It shall be no home to the dead.^"
                "Therefore, by my word... thou art banished from this forest, never to return!^"
                "Begone, Poe! Begone! Go and haunt some other land!",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM } },
    // Link's house: she finds him asleep
    { 0x1095, { "Psst... M-Master? Are you awake?&Please don't smite me!^"
                "I followed a dark aura all the way here... so powerful, so wonderfully evil... and it led me "
                "straight to you!",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_TOP } },
    { 0x1096, { "Master, please wake up! I've come to serve you!^"
                "That old stump threw me out of his forest... but I would much rather serve the King of Evil! "
                "Will you have me?",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_TOP } },
    // He gets up
    { 0x1000, { "You're awake! I'm a humble Poe, Master, and your loyal servant from this day on!^"
                "The Great Deku Tree banished me for haunting his grove... His loss! I'll follow you anywhere and "
                "tell you everything I see. Hehehe!",
                TEXTBOX_TYPE_BLUE, TEXTBOX_POS_VARIABLE } },
    { 0x1098, { "That old Deku Tree has summoned you, Master!&Let's go and see what the withered stump wants.",
                TEXTBOX_TYPE_BLUE, TEXTBOX_POS_VARIABLE } },
};

static void BuildMinionMessage(uint16_t* textId, bool* loadFromMessageTable) {
    if (!PlayAsGanondorf_UsePoeNavi()) {
        return;
    }
    auto scene = sMinionOpening.find(*textId);
    if (scene != sMinionOpening.end()) {
        CustomMessage msg = CustomMessage(scene->second.text);
        msg.SetTextBoxType(scene->second.type);
        msg.SetTextBoxPosition(scene->second.position);
        *loadFromMessageTable = false;
        msg.AutoFormat();
        msg.LoadIntoFont();
        return;
    }
    auto it = sMinionLines.find(*textId);
    if (it == sMinionLines.end()) {
        return;
    }
    CustomMessage msg = CustomMessage(it->second);
    msg.SetTextBoxType(TEXTBOX_TYPE_BLUE);
    msg.SetTextBoxPosition(TEXTBOX_POS_BOTTOM);
    *loadFromMessageTable = false;
    msg.AutoFormat();
    msg.LoadIntoFont();
}

static void RegisterGanondorfMinion() {
    GameInteractor::Instance->RegisterGameHook<GameInteractor::OnOpenText>(BuildMinionMessage);
}

static RegisterShipInitFunc initFunc(RegisterGanondorfMinion, {});
