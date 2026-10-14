/* qlaunch-ext (C) 2026 Souldbminer */
/* Licensed under the GPLv2         */
/* Pain... and suffering.           */

#pragma once
#include <stddef.h>

/// @brief Single JSON token (object, array, string, or primitive).
struct JxTok {
    int type;  /* 0 undef, 1 obj, 2 arr, 3 str, 4 prim */
    int start;
    int end;
    int size;  /* children count  */
};

constexpr int JX_MAX_TOKS = 512;

/// @brief Parsed JSON document: source text plus token table.
struct JxDoc {
    const char *text;
    JxTok toks[JX_MAX_TOKS];
    int count;
};

namespace Jx {
    /// @brief Parse JSON text into a token table.
    /// @param d Output document (references text, no copy).
    /// @param text JSON source bytes.
    /// @param len Byte count (max 1MB).
    /// @return True when at least one token parsed.
    bool Parse(JxDoc &d, const char *text, unsigned len);

    /// @brief Look up a key inside an object token.
    /// @param d Parsed document.
    /// @param obj Object token index.
    /// @param key Nul-terminated key.
    /// @return Value token index, or -1 when missing.
    int ObjGet(const JxDoc &d, int obj, const char *key);

    /// @brief Give a token's type code.
    /// @param d Parsed document.
    /// @param t Token index.
    /// @return 0 undef, 1 obj, 2 arr, 3 str, 4 prim.
    int TokType(const JxDoc &d, int t);

    /// @brief Give an array token's element count.
    /// @param d Parsed document.
    /// @param arr Array token index.
    /// @return Element count, or 0 when not an array.
    int ArrSize(const JxDoc &d, int arr);

    /// @brief Give an array element's token index.
    /// @param d Parsed document.
    /// @param arr Array token index.
    /// @param i Element position.
    /// @return Token index, or -1 when out of range.
    int ArrAt(const JxDoc &d, int arr, int i);

    /// @brief Give the token after a value's whole subtree.
    /// @param d Parsed document.
    /// @param t Token index.
    /// @return Next sibling token index.
    int TokAfter(const JxDoc &d, int t);

    /// @brief Test whether a primitive token is JSON true.
    /// @param d Parsed document.
    /// @param t Token index.
    /// @return True only for the literal "true".
    bool IsTrue(const JxDoc &d, int t);

    /// @brief Parse a primitive token as a long integer.
    /// @param d Parsed document.
    /// @param t Token index.
    /// @param dflt Default when malformed.
    /// @return Parsed value or default.
    long Num(const JxDoc &d, int t, long dflt);

    /// @brief Decode a string token with escape handling.
    /// @param d Parsed document.
    /// @param t Token index (string or primitive).
    /// @param out Output buffer.
    /// @param cap Buffer capacity.
    /// @return Decoded length, or -1 on failure.
    int Str(const JxDoc &d, int t, char *out, unsigned cap);

    /// @brief Compare a string token against a C string.
    /// @param d Parsed document.
    /// @param t Token index.
    /// @param s Expected value.
    /// @return True on exact match.
    bool StrEq(const JxDoc &d, int t, const char *s);
} /* namespace Jx */
