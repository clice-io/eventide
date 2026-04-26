#include "kota/zest/zest.h"
#include "kota/support/glob_pattern.h"

namespace kota {

namespace {

#define PATDEF(NAME, PAT)                                                                          \
    auto Res##NAME = kota::GlobPattern::create(PAT, 100);                                         \
    EXPECT_TRUE(Res##NAME.has_value());                                                            \
    if(!Res##NAME.has_value()) return;                                                             \
    auto NAME = std::move(*Res##NAME);

TEST_SUITE(GlobPattern) {

TEST_CASE(PatternSema) {
    auto Pat1 = kota::GlobPattern::create("**/****.{c,cc}", 100);
    EXPECT_FALSE(Pat1.has_value());

    auto Pat2 = kota::GlobPattern::create("/foo/bar/baz////aaa.{c,cc}", 100);
    EXPECT_FALSE(Pat2.has_value());

    auto Pat3 = kota::GlobPattern::create("/foo/bar/baz/**////*.{c,cc}", 100);
    EXPECT_FALSE(Pat3.has_value());
}

TEST_CASE(MaxSubGlob) {
    auto Pat1 = kota::GlobPattern::create("{AAA,BBB,AB*}");
    EXPECT_TRUE(Pat1.has_value());
    EXPECT_TRUE(Pat1->match("AAA"));
    EXPECT_TRUE(Pat1->match("BBB"));
    EXPECT_TRUE(Pat1->match("AB"));
    EXPECT_TRUE(Pat1->match("ABCD"));
    EXPECT_FALSE(Pat1->match("CCC"));
    EXPECT_TRUE(Pat1->match("ABCDE"));
}

TEST_CASE(Simple) {
    PATDEF(Pat1, "node_modules")
    EXPECT_TRUE(Pat1.match("node_modules"));
    EXPECT_FALSE(Pat1.match("node_module"));
    EXPECT_FALSE(Pat1.match("/node_modules"));
    EXPECT_FALSE(Pat1.match("test/node_modules"));

    PATDEF(Pat2, "test.txt")
    EXPECT_TRUE(Pat2.match("test.txt"));
    EXPECT_FALSE(Pat2.match("test?txt"));
    EXPECT_FALSE(Pat2.match("/text.txt"));
    EXPECT_FALSE(Pat2.match("test/test.txt"));

    PATDEF(Pat3, "test(.txt")
    EXPECT_TRUE(Pat3.match("test(.txt"));
    EXPECT_FALSE(Pat3.match("test?txt"));

    PATDEF(Pat4, "qunit")
    EXPECT_TRUE(Pat4.match("qunit"));
    EXPECT_FALSE(Pat4.match("qunit.css"));
    EXPECT_FALSE(Pat4.match("test/qunit"));

    PATDEF(Pat5, "/DNXConsoleApp/**/*.cs")
    EXPECT_TRUE(Pat5.match("/DNXConsoleApp/Program.cs"));
    EXPECT_TRUE(Pat5.match("/DNXConsoleApp/foo/Program.cs"));
}

TEST_CASE(DotHidden) {
    PATDEF(Pat1, ".*")
    EXPECT_TRUE(Pat1.match(".git"));
    EXPECT_TRUE(Pat1.match(".hidden.txt"));
    EXPECT_FALSE(Pat1.match("git"));
    EXPECT_FALSE(Pat1.match("hidden.txt"));
    EXPECT_FALSE(Pat1.match("path/.git"));
    EXPECT_FALSE(Pat1.match("path/.hidden.txt"));

    PATDEF(Pat2, "**/.*")
    EXPECT_TRUE(Pat2.match(".git"));
    EXPECT_TRUE(Pat2.match("/.git"));
    EXPECT_TRUE(Pat2.match(".hidden.txt"));
    EXPECT_FALSE(Pat2.match("git"));
    EXPECT_FALSE(Pat2.match("hidden.txt"));
    EXPECT_TRUE(Pat2.match("path/.git"));
    EXPECT_TRUE(Pat2.match("path/.hidden.txt"));
    EXPECT_TRUE(Pat2.match("/path/.git"));
    EXPECT_TRUE(Pat2.match("/path/.hidden.txt"));
    EXPECT_FALSE(Pat2.match("path/git"));
    EXPECT_FALSE(Pat2.match("pat.h/hidden.txt"));

    PATDEF(Pat3, "._*")
    EXPECT_TRUE(Pat3.match("._git"));
    EXPECT_TRUE(Pat3.match("._hidden.txt"));
    EXPECT_FALSE(Pat3.match("git"));
    EXPECT_FALSE(Pat3.match("hidden.txt"));
    EXPECT_FALSE(Pat3.match("path/._git"));
    EXPECT_FALSE(Pat3.match("path/._hidden.txt"));

    PATDEF(Pat4, "**/._*")
    EXPECT_TRUE(Pat4.match("._git"));
    EXPECT_TRUE(Pat4.match("._hidden.txt"));
    EXPECT_FALSE(Pat4.match("git"));
    EXPECT_FALSE(Pat4.match("hidden._txt"));
    EXPECT_TRUE(Pat4.match("path/._git"));
    EXPECT_TRUE(Pat4.match("path/._hidden.txt"));
    EXPECT_TRUE(Pat4.match("/path/._git"));
    EXPECT_TRUE(Pat4.match("/path/._hidden.txt"));
    EXPECT_FALSE(Pat4.match("path/git"));
    EXPECT_FALSE(Pat4.match("pat.h/hidden._txt"));
}

TEST_CASE(EscapeCharacter) {
    PATDEF(Pat1, R"(\*star)")
    EXPECT_TRUE(Pat1.match("*star"));

    PATDEF(Pat2, R"(\{\*\})")
    EXPECT_TRUE(Pat2.match("{*}"));
}

TEST_CASE(BracketExpr) {
    PATDEF(Pat1, R"([a-zA-Z\]])")
    EXPECT_TRUE(Pat1.match(R"(])"));
    EXPECT_FALSE(Pat1.match(R"([)"));
    EXPECT_TRUE(Pat1.match(R"(s)"));
    EXPECT_TRUE(Pat1.match(R"(S)"));
    EXPECT_FALSE(Pat1.match(R"(0)"));

    PATDEF(Pat2, R"([\\^a-zA-Z""\\])")
    EXPECT_TRUE(Pat2.match(R"(")"));
    EXPECT_TRUE(Pat2.match(R"(^)"));
    EXPECT_TRUE(Pat2.match(R"(\)"));
    EXPECT_TRUE(Pat2.match(R"(")"));
    EXPECT_TRUE(Pat2.match(R"(x)"));
    EXPECT_TRUE(Pat2.match(R"(X)"));
    EXPECT_FALSE(Pat2.match(R"(0)"));

    PATDEF(Pat3, R"([!0-9a-fA-F\-+\*])")
    EXPECT_FALSE(Pat3.match("1"));
    EXPECT_FALSE(Pat3.match("*"));
    EXPECT_TRUE(Pat3.match("s"));
    EXPECT_TRUE(Pat3.match("S"));
    EXPECT_TRUE(Pat3.match("H"));
    EXPECT_TRUE(Pat3.match("]"));

    PATDEF(Pat4, R"([^\^0-9a-fA-F\-+\*])")
    EXPECT_FALSE(Pat4.match("1"));
    EXPECT_FALSE(Pat4.match("*"));
    EXPECT_FALSE(Pat4.match("^"));
    EXPECT_TRUE(Pat4.match("s"));
    EXPECT_TRUE(Pat4.match("S"));
    EXPECT_TRUE(Pat4.match("H"));
    EXPECT_TRUE(Pat4.match("]"));

    PATDEF(Pat5, R"([\*-\^])")
    EXPECT_TRUE(Pat5.match("*"));
    EXPECT_FALSE(Pat5.match("a"));
    EXPECT_FALSE(Pat5.match("z"));
    EXPECT_TRUE(Pat5.match("A"));
    EXPECT_TRUE(Pat5.match("Z"));
    EXPECT_TRUE(Pat5.match("\\"));
    EXPECT_TRUE(Pat5.match("^"));
    EXPECT_TRUE(Pat5.match("-"));

    PATDEF(Pat6, "foo.[^0-9]")
    EXPECT_FALSE(Pat6.match("foo.5"));
    EXPECT_FALSE(Pat6.match("foo.8"));
    EXPECT_FALSE(Pat6.match("bar.5"));
    EXPECT_TRUE(Pat6.match("foo.f"));

    PATDEF(Pat7, "foo.[!0-9]")
    EXPECT_FALSE(Pat7.match("foo.5"));
    EXPECT_FALSE(Pat7.match("foo.8"));
    EXPECT_FALSE(Pat7.match("bar.5"));
    EXPECT_TRUE(Pat7.match("foo.f"));

    PATDEF(Pat8, "foo.[0!^*?]")
    EXPECT_FALSE(Pat8.match("foo.5"));
    EXPECT_FALSE(Pat8.match("foo.8"));
    EXPECT_TRUE(Pat8.match("foo.0"));
    EXPECT_TRUE(Pat8.match("foo.!"));
    EXPECT_TRUE(Pat8.match("foo.^"));
    EXPECT_TRUE(Pat8.match("foo.*"));
    EXPECT_TRUE(Pat8.match("foo.?"));

    PATDEF(Pat9, "foo[/]bar")
    EXPECT_FALSE(Pat9.match("foo/bar"));

    PATDEF(Pat10, "foo.[[]")
    EXPECT_TRUE(Pat10.match("foo.["));

    PATDEF(Pat11, "foo.[]]")
    EXPECT_TRUE(Pat11.match("foo.]"));

    PATDEF(Pat12, "foo.[][!]")
    EXPECT_TRUE(Pat12.match("foo.]"));
    EXPECT_TRUE(Pat12.match("foo.["));
    EXPECT_TRUE(Pat12.match("foo.!"));

    PATDEF(Pat13, "foo.[]-]")
    EXPECT_TRUE(Pat13.match("foo.]"));
    EXPECT_TRUE(Pat13.match("foo.-"));

    PATDEF(Pat14, "foo.[0-9]")
    EXPECT_TRUE(Pat14.match("foo.5"));
    EXPECT_TRUE(Pat14.match("foo.8"));
    EXPECT_FALSE(Pat14.match("bar.5"));
    EXPECT_FALSE(Pat14.match("foo.f"));
}

TEST_CASE(BraceExpr) {
    PATDEF(Pat1, "*foo[0-9a-z].{c,cpp,cppm,?pp}")
    EXPECT_FALSE(Pat1.match("foo1.cc"));
    EXPECT_TRUE(Pat1.match("foo2.cpp"));
    EXPECT_TRUE(Pat1.match("foo3.cppm"));
    EXPECT_TRUE(Pat1.match("foot.cppm"));
    EXPECT_TRUE(Pat1.match("foot.hpp"));
    EXPECT_TRUE(Pat1.match("foot.app"));
    EXPECT_FALSE(Pat1.match("fooD.cppm"));
    EXPECT_FALSE(Pat1.match("BarfooD.cppm"));
    EXPECT_FALSE(Pat1.match("foofooD.cppm"));

    PATDEF(Pat2, "proj/{build*,include,src}/*.{cc,cpp,h,hpp}")
    EXPECT_TRUE(Pat2.match("proj/include/foo.cc"));
    EXPECT_TRUE(Pat2.match("proj/include/bar.cpp"));
    EXPECT_FALSE(Pat2.match("proj/include/xxx/yyy/zzz/foo.cc"));
    EXPECT_TRUE(Pat2.match("proj/build-yyy/foo.h"));
    EXPECT_TRUE(Pat2.match("proj/build-xxx/foo.cpp"));
    EXPECT_TRUE(Pat2.match("proj/build/foo.cpp"));
    EXPECT_FALSE(Pat2.match("proj/build-xxx/xxx/yyy/zzz/foo.cpp"));

    PATDEF(Pat3, "*.{html,js}")
    EXPECT_TRUE(Pat3.match("foo.js"));
    EXPECT_TRUE(Pat3.match("foo.html"));
    EXPECT_FALSE(Pat3.match("folder/foo.js"));
    EXPECT_FALSE(Pat3.match("/node_modules/foo.js"));
    EXPECT_FALSE(Pat3.match("foo.jss"));
    EXPECT_FALSE(Pat3.match("some.js/test"));

    PATDEF(Pat4, "*.{html}")
    EXPECT_TRUE(Pat4.match("foo.html"));
    EXPECT_FALSE(Pat4.match("foo.js"));
    EXPECT_FALSE(Pat4.match("folder/foo.js"));
    EXPECT_FALSE(Pat4.match("/node_modules/foo.js"));
    EXPECT_FALSE(Pat4.match("foo.jss"));
    EXPECT_FALSE(Pat4.match("some.js/test"));

    PATDEF(Pat5, "{node_modules,testing}")
    EXPECT_TRUE(Pat5.match("node_modules"));
    EXPECT_TRUE(Pat5.match("testing"));
    EXPECT_FALSE(Pat5.match("node_module"));
    EXPECT_FALSE(Pat5.match("dtesting"));

    PATDEF(Pat6, "**/{foo,bar}")
    EXPECT_TRUE(Pat6.match("foo"));
    EXPECT_TRUE(Pat6.match("bar"));
    EXPECT_TRUE(Pat6.match("test/foo"));
    EXPECT_TRUE(Pat6.match("test/bar"));
    EXPECT_TRUE(Pat6.match("other/more/foo"));
    EXPECT_TRUE(Pat6.match("other/more/bar"));
    EXPECT_TRUE(Pat6.match("/foo"));
    EXPECT_TRUE(Pat6.match("/bar"));
    EXPECT_TRUE(Pat6.match("/test/foo"));
    EXPECT_TRUE(Pat6.match("/test/bar"));
    EXPECT_TRUE(Pat6.match("/other/more/foo"));
    EXPECT_TRUE(Pat6.match("/other/more/bar"));

    PATDEF(Pat7, "{foo,bar}/**")
    EXPECT_TRUE(Pat7.match("foo"));
    EXPECT_TRUE(Pat7.match("bar"));
    EXPECT_TRUE(Pat7.match("bar/"));
    EXPECT_TRUE(Pat7.match("foo/test"));
    EXPECT_TRUE(Pat7.match("bar/test"));
    EXPECT_TRUE(Pat7.match("bar/test/"));
    EXPECT_TRUE(Pat7.match("foo/other/more"));
    EXPECT_TRUE(Pat7.match("bar/other/more"));
    EXPECT_TRUE(Pat7.match("bar/other/more/"));

    PATDEF(Pat8, "{**/*.d.ts,**/*.js}")
    EXPECT_TRUE(Pat8.match("foo.js"));
    EXPECT_TRUE(Pat8.match("testing/foo.js"));
    EXPECT_TRUE(Pat8.match("/testing/foo.js"));
    EXPECT_TRUE(Pat8.match("foo.d.ts"));
    EXPECT_TRUE(Pat8.match("testing/foo.d.ts"));
    EXPECT_TRUE(Pat8.match("/testing/foo.d.ts"));
    EXPECT_FALSE(Pat8.match("foo.d"));
    EXPECT_FALSE(Pat8.match("testing/foo.d"));
    EXPECT_FALSE(Pat8.match("/testing/foo.d"));

    PATDEF(Pat9, "{**/*.d.ts,**/*.js,path/simple.jgs}")
    EXPECT_TRUE(Pat9.match("foo.js"));
    EXPECT_TRUE(Pat9.match("testing/foo.js"));
    EXPECT_TRUE(Pat9.match("/testing/foo.js"));
    EXPECT_TRUE(Pat9.match("path/simple.jgs"));
    EXPECT_FALSE(Pat9.match("/path/simple.jgs"));

    PATDEF(Pat10, "{**/*.d.ts,**/*.js,foo.[0-9]}")
    EXPECT_TRUE(Pat10.match("foo.5"));
    EXPECT_TRUE(Pat10.match("foo.8"));
    EXPECT_FALSE(Pat10.match("bar.5"));
    EXPECT_FALSE(Pat10.match("foo.f"));
    EXPECT_TRUE(Pat10.match("foo.js"));

    PATDEF(Pat11, "prefix/{**/*.d.ts,**/*.js,foo.[0-9]}")
    EXPECT_TRUE(Pat11.match("prefix/foo.5"));
    EXPECT_TRUE(Pat11.match("prefix/foo.8"));
    EXPECT_FALSE(Pat11.match("prefix/bar.5"));
    EXPECT_FALSE(Pat11.match("prefix/foo.f"));
    EXPECT_TRUE(Pat11.match("prefix/foo.js"));
}

TEST_CASE(WildGlob) {
    PATDEF(Pat1, "**/*")
    EXPECT_TRUE(Pat1.match("foo"));
    EXPECT_TRUE(Pat1.match("foo/bar/baz"));

    PATDEF(Pat2, "**/[0-9]*")
    EXPECT_TRUE(Pat2.match("114514foo"));
    EXPECT_FALSE(Pat2.match("foo/bar/baz/xxx/yyy/zzz"));
    EXPECT_FALSE(Pat2.match("foo/bar/baz/xxx/yyy/zzz114514"));
    EXPECT_TRUE(Pat2.match("foo/bar/baz/xxx/yyy/114514"));
    EXPECT_TRUE(Pat2.match("foo/bar/baz/xxx/yyy/114514zzz"));

    PATDEF(Pat3, "**/*[0-9]")
    EXPECT_TRUE(Pat3.match("foo5"));
    EXPECT_FALSE(Pat3.match("foo/bar/baz/xxx/yyy/zzz"));
    EXPECT_TRUE(Pat3.match("foo/bar/baz/xxx/yyy/zzz114514"));

    PATDEF(Pat4, "**/include/test/*.{cc,hh,c,h,cpp,hpp}")
    EXPECT_TRUE(Pat4.match("include/test/aaa.cc"));
    EXPECT_TRUE(Pat4.match("/include/test/aaa.cc"));
    EXPECT_TRUE(Pat4.match("xxx/yyy/include/test/aaa.cc"));
    EXPECT_TRUE(Pat4.match("include/foo/bar/baz/include/test/bbb.hh"));
    EXPECT_TRUE(Pat4.match("include/include/include/include/include/test/bbb.hpp"));

    PATDEF(Pat5, "**include/test/*.{cc,hh,c,h,cpp,hpp}")
    EXPECT_TRUE(Pat5.match("include/test/fff.hpp"));
    EXPECT_TRUE(Pat5.match("xxx-yyy-include/test/fff.hpp"));
    EXPECT_TRUE(Pat5.match("xxx-yyy-include/test/.hpp"));
    EXPECT_TRUE(Pat5.match("/include/test/aaa.cc"));
    EXPECT_TRUE(Pat5.match("include/foo/bar/baz/include/test/bbb.hh"));

    PATDEF(Pat6, "**/*foo.{c,cpp}")
    EXPECT_TRUE(Pat6.match("bar/foo.cpp"));
    EXPECT_TRUE(Pat6.match("bar/barfoo.cpp"));
    EXPECT_TRUE(Pat6.match("/foofoo.cpp"));
    EXPECT_TRUE(Pat6.match("foo/foo/foo/foo/foofoo.cpp"));
    EXPECT_TRUE(Pat6.match("foofoo.cpp"));
    EXPECT_TRUE(Pat6.match("barfoo.cpp"));
    EXPECT_TRUE(Pat6.match("foo.cpp"));

    PATDEF(Pat7, "**")
    EXPECT_TRUE(Pat7.match("foo"));
    EXPECT_TRUE(Pat7.match("foo/bar/baz"));

    PATDEF(Pat8, "x/**")
    EXPECT_TRUE(Pat8.match("x/"));
    EXPECT_TRUE(Pat8.match("x/foo/bar/baz"));
    EXPECT_TRUE(Pat8.match("x"));

    PATDEF(Pat9, "**/x")
    EXPECT_TRUE(Pat9.match("x"));
    EXPECT_TRUE(Pat9.match("/x"));
    EXPECT_TRUE(Pat9.match("/x/x/x/x/x"));

    PATDEF(Pat10, "**/*")
    EXPECT_TRUE(Pat10.match("foo"));
    EXPECT_TRUE(Pat10.match("foo/bar"));
    EXPECT_TRUE(Pat10.match("foo/bar/baz"));

    PATDEF(Pat11, "**/*.{cc,cpp}")
    EXPECT_TRUE(Pat11.match("foo/bar/baz.cc"));
    EXPECT_TRUE(Pat11.match("foo/foo/foo.cpp"));
    EXPECT_TRUE(Pat11.match("foo/bar/.cc"));

    PATDEF(Pat12, "**/*?.{cc,cpp}")
    EXPECT_TRUE(Pat12.match("foo/bar/baz/xxx/yyy/zzz/aaa.cc"));
    EXPECT_TRUE(Pat12.match("foo/bar/baz/xxx/yyy/zzz/a.cc"));
    EXPECT_FALSE(Pat12.match("foo/bar/baz/xxx/yyy/zzz/.cc"));

    PATDEF(Pat13, "**/?*.{cc,cpp}")
    EXPECT_TRUE(Pat13.match("foo/bar/baz/xxx/yyy/zzz/aaa.cc"));
    EXPECT_TRUE(Pat13.match("foo/bar/baz/xxx/yyy/zzz/a.cc"));
    EXPECT_FALSE(Pat13.match("foo/bar/baz/xxx/yyy/zzz/.cc"));

    PATDEF(Pat14, "**/*[0-9]")
    EXPECT_TRUE(Pat14.match("foo5"));
    EXPECT_FALSE(Pat14.match("foo/bar/baz/xxx/yyy/zzz"));
    EXPECT_TRUE(Pat14.match("foo/bar/baz/xxx/yyy/zzz114514"));

    PATDEF(Pat15, "**/*")
    EXPECT_TRUE(Pat15.match("foo"));
    EXPECT_TRUE(Pat15.match("foo/bar/baz"));

    PATDEF(Pat16, "**/*.js")
    EXPECT_TRUE(Pat16.match("foo.js"));
    EXPECT_TRUE(Pat16.match("/foo.js"));
    EXPECT_TRUE(Pat16.match("folder/foo.js"));
    EXPECT_TRUE(Pat16.match("/node_modules/foo.js"));
    EXPECT_FALSE(Pat16.match("foo.jss"));
    EXPECT_FALSE(Pat16.match("some.js/test"));
    EXPECT_FALSE(Pat16.match("/some.js/test"));

    PATDEF(Pat17, "**/project.json")
    EXPECT_TRUE(Pat17.match("project.json"));
    EXPECT_TRUE(Pat17.match("/project.json"));
    EXPECT_TRUE(Pat17.match("some/folder/project.json"));
    EXPECT_TRUE(Pat17.match("/some/folder/project.json"));
    EXPECT_FALSE(Pat17.match("some/folder/file_project.json"));
    EXPECT_FALSE(Pat17.match("some/folder/fileproject.json"));
    EXPECT_FALSE(Pat17.match("some/rrproject.json"));

    PATDEF(Pat18, "test/**")
    EXPECT_TRUE(Pat18.match("test"));
    EXPECT_TRUE(Pat18.match("test/foo"));
    EXPECT_TRUE(Pat18.match("test/foo/"));
    EXPECT_TRUE(Pat18.match("test/foo.js"));
    EXPECT_TRUE(Pat18.match("test/other/foo.js"));
    EXPECT_FALSE(Pat18.match("est/other/foo.js"));

    PATDEF(Pat19, "**")
    EXPECT_TRUE(Pat19.match("/"));
    EXPECT_TRUE(Pat19.match("foo.js"));
    EXPECT_TRUE(Pat19.match("folder/foo.js"));
    EXPECT_TRUE(Pat19.match("folder/foo/"));
    EXPECT_TRUE(Pat19.match("/node_modules/foo.js"));
    EXPECT_TRUE(Pat19.match("foo.jss"));
    EXPECT_TRUE(Pat19.match("some.js/test"));

    PATDEF(Pat20, "test/**/*.js")
    EXPECT_TRUE(Pat20.match("test/foo.js"));
    EXPECT_TRUE(Pat20.match("test/other/foo.js"));
    EXPECT_TRUE(Pat20.match("test/other/more/foo.js"));
    EXPECT_FALSE(Pat20.match("test/foo.ts"));
    EXPECT_FALSE(Pat20.match("test/other/foo.ts"));
    EXPECT_FALSE(Pat20.match("test/other/more/foo.ts"));

    PATDEF(Pat21, "**/**/*.js")
    EXPECT_TRUE(Pat21.match("foo.js"));
    EXPECT_TRUE(Pat21.match("/foo.js"));
    EXPECT_TRUE(Pat21.match("folder/foo.js"));
    EXPECT_TRUE(Pat21.match("/node_modules/foo.js"));
    EXPECT_FALSE(Pat21.match("foo.jss"));
    EXPECT_FALSE(Pat21.match("some.js/test"));

    PATDEF(Pat22, "**/node_modules/**/*.js")
    EXPECT_FALSE(Pat22.match("foo.js"));
    EXPECT_FALSE(Pat22.match("folder/foo.js"));
    EXPECT_TRUE(Pat22.match("node_modules/foo.js"));
    EXPECT_TRUE(Pat22.match("/node_modules/foo.js"));
    EXPECT_TRUE(Pat22.match("node_modules/some/folder/foo.js"));
    EXPECT_TRUE(Pat22.match("/node_modules/some/folder/foo.js"));
    EXPECT_FALSE(Pat22.match("node_modules/some/folder/foo.ts"));
    EXPECT_FALSE(Pat22.match("foo.jss"));
    EXPECT_FALSE(Pat22.match("some.js/test"));

    PATDEF(Pat23, "{**/node_modules/**,**/.git/**,**/bower_components/**}")
    EXPECT_TRUE(Pat23.match("node_modules"));
    EXPECT_TRUE(Pat23.match("/node_modules"));
    EXPECT_TRUE(Pat23.match("/node_modules/more"));
    EXPECT_TRUE(Pat23.match("some/test/node_modules"));
    EXPECT_TRUE(Pat23.match("/some/test/node_modules"));
    EXPECT_TRUE(Pat23.match("bower_components"));
    EXPECT_TRUE(Pat23.match("bower_components/more"));
    EXPECT_TRUE(Pat23.match("/bower_components"));
    EXPECT_TRUE(Pat23.match("some/test/bower_components"));
    EXPECT_TRUE(Pat23.match("/some/test/bower_components"));
    EXPECT_TRUE(Pat23.match(".git"));
    EXPECT_TRUE(Pat23.match("/.git"));
    EXPECT_TRUE(Pat23.match("some/test/.git"));
    EXPECT_TRUE(Pat23.match("/some/test/.git"));
    EXPECT_FALSE(Pat23.match("tempting"));
    EXPECT_FALSE(Pat23.match("/tempting"));
    EXPECT_FALSE(Pat23.match("some/test/tempting"));
    EXPECT_FALSE(Pat23.match("/some/test/tempting"));

    PATDEF(Pat24, "{**/package.json,**/project.json}")
    EXPECT_TRUE(Pat24.match("package.json"));
    EXPECT_TRUE(Pat24.match("/package.json"));
    EXPECT_FALSE(Pat24.match("xpackage.json"));
    EXPECT_FALSE(Pat24.match("/xpackage.json"));

    PATDEF(Pat25, "some/**/*.js")
    EXPECT_TRUE(Pat25.match("some/foo.js"));
    EXPECT_TRUE(Pat25.match("some/folder/foo.js"));
    EXPECT_FALSE(Pat25.match("something/foo.js"));
    EXPECT_FALSE(Pat25.match("something/folder/foo.js"));

    PATDEF(Pat26, "some/**/*")
    EXPECT_TRUE(Pat26.match("some/foo.js"));
    EXPECT_TRUE(Pat26.match("some/folder/foo.js"));
    EXPECT_FALSE(Pat26.match("something/foo.js"));
    EXPECT_FALSE(Pat26.match("something/folder/foo.js"));
}

};  // TEST_SUITE(GlobPattern)

}  // namespace

}  // namespace kota
