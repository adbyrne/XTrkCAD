# build unit tests for the dynstring library

xtrkcad_add_unit_test(dynstringtest DynStringTest
    MAIN DynStringTest.c
    LIBS dynstring)
