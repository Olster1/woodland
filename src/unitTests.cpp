static void DEBUG_runUnitTests() {
    assert(easyString_string_contains_utf8("Oliver", "iver"));
    assert(!easyString_string_contains_utf8("Olive", "iver"));
    assert(easyString_string_contains_utf8("Oliveiveiver", "iver"));
    assert(!easyString_string_contains_utf8("Oliveiveiver", ""));
    assert(!easyString_string_contains_utf8("", "iver"));
    assert(!easyString_string_contains_utf8("", ""));

    //NOTE: Test utf8 support 
    assert(string_utf8_build_shift_table("გთხოვთ").shiftCount_inRunes == 6);
    assert(easyString_getSizeInBytes_utf8("გთხოვთ") == 18); 
    assert(easyString_getStringLength_utf8("გთხოვთ") == 6); 

    char *match = "oliver";

    match += 6;

    assert(string_utf8_matchStringBackwards_(match, "oliver", easyString_getSizeInBytes_utf8("oliver")));
    assert(!string_utf8_matchStringBackwards_(match, "oli", easyString_getSizeInBytes_utf8("oli")));
    assert(!string_utf8_matchStringBackwards_(match, "olive", easyString_getSizeInBytes_utf8("olive")));

    assert(string_utf8_string_bigger_than_substring("oliver", "oliv"));
    assert(!string_utf8_string_bigger_than_substring("oliv", "oliver"));

    StringShiftTable table = string_utf8_build_shift_table("Hello");
    
    String_Query_Search_Results query = string_utf8_find_sub_string("Heelo heelo Hello Hello ", "Hello");
    assert(query.byteOffsetCount == 2);

    char *a = "ol";
    a++;
    assert(size_of_last_utf8_codepoint_in_bytes(a, 1) == 1);
    a++;
    assert(size_of_last_utf8_codepoint_in_bytes(a, 2) == 1);

    
}