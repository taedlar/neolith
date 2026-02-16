// unicode processing
#pragma save_binary

string textwrap (string str, int width) {
    string* mbch;
    string result = "";
    int line_length = 0;

    mbch = explode (str, ""); // [NEOLITH-EXTENSION] explode to array of utf-8 characters
    foreach (string ch in mbch) {
        int char_width = (strlen (ch) > 1) ? 2 : 1; // assume multi-byte chars are double width (e.g. CJK characters)
        if (ch == "\n" || ch == "\r")
            ch = " "; // treat newlines as spaces for wrapping purposes
        if (char_width > 1 || ch == " ") { // break on spaces and multi-byte characters
            if (line_length + char_width > width) {
                result += "\n";
                line_length = 0;
            }
        }
        if (ch == " " && line_length == 0)
            continue; // skip leading spaces
        result += ch;
        if (ch == "\t")
            line_length += 8 - (line_length % 8); // tab stops every 8 characters
        else
            line_length += char_width;
    }

    return result;
}
