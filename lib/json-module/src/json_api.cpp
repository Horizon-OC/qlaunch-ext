/* qlaunch-ext json-module (C) 2026 Souldbminer */
/* Licensed under the GPLv2                 */

#include <nlohmann/json.hpp>
#include <cstring>

namespace {

using nlohmann::json;

constexpr size_t kMaxInput = 1024u * 1024u;

/// @brief Test whether a JSON pointer is well-formed (RFC 6901).
/// Empty addresses the whole document; otherwise it must start with
/// '/' and every '~' must open "~0" or "~1".
static bool ValidPtr(const char *ptr)
{
    if (ptr == nullptr)
        return false;
    if (ptr[0] == '\0')
        return true;
    if (ptr[0] != '/')
        return false;
    for (const char *p = ptr; *p; p++) {
        if (*p != '~')
            continue;
        if (p[1] != '0' && p[1] != '1')
            return false;
        p++;
    }
    return true;
}

/// @brief Parse input into a document.
/// @return False for empty/oversize input or syntax errors.
static bool ParseDoc(const char *text, size_t len, json &out)
{
    if (text == nullptr || len == 0 || len > kMaxInput)
        return false;
    out = json::parse(text, text + len, nullptr, false);
    return !out.is_discarded();
}

/// @brief Resolve a pointer inside a parsed document.
/// @return Target value, or null when missing/invalid.
static const json *Resolve(const json &doc, const char *ptr)
{
    if (!ValidPtr(ptr))
        return nullptr;
    json::json_pointer jp(ptr);
    if (!doc.contains(jp))
        return nullptr;
    return &doc[jp];
}

} // namespace

extern "C" {

/// @brief Test whether a pointer resolves in the input.
/// @return 1 when present, else 0.
int json_has(const char *text, size_t len, const char *ptr)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return 0;
    return Resolve(doc, ptr) != nullptr ? 1 : 0;
}

/// @brief Give the type at a pointer: 0 null, 1 bool, 2 int, 3 float,
/// 4 string, 5 array, 6 object, -1 when missing/invalid.
int json_type(const char *text, size_t len, const char *ptr)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return -1;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr)
        return -1;
    if (v->is_null())
        return 0;
    if (v->is_boolean())
        return 1;
    if (v->is_number_integer() || v->is_number_unsigned())
        return 2;
    if (v->is_number_float())
        return 3;
    if (v->is_string())
        return 4;
    if (v->is_array())
        return 5;
    if (v->is_object())
        return 6;
    return -1;
}

/// @brief Give an array's element count, or -1.
int json_array_size(const char *text, size_t len, const char *ptr)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return -1;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr || !v->is_array())
        return -1;
    return (int)v->size();
}

/// @brief Give an object's member count, or -1.
int json_object_size(const char *text, size_t len, const char *ptr)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return -1;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr || !v->is_object())
        return -1;
    return (int)v->size();
}

/// @brief Copy an object's idx-th member name.
/// @return Name length, or -1 when missing/out of range.
int json_key_at(const char *text, size_t len, const char *ptr, int idx, char *out, size_t cap)
{
    json doc;
    if (!ParseDoc(text, len, doc) || out == nullptr || cap == 0 || idx < 0)
        return -1;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr || !v->is_object())
        return -1;
    int i = 0;
    for (auto it = v->begin(); it != v->end(); ++it, i++) {
        if (i != idx)
            continue;
        const std::string &key = it.key();
        size_t n = key.size();
        if (n >= cap)
            n = cap - 1;
        memcpy(out, key.data(), n);
        out[n] = '\0';
        return (int)n;
    }
    return -1;
}

/// @brief Read a boolean (0/1), defaulting on absence/mismatch.
int json_get_bool(const char *text, size_t len, const char *ptr, int dflt)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return dflt != 0 ? 1 : 0;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr || !v->is_boolean())
        return dflt != 0 ? 1 : 0;
    return v->get<bool>() ? 1 : 0;
}

/// @brief Read a signed integer, defaulting on absence/mismatch.
long json_get_int(const char *text, size_t len, const char *ptr, long dflt)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return dflt;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr)
        return dflt;
    if (v->is_number_integer())
        return v->get<long>();
    if (v->is_number_unsigned()) {
        unsigned long long u = v->get<unsigned long long>();
        if (u <= (unsigned long long)0x7FFFFFFFFFFFFFFFULL)
            return (long)u;
        return dflt;
    }
    return dflt;
}

/// @brief Read an unsigned integer, defaulting on absence/mismatch.
unsigned long long json_get_uint(const char *text, size_t len, const char *ptr,
                                 unsigned long long dflt)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return dflt;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr)
        return dflt;
    if (v->is_number_unsigned())
        return v->get<unsigned long long>();
    if (v->is_number_integer()) {
        long long s = v->get<long long>();
        if (s >= 0)
            return (unsigned long long)s;
        return dflt;
    }
    return dflt;
}

/// @brief Read a number as double, defaulting on absence/mismatch.
double json_get_float(const char *text, size_t len, const char *ptr, double dflt)
{
    json doc;
    if (!ParseDoc(text, len, doc))
        return dflt;
    const json *v = Resolve(doc, ptr);
    if (v == nullptr || !v->is_number())
        return dflt;
    return v->get<double>();
}

/// @brief Copy a string value (or the default) into out.
/// @return Bytes written excluding the terminator, or -1 on bad args.
int json_get_string(const char *text, size_t len, const char *ptr,
                    const char *dflt, char *out, size_t cap)
{
    if (out == nullptr || cap == 0)
        return -1;
    const char *src = dflt != nullptr ? dflt : "";
    json doc;
    if (ParseDoc(text, len, doc)) {
        const json *v = Resolve(doc, ptr);
        if (v != nullptr && v->is_string())
            src = v->get_ref<const std::string &>().c_str();
    }
    size_t n = strlen(src);
    if (n >= cap)
        n = cap - 1;
    memcpy(out, src, n);
    out[n] = '\0';
    return (int)n;
}

} /* extern C */
