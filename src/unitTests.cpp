static void DEBUG_runUnitTests() {
    assert(easyString_string_contains_utf8("Oliver", "iver"));
    assert(!easyString_string_contains_utf8("Olive", "iver"));
    assert(easyString_string_contains_utf8("Oliveiveiver", "iver"));
    assert(!easyString_string_contains_utf8("Oliveiveiver", ""));
    assert(!easyString_string_contains_utf8("", "iver"));
    assert(!easyString_string_contains_utf8("", ""));
	
}