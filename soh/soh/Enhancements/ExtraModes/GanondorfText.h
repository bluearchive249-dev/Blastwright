// Copyright (c) 2026 Grimsfrost. All rights reserved. No copying, editing or additions, by people or AI tools, without the author's written consent. See AI-POLICY.md.
// Play as Ganondorf: fitting his dialogue to the text box. Each box holds four lines; text that runs longer would
// spill over into the next box mid-sentence (often a single word left alone, and the speaker's name lost). Here the
// text is laid out by whole sentences instead: as many as fit in a box, the rest in the next box, which begins with
// the speaker's name again. A sentence too long for a box is split at its commas, or failing that, between words.
//
// Two forms of text:
// - GdfFitBoxes: text for CustomMessage::AutoFormat ('&' a new line, '^' a new box, "%r" style colours, '@' the
//   player's name), laid out with explicit '&' so AutoFormat adds no breaks of its own. The name, when a box starts
//   with one ("%rCaptain:%w ..."), stays on the first line.
// - GdfFitBoxesRaw: text in the game's own codes ('\x01' a new line, '\x04' a new box, '\x05' colours), for his
//   lines in the NPC conversations, where the name ("Ganondorf:") stands on a line of its own. Their old line breaks
//   are dropped and the lines laid out again.
#pragma once

#include <cstring>
#include <string>
#include <vector>

namespace GdfText {

// The game's font widths, in pixels (as CustomMessageManager.cpp measures them)
inline int CharWidth(unsigned char c) {
    static const unsigned char kWidths[95] = {
        6, 6, 5, 7, 7, 11, 9, 3, 6, 6, 6, 7, 3, 5, 3, 7, 8, 4, 7, 7, 8, 7, 7, 7, 8, 7, 5, 5, 7, 9, 7, 9,
        10, 9, 7, 9, 9, 6, 6, 9, 8, 3, 6, 8, 6, 10, 9, 10, 7, 10, 8, 8, 7, 8, 9, 12, 9, 8, 8, 6, 8, 6, 8, 7,
        4, 6, 7, 6, 7, 7, 5, 7, 6, 3, 5, 6, 3, 9, 7, 7, 7, 7, 6, 6, 6, 6, 7, 9, 6, 7, 6, 6, 4, 6, 8,
    };
    if ((c >= 0x20) && (c <= 0x7E)) {
        return kWidths[c - 0x20];
    }
    return (c >= 0x80) ? 5 : 0; // (a UTF-8 byte: a letter counted over its two bytes; other codes take no room)
}

constexpr int kLineWidth = 206; // (the box takes 216; a little kept back so AutoFormat never breaks again)
constexpr int kBoxLines = 4;

struct Style {
    char newline;  // '&' or '\x01'
    char box;      // '^' or '\x04'
    bool raw;      // game codes ('\x05' colours) rather than '%' colours
    bool nameLine; // the speaker's name stands on a line of its own
};

// The width of a word, its colour codes taking no room
inline int Width(const std::string& s, const Style& st) {
    int w = 0;
    for (size_t i = 0; i < s.size(); i++) {
        unsigned char c = (unsigned char)s[i];
        if (!st.raw && (c == '%') && (i + 1 < s.size())) {
            i++;
        } else if (st.raw && (c == 0x05) && (i + 1 < s.size())) {
            i++;
        } else if (!st.raw && (c == '@')) {
            w += 96; // (the player's name at its widest)
        } else {
            w += CharWidth(c);
        }
    }
    return w;
}

// Splits text into words, keeping hard line breaks as their own words
inline std::vector<std::string> Words(const std::string& text, const Style& st) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == ' ') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else if (c == st.newline) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            out.push_back(std::string(1, st.newline));
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

// Lays words out in lines no wider than the box (the first line starts after `lead`, the name when it's inline)
inline std::vector<std::string> Wrap(const std::vector<std::string>& words, int lead, const Style& st) {
    std::vector<std::string> lines(1);
    int w = lead;
    bool empty = true; // (nothing on this line yet)
    for (const std::string& word : words) {
        if ((word.size() == 1) && (word[0] == st.newline)) {
            lines.emplace_back();
            w = 0;
            empty = true;
            continue;
        }
        int ww = Width(word, st);
        if (empty && ((w + ww <= kLineWidth) || (w == 0))) {
            lines.back() += word;
            w += ww;
        } else if (!empty && (w + CharWidth(' ') + ww <= kLineWidth)) {
            lines.back() += " " + word;
            w += CharWidth(' ') + ww;
        } else {
            lines.push_back(word);
            w = ww;
        }
        empty = false;
    }
    return lines;
}

// Splits text into pieces that end a sentence (". ", "! ", "? ", also after a closing quote or bracket)
inline std::vector<std::string> Sentences(const std::string& text, const char* enders) {
    std::vector<std::string> out;
    size_t start = 0;
    for (size_t i = 0; i < text.size(); i++) {
        if (strchr(enders, text[i]) == nullptr) {
            continue;
        }
        size_t j = i + 1;
        while ((j < text.size()) && (strchr(enders, text[j]) != nullptr || text[j] == '"' || text[j] == '\'' ||
                                     text[j] == ')')) {
            j++;
        }
        if ((j < text.size()) && (text[j] == ' ')) {
            out.push_back(text.substr(start, j - start));
            start = j + 1;
            i = j;
        }
    }
    if (start < text.size()) {
        out.push_back(text.substr(start));
    }
    return out;
}

inline std::string Join(const std::vector<std::string>& parts) {
    std::string s;
    for (const std::string& p : parts) {
        if (!s.empty()) {
            s += ' ';
        }
        s += p;
    }
    return s;
}

// Lays one box's body out in as many boxes as it needs, each starting with `name`
inline std::vector<std::string> FitBody(const std::string& name, const std::string& body, const Style& st) {
    const int lead = st.nameLine ? 0 : Width(name, st);
    const int maxLines = st.nameLine && !name.empty() ? kBoxLines - 1 : kBoxLines;
    auto lineCount = [&](const std::string& s) { return (int)Wrap(Words(s, st), lead, st).size(); };
    auto render = [&](const std::string& s) {
        std::vector<std::string> lines = Wrap(Words(s, st), lead, st);
        std::string out = name;
        for (size_t i = 0; i < lines.size(); i++) {
            if (i > 0) {
                out += st.newline;
            }
            out += lines[i];
        }
        return out;
    };

    // The pieces to fill boxes with: sentences, and any sentence too long for a box in clauses, then in words
    std::vector<std::string> pieces;
    for (const std::string& sentence : Sentences(body, ".!?")) {
        if (lineCount(sentence) <= maxLines) {
            pieces.push_back(sentence);
            continue;
        }
        for (const std::string& clause : Sentences(sentence, ",;:")) {
            if (lineCount(clause) <= maxLines) {
                pieces.push_back(clause);
                continue;
            }
            std::vector<std::string> words = Words(clause, st);
            std::string cur;
            for (const std::string& word : words) {
                std::string next = cur.empty() ? word : cur + " " + word;
                if (!cur.empty() && (lineCount(next) > maxLines)) {
                    pieces.push_back(cur);
                    cur = word;
                } else {
                    cur = next;
                }
            }
            if (!cur.empty()) {
                pieces.push_back(cur);
            }
        }
    }

    std::vector<std::string> boxes;
    std::vector<std::string> cur;
    for (const std::string& piece : pieces) {
        std::vector<std::string> next = cur;
        next.push_back(piece);
        if (!cur.empty() && (lineCount(Join(next)) > maxLines)) {
            boxes.push_back(render(Join(cur)));
            cur.clear();
            cur.push_back(piece);
        } else {
            cur = next;
        }
    }
    if (!cur.empty() || boxes.empty()) {
        boxes.push_back(render(Join(cur)));
    }
    return boxes;
}

} // namespace GdfText

// For text that goes through CustomMessage::AutoFormat ("%rCaptain:%w My king.^%rGanondorf:%w ...")
inline std::string GdfFitBoxes(const std::string& text) {
    const GdfText::Style st = { '&', '^', false, false };
    std::string out;
    size_t start = 0;
    while (start <= text.size()) {
        size_t end = text.find('^', start);
        std::string box = text.substr(start, (end == std::string::npos ? text.size() : end) - start);
        std::string name;

        // A speaker's name opening the box: "%r<Name>:%w "
        if ((box.size() > 4) && (box[0] == '%')) {
            size_t close = box.find(":%w", 2);
            if ((close != std::string::npos) && (close < 40) && (box.find('&') > close)) {
                size_t after = close + 3;
                while ((after < box.size()) && (box[after] == ' ')) {
                    after++;
                }
                name = box.substr(0, close + 3) + " ";
                box = box.substr(after);
            }
        }
        for (const std::string& b : GdfText::FitBody(name, box, st)) {
            if (!out.empty()) {
                out += '^';
            }
            out += b;
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return out;
}

// For his lines in the game's own codes ("\x05\x41Ganondorf:\x05\x40\x01..." with '\x01' and '\x04'): his old
// boxes joined back together and laid out again by sentences, each box under his name
inline std::string GdfFitBoxesRaw(const std::string& text) {
    const GdfText::Style st = { '\x01', '\x04', true, true };
    static const std::string kName = "\x05\x41Ganondorf:\x05\x40\x01";
    if (text.compare(0, kName.size(), kName) != 0) {
        return text; // (not one of his own: left as it is)
    }
    // Back to one flowing text
    std::string body;
    size_t i = kName.size();
    while (i < text.size()) {
        if (text.compare(i, 1 + kName.size(), "\x04" + kName) == 0) {
            body += ' ';
            i += 1 + kName.size();
        } else if ((text[i] == '\x01') || (text[i] == '\x04')) {
            body += ' ';
            i++;
        } else {
            body += text[i];
            i++;
        }
    }
    std::string out;
    for (const std::string& b : GdfText::FitBody(kName, body, st)) {
        if (!out.empty()) {
            out += '\x04';
        }
        out += b;
    }
    return out;
}
