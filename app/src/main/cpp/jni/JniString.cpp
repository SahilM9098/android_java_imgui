#include "JniString.h"

#include <cstdint>

namespace menu {
namespace {

constexpr uint32_t kReplacementChar = 0xfffd;

void AppendUtf8(std::string *out, uint32_t codepoint) {
  if (codepoint <= 0x7f) {
    out->push_back(static_cast<char>(codepoint));
  } else if (codepoint <= 0x7ff) {
    out->push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else if (codepoint <= 0xffff) {
    out->push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  } else {
    out->push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
    out->push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
    out->push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
  }
}

bool DecodeUtf8(const std::string &text, size_t *offset, uint32_t *codepoint) {
  const auto *bytes = reinterpret_cast<const unsigned char *>(text.data());
  const size_t length = text.size();
  const size_t index = *offset;
  if (index >= length) {
    return false;
  }

  const unsigned char first = bytes[index];
  if (first < 0x80) {
    *codepoint = first;
    *offset = index + 1;
    return true;
  }

  uint32_t value = 0;
  size_t needed = 0;
  uint32_t minValue = 0;
  if ((first & 0xe0) == 0xc0) {
    value = first & 0x1f;
    needed = 2;
    minValue = 0x80;
  } else if ((first & 0xf0) == 0xe0) {
    value = first & 0x0f;
    needed = 3;
    minValue = 0x800;
  } else if ((first & 0xf8) == 0xf0) {
    value = first & 0x07;
    needed = 4;
    minValue = 0x10000;
  } else {
    *codepoint = kReplacementChar;
    *offset = index + 1;
    return true;
  }

  if (index + needed > length) {
    *codepoint = kReplacementChar;
    *offset = length;
    return true;
  }

  for (size_t i = 1; i < needed; ++i) {
    const unsigned char next = bytes[index + i];
    if ((next & 0xc0) != 0x80) {
      *codepoint = kReplacementChar;
      *offset = index + i;
      return true;
    }
    value = (value << 6) | (next & 0x3f);
  }

  if (value < minValue || value > 0x10ffff ||
      (value >= 0xd800 && value <= 0xdfff)) {
    *codepoint = kReplacementChar;
  } else {
    *codepoint = value;
  }
  *offset = index + needed;
  return true;
}

}  // namespace

std::string JStringToUtf8(JNIEnv *env, jstring value) {
  if (env == nullptr || value == nullptr) {
    return {};
  }

  const jsize length = env->GetStringLength(value);
  const jchar *chars = env->GetStringChars(value, nullptr);
  if (chars == nullptr) {
    return {};
  }

  std::string result;
  result.reserve(static_cast<size_t>(length) * 3);
  for (jsize i = 0; i < length; ++i) {
    uint32_t codepoint = chars[i];
    if (codepoint >= 0xd800 && codepoint <= 0xdbff && i + 1 < length) {
      const uint32_t low = chars[i + 1];
      if (low >= 0xdc00 && low <= 0xdfff) {
        codepoint = 0x10000 + (((codepoint - 0xd800) << 10) | (low - 0xdc00));
        ++i;
      } else {
        codepoint = kReplacementChar;
      }
    } else if (codepoint >= 0xdc00 && codepoint <= 0xdfff) {
      codepoint = kReplacementChar;
    }
    AppendUtf8(&result, codepoint);
  }

  env->ReleaseStringChars(value, chars);
  return result;
}

jstring Utf8ToJString(JNIEnv *env, const std::string &value) {
  if (env == nullptr) {
    return nullptr;
  }

  std::u16string utf16;
  utf16.reserve(value.size());

  size_t offset = 0;
  uint32_t codepoint = 0;
  while (DecodeUtf8(value, &offset, &codepoint)) {
    if (codepoint <= 0xffff) {
      utf16.push_back(static_cast<char16_t>(codepoint));
    } else {
      codepoint -= 0x10000;
      utf16.push_back(static_cast<char16_t>(0xd800 | (codepoint >> 10)));
      utf16.push_back(static_cast<char16_t>(0xdc00 | (codepoint & 0x3ff)));
    }
  }

  return env->NewString(reinterpret_cast<const jchar *>(utf16.data()),
                        static_cast<jsize>(utf16.size()));
}

}  // namespace menu
