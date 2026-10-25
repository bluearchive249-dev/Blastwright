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
    // Her notes on enemies, when he targets them (same advice)
    { 0x0600, "What's that, Master? Shall I find out?" },
    { 0x0601, "%rGohma%w&A parasite living inside the Deku Tree, Master. Strike its eye when it turns red!" },
    { 0x0602, "%rGohma Egg%w&A Gohma Larva will hatch from this egg, Master. Crush it first!" },
    { 0x0603, "%rGohma Larva%w&Careful when it gets ready to pounce, Master!" },
    { 0x0604, "%rSkulltula%w&Its soft belly is its weak point, Master!" },
    { 0x0605, "%rBig Skulltula%w&Its soft belly is its weak point, Master!" },
    { 0x0606, "%rTailpasaran%w&Its tail is its weak point, Master!" },
    { 0x0607, "%rDeku Baba%w&Strike it as it lunges and it will stand upright, Master. Cut it quickly for a Deku Stick!" },
    { 0x0608, "%rBig Deku Baba%w&Strike it as it lunges and it will stand upright, Master. Cut it quickly for a Deku Stick!" },
    { 0x0609, "%rDeku Baba%w&It looks withered, Master, but it will still hurt you if you touch it!" },
    { 0x060A, "%rDeku Scrub%w&It hides in the grass when you come close, Master. Send the nuts it spits right back at it!" },
    { 0x060C, "%rKing Dodongo%w&A huge Dodongo that eats anything, Master! Give it a shock, then finish it off!" },
    { 0x060D, "%rDodongo%w&Watch out for its fiery breath, Master! Use Z Targeting!" },
    { 0x060E, "%rBaby Dodongo%w&Careful of its leaping attack, Master! It explodes when it's defeated!" },
    { 0x060F, "%rLizalfos%w&A clever fighter, Master. Guard well and use Z Targeting!" },
    { 0x0610, "%rDinolfos%w&A clever fighter, Master. Guard well and use Z Targeting!" },
    { 0x0611, "%rFire Keese%w&Destroy it before it flies into you, Master! Its flames burn anything they touch!" },
    { 0x0612, "%rKeese%w&Use Z Targeting when it comes close, Master. You can keep it in your sights even as it flies away." },
    { 0x0613, "%rArmos%w&Stop it moving, Master, then destroy it!" },
    { 0x0614, "%rBarinade%w&A swarm of parasitic jellyfish guards this monster's body, Master. Aim for the body!" },
    { 0x0615, "%rParasitic Tentacle?%w&The narrow part seems to be its weak point, Master... It would take a particular item to destroy it." },
    { 0x0616, "%rShabom%w&Blades bounce right off it, Master!" },
    { 0x0617, "%rBiri%w&Touch it and you'll be electrocuted, Master!" },
    { 0x0618, "%rBari%w&Touch it and you'll be electrocuted, Master!" },
    { 0x0619, "%rStinger%w&Destroy it before it flies into you, Master!" },
    { 0x061A, "%rPhantom Ganon%w&The evil ghost of the Forest Temple... wearing YOUR face, Master! How dare he!^He rides out of one of the paintings. Answer his magic with your own!" },
    { 0x061B, "%rStalfos%w&Lure it close and watch it carefully, Master! Strike when it drops its guard!" },
    { 0x061C, "%rBlue Bubble%w&Guard against its blue flame, Master!" },
    { 0x061D, "%rWhite Bubble%w&Strike when it stops moving, Master!" },
    { 0x061E, "%rGreen Bubble%w&Strike when its green fire goes out, Master!" },
    { 0x061F, "%rSkullwalltula%w&Careful not to touch it, Master!" },
    { 0x0620, "%rGold Skulltula%w&A Spider of the Curse, Master! Defeat it and you'll get a token as proof!" },
    { 0x0621, "%rVolvagia%w&The Boss of the Fire Temple... revived by you, Master? I don't know its weak point..." },
    { 0x0622, "%rFlare Dancer%w&Put out its flaming clothes first, Master..." },
    { 0x0623, "%rTorch Slug%w&When the fire on its back goes out, it runs away, Master. Destroy it before it lights up again!" },
    { 0x0624, "%rRed Bubble%w&Guard against its attack, Master!" },
    { 0x0625, "%rMorpha%w&The master of the Water Temple, Master. It controls the water itself! Pull out its nucleus and attack it!" },
    { 0x0626, "%rDark @%w&Conquer yourself, Master! ...If anyone can, it's you." },
    { 0x0627, "%rShell Blade%w&The muscle inside its shell is its weak point, Master!" },
    { 0x0628, "%rSpike%w&Strike when it pulls in its spikes, Master!" },
    { 0x0629, "%rBongo Bongo%w&The Boss of the Shadow Temple, risen from the well, Master. Look at it with the eye of truth!" },
    { 0x062A, "%rReDead%w&Its gaze paralyzes, Master! If it grabs you, tap any button to break free!" },
    { 0x062B, "%rPhantom Ganon%w&Strike him when he comes out of one of the paintings, Master! Watch out for the fake!" },
    { 0x062D, "%rGibdo%w&Its gaze paralyzes, Master! If it grabs you, tap any button to break free!" },
    { 0x062E, "%rDead Hand's Hands%w&If it grabs you, tap any button to break free, Master!" },
    { 0x062F, "%rDead Hand%w&It has hands without end, Master! Aim for its head!" },
    { 0x0630, "%rWallmaster%w&Watch for its shadow on the floor, Master! Destroy it before it goes back up to the ceiling!" },
    { 0x0631, "%rFloormaster%w&When it splits, destroy every piece before they come back together, Master!" },
    { 0x0632, "%rTwinrova Koume%w&Your own surrogate mother, Master! The Sorceress of Flame. Cold seems to be her weakness..." },
    { 0x0633, "%rTwinrova Kotake%w&Your own surrogate mother, Master! The Sorceress of Ice. Heat seems to be her weakness..." },
    { 0x0634, "%rIron Knuckle%w&Something's strange, Master... This is no ordinary enemy!" },
    { 0x0635, "%rIron Knuckle%w&Beware its axe, Master! It hits hard! Strike when it drops its guard!" },
    { 0x0636, "%rSkull Kid%w&So this is what becomes of children lost in the forest, Master. He doesn't like grownups." },
    { 0x0637, "%rLike Like%w&It eats shields and certain clothes, Master. Beat it quickly to get your things back!" },
    { 0x0639, "%rBeamos%w&Watch out for its searching beam, Master! I bet it hates smoke in its eye!" },
    { 0x063A, "%rAnubis%w&Fire is its weakness, Master!" },
    { 0x063B, "%rFreezzard%w&Watch out for its freezing breath, Master! Destroy it completely before it revives." },
    { 0x063D, "%rGanondorf the Evil King%w&M-Master?! There are two of you? Oh, this is wonderful and terrifying!" },
    { 0x063E, "%rGanon%w&Master... is that you? I really, really don't know its weak point!" },
    { 0x063F, "%rSkull Kid%w&So this is what becomes of children lost in the forest, Master. He might be friendly if we do something for him..." },
    { 0x0640, "%rSkull Kid%w&So this is what becomes of children lost in this forest, Master. He seems unhappy to have no face..." },
    { 0x0641, "%rSkull Kid%w&So this is what becomes of children lost in this forest, Master. He looks happy with his skull face." },
    { 0x0642, "%rOctorok%w&Send the rocks it spits right back at it, Master!" },
    { 0x0643, "%rPoe of the graveyard?%w&A cousin of mine, Master! If you stare at it with Z Targeting, it will vanish..." },
    { 0x0644, "%rPoe%w&A cousin of mine, Master! If you stare at it with Z Targeting, it will vanish..." },
    { 0x0645, "%rRed Tektite%w&Lock on with Z Targeting as it jumps, Master!" },
    { 0x0646, "%rBlue Tektite%w&It's hard to beat on the water, Master. Lure it onto land!" },
    { 0x0647, "%rLeever%w&Watch it closely and let it pass, Master!" },
    { 0x0648, "%rPeahat%w&Its roots are its weak point, Master!" },
    { 0x0649, "%rPeahat Larva%w&Guard yourself, Master!" },
    { 0x064C, "%rWolfos%w&Draw it close and watch it carefully, Master! Strike when it drops its guard!" },
    { 0x064D, "%rMad Scrub%w&It hides in the grass when you come close, Master..." },
    { 0x064E, "%rBusiness Scrub%w&It hides in the grass when you come close, Master..." },
    { 0x064F, "%rDampe's Ghost%w&The ghost of the old gravekeeper, Master! Chase him and he might give you his keepsake..." },
    { 0x0650, "%rMeg - one of the Poe sisters%w&My eldest sister, Master! When she splits into copies, find the real one... is it this one?" },
    { 0x0651, "%rJoelle - one of the Poe sisters%w&My sister, Master! Strike her when she appears... sorry, Joelle!" },
    { 0x0652, "%rBeth - one of the Poe sisters%w&My sister, Master! Strike her when she appears... sorry, Beth!" },
    { 0x0653, "%rAmy - one of the Poe sisters%w&My little sister, Master! Strike her when she appears... sorry, Amy!" },
    { 0x0654, "%rGerudo Thief%w&One of your own people, Master! Strike when she drops her guard, or she'll capture you!" },
    { 0x0655, "%rStalchild%w&Nothing to fear, Master! Just keep hitting it!" },
    { 0x0656, "%rIce Keese%w&Destroy it before it flies into you, Master!" },
    { 0x0657, "%rWhite Wolfos%w&Lure it close and watch it carefully, Master! Strike when it drops its guard!" },
    { 0x0658, "%rGuay%w&Destroy it before it flies into you, Master!" },
    { 0x0659, "%rBigocto%w&Attack it from behind, Master!" },
    { 0x065A, "%rBig Poe%w&This big Poe won't vanish when you stare at it, Master. And it's fast for its size!" },
    { 0x065B, "%rTwinrova%w&Kotake and Koume combined, Master! Turn their magic back on them!" },
    { 0x065C, "%rPoe%w&This Poe won't vanish when you stare at it, Master. It doesn't spook easily..." },
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
    // The Kokiri village and the Deku Tree, now that she's with him
    { 0x109E, { "The children of the forest, the Kokiri, live here with me. Each Kokiri has his or her own guardian "
                "fairy.", TEXTBOX_TYPE_NONE_BOTTOM, TEXTBOX_POS_VARIABLE } },
    { 0x109F, { "And lately... a lost soul has been haunting my grove...", TEXTBOX_TYPE_NONE_BOTTOM,
                TEXTBOX_POS_VARIABLE } },
    { 0x1002, { "Eek! Is that a ghost?!^A Poe is following you around, @! Where is your fairy?", TEXTBOX_TYPE_BLACK,
                TEXTBOX_POS_VARIABLE } },
    { 0x1005, { "Oh, you have a... a Poe now?! That's... different, @!^What?^You've been called by the Great Deku "
                "Tree? What an honor!^He may give you a special gift! Tee hee!^That's because the Great Deku Tree is "
                "our father, the forest guardian, and he gave life to all of us Kokiri!",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_VARIABLE } },
    { 0x100C, { "Yes, yes! That's how you use your... um... Poe!^It's so great that you finally have a partner, even "
                "a spooky one!^I'll teach you how to talk to people using her!", TEXTBOX_TYPE_BLACK,
                TEXTBOX_POS_BOTTOM } },
    { 0x100D, { "When your Poe floats near a person or thing, press Z to look in that direction.^If you use Z "
                "Targeting, you can talk to people from a distance, like we're doing now.^When there's nothing to "
                "target, you can press Z just to look forward. Try it!", TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM } },
    { 0x100E, { "Oh, a Poe came to you instead of a fairy?!^Now you have a lot to learn!", TEXTBOX_TYPE_BLACK,
                TEXTBOX_POS_VARIABLE } },
    { 0x105F, { "Why is that Poe following you around? You're not one of us!", TEXTBOX_TYPE_BLACK,
                TEXTBOX_POS_VARIABLE } },
    { 0x10D0, { "What?! You've got a... a ghost?!&Say what? The Great Deku Tree actually summoned you?",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_VARIABLE } },
    { 0x1015, { "Poe... I banished thee, yet thou hast returned...^And thou hast brought... this one.^"
                "I sense a power in thee, stranger... a dark power, vast and terrible...^"
                "Yet a curse hath been placed upon me, and I am too weak to break it alone...^"
                "A wicked creature dwells within me, sent by a man of the desert... Hmm... thou dost look "
                "strangely like him...^No matter. The time has come to test thy might...",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM } },
    { 0x1017, { "Then enter, dark one, and thou too, Poe...^Poe... Thou must aid this stranger, though it pains me "
                "to say it...^And thou... when the Poe speaks, use C-Up to hear her counsel...",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM } },
    { 0x102A, { "The future depends upon thee, stranger... Thou art... powerful...^Poe... Help him to carry out... "
                "my will... I pray I have not erred...^I entreat ye... Poe... Good...bye...",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM } },
    { 0x102C, { "Hmph... Good-bye, old stump...", TEXTBOX_TYPE_BLUE, TEXTBOX_POS_BOTTOM } },
    { 0x103F, { "If you press C-Up, you can change your view.^In a place like this, it switches to a top-down view. "
                "Outdoors, in a field for example, it switches to a first-person view.^Also, when \"Navi\" shows in "
                "the upper right of your screen, your Poe wants to talk to you. Use C-Up to hear her out!",
                TEXTBOX_TYPE_BLACK, TEXTBOX_POS_BOTTOM } },
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
