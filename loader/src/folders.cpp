/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */

#include "folders.hpp"
#include "titles.hpp"
#include <cstring>
#include <shared/logging.hpp>
#include <stdio.h>
#include <stdlib.h>

namespace folders {

namespace {

constexpr int kOrderFileCapacity = 16384;

struct Folder {
    u64 id = 0;
    char name[kFolderNameBytes] = {0};
    u64 titleIds[kFolderMaxGames] = {0};
    int gameCount = 0;
    int color = 0;
};

Folder s_folders[kFolderMax];
int s_folderCount = 0;
u64 s_nextFolderId = 1;
bool s_foldersLoaded = false;
static char s_readMode[] = { 'r', 'b', 0 };

/// @brief Copy a name, truncating on a UTF-8 boundary and terminating.
/// @param destination Destination buffer (kFolderNameBytes).
/// @param source Source UTF-8 text.
void CopyName(char *destination, const char *source) {
    if (!destination) {
        return;
    }

    destination[0] = 0;

    if (!source)
        return;

    unsigned copyLength = 0;

    while (copyLength + 1 < kFolderNameBytes && source[copyLength])
        copyLength++;

    memcpy(destination, source, copyLength);

    while (copyLength > 0 && (destination[copyLength - 1] & 0xC0) == 0x80)
        copyLength--;

    destination[copyLength] = 0;
}

/// @brief Check a JSON escape lead-in.
/// @param c Character.
/// @return True for characters starting an escape pair.
bool IsEscaped(char escapedChar) {
    return escapedChar == '"' || escapedChar == '\\' || escapedChar == '/' || escapedChar == 'b' ||
           escapedChar == 'f' || escapedChar == 'n' || escapedChar == 'r' || escapedChar == 't' ||
           escapedChar == 'u';
}

/// @brief Append a Unicode codepoint as UTF-8.
/// @param writeCursor Output cursor (advanced).
/// @param bufferEnd Output end (room for 4 bytes + NUL required by caller).
/// @param codepoint Codepoint.
/// @return True on success.
bool PutCodepoint(char *&writeCursor, char *bufferEnd, unsigned codepoint) {
    if (codepoint < 0x80) {
        if (writeCursor + 1 >= bufferEnd)
            return false;

        *writeCursor++ = (char)codepoint;
    } else if (codepoint < 0x800) {
        if (writeCursor + 2 >= bufferEnd)
            return false;

        *writeCursor++ = (char)(0xC0 | (codepoint >> 6));
        *writeCursor++ = (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint < 0x10000) {
        if (writeCursor + 3 >= bufferEnd)
            return false;

        *writeCursor++ = (char)(0xE0 | (codepoint >> 12));
        *writeCursor++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        *writeCursor++ = (char)(0x80 | (codepoint & 0x3F));
    } else if (codepoint <= 0x10FFFF) {
        if (writeCursor + 4 >= bufferEnd)
            return false;

        *writeCursor++ = (char)(0xF0 | (codepoint >> 18));
        *writeCursor++ = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        *writeCursor++ = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        *writeCursor++ = (char)(0x80 | (codepoint & 0x3F));
    } else {
        return false;
    }

    return true;
}

/// @brief Parse 4 hex digits.
/// @param digits Digit run.
/// @param value Value output.
/// @return True on success.
bool ParseHex4(const char *digits, unsigned *value) {
    unsigned parsedValue = 0;

    for (int digitIndex = 0; digitIndex < 4; digitIndex++) {
        char digitChar = digits[digitIndex];
        parsedValue <<= 4;

        if (digitChar >= '0' && digitChar <= '9')
            parsedValue |= (unsigned)(digitChar - '0');
        else if (digitChar >= 'a' && digitChar <= 'f')
            parsedValue |= (unsigned)(digitChar - 'a' + 10);
        else if (digitChar >= 'A' && digitChar <= 'F')
            parsedValue |= (unsigned)(digitChar - 'A' + 10);
        else
            return false;
    }

    *value = parsedValue;
    return true;
}

/// @brief JSON cursor over the loaded file text.
struct Cursor {
    const char *position;
    const char *limit;
};

/// @brief Skip JSON whitespace.
/// @param cursor Cursor.
void SkipSpace(Cursor &cursor) {
    while (cursor.position < cursor.limit && (*cursor.position == ' ' || *cursor.position == '\t' || *cursor.position == '\n' || *cursor.position == '\r'))
        cursor.position++;
}

/// @brief Consume an expected byte.
/// @param cursor Cursor.
/// @param want Expected byte.
/// @return True when consumed.
bool TakeByte(Cursor &cursor, char want) {
    SkipSpace(cursor);

    if (cursor.position >= cursor.limit || *cursor.position != want)
        return false;

    cursor.position++;
    return true;
}

/// @brief Match a literal keyword.
/// @param cursor Cursor.
/// @param word Keyword.
/// @return True when matched.
bool TakeWord(Cursor &cursor, const char *word) {
    SkipSpace(cursor);

    while (*word && cursor.position < cursor.limit && *cursor.position == *word) {
        cursor.position++;
        word++;
    }

    return *word == 0;
}

/// @brief Parse an unsigned decimal.
/// @param cursor Cursor.
/// @param outputValue Value output.
/// @return True when at least one digit parsed.
bool TakeNumber(Cursor &cursor, u64 *outputValue) {
    SkipSpace(cursor);

    u64 value = 0;
    bool anyDigits = false;

    while (cursor.position < cursor.limit && *cursor.position >= '0' && *cursor.position <= '9') {
        anyDigits = true;
        value = value * 10 + (u64)(*cursor.position - '0');
        cursor.position++;
    }

    if (outputValue)
        *outputValue = value;

    return anyDigits;
}

/// @brief Parse a JSON string with escape handling into a name buffer.
/// @param cursor Cursor (must sit on the opening quote).
/// @param output Output buffer.
/// @param capacity Buffer capacity.
/// @return True on success.
bool TakeName(Cursor &cursor, char *output, unsigned capacity) {
    if (!output || capacity == 0)
        return false;

    output[0] = 0;
    SkipSpace(cursor);

    if (cursor.position >= cursor.limit || *cursor.position != '"')
        return false;

    cursor.position++;

    char *writeCursor = output;
    char *bufferEnd = output + capacity;

    while (cursor.position < cursor.limit && *cursor.position != '"') {
        if (*cursor.position != '\\') {
            if (writeCursor + 1 >= bufferEnd)
                return false;

            *writeCursor++ = *cursor.position++;
            continue;
        }

        cursor.position++;

        if (cursor.position >= cursor.limit || !IsEscaped(*cursor.position))
            return false;

        char escapeCode = *cursor.position++;

        if (escapeCode == 'u') {
            if (cursor.limit - cursor.position < 4)
                return false;

            unsigned codepoint = 0;

            if (!ParseHex4(cursor.position, &codepoint))
                return false;

            cursor.position += 4;

            if (codepoint >= 0xD800 && codepoint <= 0xDBFF) {
                if (cursor.limit - cursor.position < 6 || cursor.position[0] != '\\' || cursor.position[1] != 'u')
                    return false;

                unsigned lowSurrogate = 0;

                if (!ParseHex4(cursor.position + 2, &lowSurrogate))
                    return false;

                if (lowSurrogate < 0xDC00 || lowSurrogate > 0xDFFF)
                    return false;

                cursor.position += 6;
                codepoint = 0x10000 + ((codepoint - 0xD800) << 10) + (lowSurrogate - 0xDC00);
            }

            if (!PutCodepoint(writeCursor, bufferEnd, codepoint))
                return false;
        } else {
            if (writeCursor + 1 >= bufferEnd)
                return false;

            switch (escapeCode) {
                case '"': *writeCursor++ = '"'; break;
                case '\\': *writeCursor++ = '\\'; break;
                case '/': *writeCursor++ = '/'; break;
                case 'b': *writeCursor++ = '\b'; break;
                case 'f': *writeCursor++ = '\f'; break;
                case 'n': *writeCursor++ = '\n'; break;
                case 'r': *writeCursor++ = '\r'; break;
                case 't': *writeCursor++ = '\t'; break;
                default: return false;
            }
        }
    }

    if (cursor.position >= cursor.limit)
        return false;

    cursor.position++;

    if (writeCursor >= bufferEnd)
        return false;

    *writeCursor = 0;
    return true;
}

/// @brief Skip one JSON value of any shape.
/// @param c Cursor.
/// @return True on success.
bool SkipValue(Cursor &cursor) {
    SkipSpace(cursor);

    if (cursor.position >= cursor.limit)
        return false;

    if (*cursor.position == '{') {
        cursor.position++;
        SkipSpace(cursor);

        if (cursor.position < cursor.limit && *cursor.position == '}') {
            cursor.position++;
            return true;
        }

        while (true) {
            if (cursor.position >= cursor.limit || *cursor.position != '"')
                return false;

            char scratch[4];

            if (!TakeName(cursor, scratch, sizeof(scratch)))
                return false;

            if (!TakeByte(cursor, ':') || !SkipValue(cursor))
                return false;

            SkipSpace(cursor);

            if (cursor.position < cursor.limit && *cursor.position == ',') {
                cursor.position++;
                continue;
            }

            return TakeByte(cursor, '}');
        }
    }

    if (*cursor.position == '[') {
        cursor.position++;
        SkipSpace(cursor);

        if (cursor.position < cursor.limit && *cursor.position == ']') {
            cursor.position++;
            return true;
        }

        while (true) {
            if (!SkipValue(cursor))
                return false;

            SkipSpace(cursor);

            if (cursor.position < cursor.limit && *cursor.position == ',') {
                cursor.position++;
                continue;
            }

            return TakeByte(cursor, ']');
        }
    }

    if (*cursor.position == '"') {
        char scratch[4];
        return TakeName(cursor, scratch, sizeof(scratch));
    }

    if ((*cursor.position >= '0' && *cursor.position <= '9') || *cursor.position == '-') {
        if (*cursor.position == '-')
            cursor.position++;

        while (cursor.position < cursor.limit && ((*cursor.position >= '0' && *cursor.position <= '9') || *cursor.position == '.' || *cursor.position == 'e' ||
                               *cursor.position == 'E' || *cursor.position == '+' || *cursor.position == '-'))
            cursor.position++;

        return true;
    }

    return TakeWord(cursor, "true") || TakeWord(cursor, "false") || TakeWord(cursor, "null");
}

/// @brief Write one escaped JSON string.
/// @param outputFile Output file.
/// @param text UTF-8 text.
void WriteName(FILE *outputFile, const char *text) {
    fputc('"', outputFile);
    for (const unsigned char *byteCursor = (const unsigned char *)text; *byteCursor; byteCursor++) {
        unsigned char currentByte = *byteCursor;
        if (currentByte == '"')
            fputs("\\\"", outputFile);
        else if (currentByte == '\\')
            fputs("\\\\", outputFile);
        else if (currentByte == '\b')
            fputs("\\b", outputFile);
        else if (currentByte == '\f')
            fputs("\\f", outputFile);
        else if (currentByte == '\n')
            fputs("\\n", outputFile);
        else if (currentByte == '\r')
            fputs("\\r", outputFile);
        else if (currentByte == '\t')
            fputs("\\t", outputFile);
        else if (currentByte < 0x20) {
            const char *hexDigits = "0123456789abcdef";
            fputs("\\u00", outputFile);
            fputc(hexDigits[(currentByte >> 4) & 0xF], outputFile);
            fputc(hexDigits[currentByte & 0xF], outputFile);
        } else {
            fputc(currentByte, outputFile);
        }
    }
    fputc('"', outputFile);
}

/// @brief Persist folders to disk through the combined order file.
void Save() {
    titles::Save();
}

/// @brief Parse one folder object; cursor sits past '{'.
/// @param cursor Cursor.
/// @param target Folder output.
/// @return True on success.
bool ParseFolder(Cursor &cursor, Folder *target) {
    target->id = 0;
    target->name[0] = 0;
    target->gameCount = 0;
    target->color = 0;

    while (true) {
        SkipSpace(cursor);

        if (cursor.position < cursor.limit && *cursor.position == '}') {
            cursor.position++;
            break;
        }

        char key[16];

        if (!TakeName(cursor, key, sizeof(key)) || !TakeByte(cursor, ':'))
            return false;

        if (strcmp(key, "id") == 0) {
            u64 folderId = 0;

            if (!TakeNumber(cursor, &folderId) || folderId == 0)
                return false;

            target->id = folderId;
        } else if (strcmp(key, "name") == 0) {
            char folderName[kFolderNameBytes];

            if (!TakeName(cursor, folderName, sizeof(folderName)))
                return false;

            CopyName(target->name, folderName);
        } else if (strcmp(key, "color") == 0) {
            u64 colorIndex = 0;

            if (TakeNumber(cursor, &colorIndex) && colorIndex < (u64)kFolderColorCount)
                target->color = (int)colorIndex;
        } else if (strcmp(key, "tids") == 0) {
            if (!TakeByte(cursor, '['))
                return false;

            SkipSpace(cursor);

            if (cursor.position < cursor.limit && *cursor.position == ']') {
                cursor.position++;
            } else {
                while (true) {
                    u64 titleId = 0;

                    if (!TakeNumber(cursor, &titleId))
                        return false;

                    if (titleId != 0 && target->gameCount < kFolderMaxGames) {
                        bool duplicateFound = false;

                        for (int gameIndex = 0; gameIndex < target->gameCount; gameIndex++) {
                            if (target->titleIds[gameIndex] == titleId) {
                                duplicateFound = true;
                                break;
                            }
                        }

                        if (!duplicateFound)
                            target->titleIds[target->gameCount++] = titleId;
                    }

                    SkipSpace(cursor);

                    if (cursor.position < cursor.limit && *cursor.position == ',') {
                        cursor.position++;
                        continue;
                    }

                    if (!TakeByte(cursor, ']'))
                        return false;

                    break;
                }
            }
        } else if (!SkipValue(cursor)) {
            return false;
        }

        SkipSpace(cursor);

        if (cursor.position < cursor.limit && *cursor.position == ',') {
            cursor.position++;
            continue;
        }

        if (!TakeByte(cursor, '}'))
            return false;

        break;
    }

    if (target->id == 0 || target->gameCount <= 0)
        return false;

    if (target->name[0] == 0)
        CopyName(target->name, "Folder");

    return true;
}

/// @brief Parse a folders value object; cursor sits past '{'.
/// @param cursor Cursor.
/// @param outFolders Output array.
/// @param outCount Output count.
/// @param outNext Next-id output (0 when absent).
/// @return True on success.
bool ParseFoldersBody(Cursor &cursor, Folder *outFolders, int &outCount, u64 &outNext) {
    outCount = 0;
    outNext = 0;
    bool haveNext = false;

    while (true) {
        SkipSpace(cursor);

        if (cursor.position < cursor.limit && *cursor.position == '}') {
            cursor.position++;
            break;
        }

        char key[16];

        if (!TakeName(cursor, key, sizeof(key)) || !TakeByte(cursor, ':'))
            return false;

        if (strcmp(key, "nextId") == 0) {
            u64 nextFolderId = 0;

            if (TakeNumber(cursor, &nextFolderId) && nextFolderId > 0) {
                outNext = nextFolderId;
                haveNext = true;
            }
        } else if (strcmp(key, "folders") == 0) {
            if (!TakeByte(cursor, '['))
                return false;

            SkipSpace(cursor);

            if (cursor.position < cursor.limit && *cursor.position == ']') {
                cursor.position++;
            } else {
                while (true) {
                    if (!TakeByte(cursor, '{'))
                        return false;

                    if (outCount >= kFolderMax)
                        return false;

                    Folder parsedFolder{};

                    if (!ParseFolder(cursor, &parsedFolder))
                        return false;

                    bool duplicateId = false;

                    for (int folderIndex = 0; folderIndex < outCount; folderIndex++) {
                        if (outFolders[folderIndex].id == parsedFolder.id) {
                            duplicateId = true;
                            break;
                        }
                    }

                    if (!duplicateId)
                        outFolders[outCount++] = parsedFolder;

                    SkipSpace(cursor);

                    if (cursor.position < cursor.limit && *cursor.position == ',') {
                        cursor.position++;
                        continue;
                    }

                    if (!TakeByte(cursor, ']'))
                        return false;

                    break;
                }
            }
        } else if (!SkipValue(cursor)) {
            return false;
        }

        SkipSpace(cursor);

        if (cursor.position < cursor.limit && *cursor.position == ',') {
            cursor.position++;
            continue;
        }

        if (!TakeByte(cursor, '}'))
            return false;

        break;
    }

    if (!haveNext)
        outNext = 0;

    return true;
}

/// @brief Commit parsed folders, fixing up the next id.
/// @param parsedFolders Parsed folders.
/// @param folderCount Parsed count.
/// @param fileNextId Next id from file (0 when absent).
void CommitFolders(const Folder *parsedFolders, int folderCount, u64 fileNextId) {
    s_folderCount = 0;

    for (int folderIndex = 0; folderIndex < folderCount && folderIndex < kFolderMax; folderIndex++)
        s_folders[s_folderCount++] = parsedFolders[folderIndex];

    if (fileNextId > s_nextFolderId)
        s_nextFolderId = fileNextId;

    for (int folderIndex = 0; folderIndex < s_folderCount; folderIndex++) {
        if (s_folders[folderIndex].id >= s_nextFolderId)
            s_nextFolderId = s_folders[folderIndex].id + 1;
    }
}

/// @brief Load folders from a file.
/// @param filePath File path.
/// @return True when a folders object parsed (possibly empty).
bool LoadFoldersFile(const char *filePath) {
    FILE *orderFile = fopen(filePath, s_readMode);

    if (!orderFile)
        return false;

    char *fileText = (char *)malloc(kOrderFileCapacity);

    if (!fileText) {
        fclose(orderFile);
        return false;
    }

    size_t readBytes = fread(fileText, 1, kOrderFileCapacity - 1, orderFile);
    fclose(orderFile);
    fileText[readBytes] = 0;

    Cursor cursor = { fileText, fileText + readBytes };
    bool parseSucceeded = false;

    if (TakeByte(cursor, '{')) {
        while (true) {
            SkipSpace(cursor);

            if (cursor.position < cursor.limit && *cursor.position == '}') {
                cursor.position++;
                break;
            }

            char key[16];

            if (!TakeName(cursor, key, sizeof(key)) || !TakeByte(cursor, ':'))
                break;

            if (strcmp(key, "folders") == 0) {
                if (!TakeByte(cursor, '{'))
                    break;

                Folder stagedFolders[kFolderMax];
                int stagedCount = 0;
                u64 stagedNextId = 0;
                parseSucceeded = ParseFoldersBody(cursor, stagedFolders, stagedCount, stagedNextId);

                if (parseSucceeded)
                    CommitFolders(stagedFolders, stagedCount, stagedNextId);

                break;
            } else if (!SkipValue(cursor)) {
                break;
            }

            SkipSpace(cursor);

            if (cursor.position < cursor.limit && *cursor.position == ',') {
                cursor.position++;
                continue;
            }

            if (!TakeByte(cursor, '}'))
                break;

            break;
        }
    }

    free(fileText);
    return parseSucceeded;
}

/// @brief Load folders from disk on first use.
/// Reads the folders section of order.json.
void EnsureLoaded() {
    if (s_foldersLoaded)
        return;

    s_foldersLoaded = true;
    LoadFoldersFile(titles::kOrderPath);
}

/// @brief Find a folder by id.
/// @param folderId Folder id.
/// @return Index, or negative when missing.
int IndexOfIdLocked(u64 folderId) {
    if (folderId == 0)
        return -1;

    for (int folderIndex = 0; folderIndex < s_folderCount; folderIndex++) {
        if (s_folders[folderIndex].id == folderId)
            return folderIndex;
    }

    return -1;
}

} // namespace

/// @brief Write one decimal u64.
/// @param outputFile Output file.
/// @param value Value.
static void WriteDec(FILE *outputFile, u64 value) {
    char digitBuffer[24];
    int digitCount = 0;

    if (value == 0)
        fputc('0', outputFile);

    while (value > 0 && digitCount < 24) {
        digitBuffer[digitCount++] = (char)('0' + value % 10);
        value /= 10;
    }

    while (digitCount > 0)
        fputc(digitBuffer[--digitCount], outputFile);
}

void WriteSection(FILE *outputFile) {
    EnsureLoaded();
    fputs("\"folders\": {\"nextId\": ", outputFile);
    WriteDec(outputFile, s_nextFolderId);
    fputs(", \"folders\": [", outputFile);

    for (int folderIndex = 0; folderIndex < s_folderCount; folderIndex++) {
        if (folderIndex > 0)
            fputc(',', outputFile);

        fputs("{\"id\": ", outputFile);
        WriteDec(outputFile, s_folders[folderIndex].id);
        fputs(", \"name\": ", outputFile);
        WriteName(outputFile, s_folders[folderIndex].name);
        fputs(", \"color\": ", outputFile);
        WriteDec(outputFile, (u64)s_folders[folderIndex].color);
        fputs(", \"tids\": [", outputFile);

        for (int gameIndex = 0; gameIndex < s_folders[folderIndex].gameCount; gameIndex++) {
            if (gameIndex > 0)
                fputc(',', outputFile);

            WriteDec(outputFile, s_folders[folderIndex].titleIds[gameIndex]);
        }

        fputs("]}", outputFile);
    }

    fputs("]}", outputFile);
}

int Count() {
    EnsureLoaded();

    return s_folderCount;
}

u64 Id(int index) {
    EnsureLoaded();

    if (index < 0 || index >= s_folderCount)
        return 0;

    return s_folders[index].id;
}

int Name(int index, char *output, unsigned capacity) {
    EnsureLoaded();

    if (index < 0 || index >= s_folderCount || !output || capacity == 0)
        return -1;

    unsigned nameLength = 0;

    while (nameLength + 1 < capacity && s_folders[index].name[nameLength]) {
        output[nameLength] = s_folders[index].name[nameLength];
        nameLength++;
    }

    output[nameLength] = 0;
    return (int)nameLength;
}

int GameCount(int index) {
    EnsureLoaded();

    if (index < 0 || index >= s_folderCount)
        return -1;

    return s_folders[index].gameCount;
}

u64 Game(int index, int position) {
    EnsureLoaded();

    if (index < 0 || index >= s_folderCount)
        return 0;

    if (position < 0 || position >= s_folders[index].gameCount)
        return 0;

    return s_folders[index].titleIds[position];
}

int Color(int index) {
    EnsureLoaded();

    if (index < 0 || index >= s_folderCount)
        return -1;

    return s_folders[index].color;
}

int SetColor(u64 folderId, int colorIndex) {
    EnsureLoaded();
    int folderIndex = IndexOfIdLocked(folderId);

    if (folderIndex < 0)
        return -1;

    if (colorIndex < 0 || colorIndex >= kFolderColorCount)
        return -1;

    s_folders[folderIndex].color = colorIndex;
    Save();
    return 0;
}

int IndexOfId(u64 folderId) {
    EnsureLoaded();
    return IndexOfIdLocked(folderId);
}

int IndexOfGame(u64 titleId) {
    EnsureLoaded();

    if (titleId == 0)
        return -1;

    for (int folderIndex = 0; folderIndex < s_folderCount; folderIndex++) {
        for (int gameIndex = 0; gameIndex < s_folders[folderIndex].gameCount; gameIndex++) {
            if (s_folders[folderIndex].titleIds[gameIndex] == titleId)
                return folderIndex;
        }
    }

    return -1;
}

u64 Create(const char *folderName, u64 titleId) {
    EnsureLoaded();

    if (!folderName || !folderName[0] || titleId == 0 || s_folderCount >= kFolderMax)
        return 0;

    if (IndexOfGame(titleId) >= 0)
        return 0;

    Folder newFolder{};
    newFolder.id = s_nextFolderId++;
    CopyName(newFolder.name, folderName);

    if (newFolder.name[0] == 0)
        return 0;

    newFolder.titleIds[0] = titleId;
    newFolder.gameCount = 1;
    s_folders[s_folderCount++] = newFolder;
    Save();
    return newFolder.id;
}

int Rename(u64 folderId, const char *folderName) {
    EnsureLoaded();
    int folderIndex = IndexOfIdLocked(folderId);

    if (folderIndex < 0 || !folderName || !folderName[0])
        return -1;

    char cleanedName[kFolderNameBytes];
    CopyName(cleanedName, folderName);

    if (cleanedName[0] == 0)
        return -1;

    memcpy(s_folders[folderIndex].name, cleanedName, sizeof(cleanedName));
    Save();
    return 0;
}

int Delete(u64 folderId) {
    EnsureLoaded();
    int folderIndex = IndexOfIdLocked(folderId);

    if (folderIndex < 0)
        return -1;

    for (int shiftIndex = folderIndex; shiftIndex + 1 < s_folderCount; shiftIndex++)
        s_folders[shiftIndex] = s_folders[shiftIndex + 1];

    s_folders[s_folderCount - 1] = Folder{};
    s_folderCount--;
    Save();
    return 0;
}

int Add(u64 folderId, u64 titleId) {
    EnsureLoaded();
    int folderIndex = IndexOfIdLocked(folderId);

    if (folderIndex < 0 || titleId == 0)
        return -1;

    if (IndexOfGame(titleId) >= 0)
        return -1;

    if (s_folders[folderIndex].gameCount >= kFolderMaxGames)
        return -1;

    s_folders[folderIndex].titleIds[s_folders[folderIndex].gameCount++] = titleId;
    Save();
    return 0;
}

int Remove(u64 folderId, u64 titleId) {
    EnsureLoaded();
    int folderIndex = IndexOfIdLocked(folderId);

    if (folderIndex < 0 || titleId == 0)
        return -1;

    int memberPosition = -1;

    for (int gameIndex = 0; gameIndex < s_folders[folderIndex].gameCount; gameIndex++) {
        if (s_folders[folderIndex].titleIds[gameIndex] == titleId) {
            memberPosition = gameIndex;
            break;
        }
    }

    if (memberPosition < 0)
        return -1;

    for (int gameIndex = memberPosition; gameIndex + 1 < s_folders[folderIndex].gameCount; gameIndex++)
        s_folders[folderIndex].titleIds[gameIndex] = s_folders[folderIndex].titleIds[gameIndex + 1];

    s_folders[folderIndex].gameCount--;

    if (s_folders[folderIndex].gameCount <= 0)
        return Delete(folderId);

    Save();
    return 0;
}

void Prune(const u64 *liveTids, int liveCount) {
    EnsureLoaded();
    bool changed = false;

    for (int folderIndex = 0; folderIndex < s_folderCount; folderIndex++) {
        int writePosition = 0;

        for (int gameIndex = 0; gameIndex < s_folders[folderIndex].gameCount; gameIndex++) {
            bool liveMember = false;

            for (int liveIndex = 0; liveIndex < liveCount; liveIndex++) {
                if (liveTids && liveTids[liveIndex] == s_folders[folderIndex].titleIds[gameIndex]) {
                    liveMember = true;
                    break;
                }
            }

            if (liveMember)
                s_folders[folderIndex].titleIds[writePosition++] = s_folders[folderIndex].titleIds[gameIndex];
            else
                changed = true;
        }

        s_folders[folderIndex].gameCount = writePosition;
    }

    for (int folderIndex = s_folderCount - 1; folderIndex >= 0; folderIndex--) {
        if (s_folders[folderIndex].gameCount <= 0) {
            for (int shiftIndex = folderIndex; shiftIndex + 1 < s_folderCount; shiftIndex++)
                s_folders[shiftIndex] = s_folders[shiftIndex + 1];

            s_folders[s_folderCount - 1] = Folder{};
            s_folderCount--;
            changed = true;
        }
    }

    if (changed)
        Save();
}

} // namespace folders
