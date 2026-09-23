#pragma once

#include <string>
#include <string_view>

namespace pfdb::io::emdb {

/// Convert raw `emdb.dat` bytes into strict UTF-8 JSON that a standard parser
/// accepts. EMDB writes UTF-16LE (BOM) text that *looks* like JSON but isn't:
/// string values carry raw control characters (the 0x1E record separator, and
/// sometimes raw newlines in plots) and unescaped backslashes (file paths).
///
/// This: strips the BOM and decodes UTF-16LE -> UTF-8 (handling surrogate
/// pairs), then walks the text tracking string context -- reliable because EMDB
/// never puts a raw `"` inside a value (it uses the `<DQ>` tag) -- and, while
/// inside a string, doubles backslashes and `\u`-escapes control characters.
/// The 0x1E separators survive as literal characters in the parsed values, ready
/// to split on. A UTF-8-with-BOM input is accepted and only sanitized.
std::string bytes_to_json(std::string_view raw);

}  // namespace pfdb::io::emdb
