# Translations

English source strings are wrapped with KI18n in the C++ code. Spanish translations live in
`po/es/linuxgitshell.po` and are compiled to a Gettext catalog during a normal build.

After changing visible strings, regenerate or merge the catalog with standard Gettext tools, review
the translations manually, then build and test:

```bash
xgettext --from-code=UTF-8 --language=C++ --package-name=LinuxGitShell \
  --keyword=i18n --keyword=i18nd:2 \
  -o po/linuxgitshell.pot gui/app/*.cpp integrations/dolphin/contextmenu/*.cpp
msgmerge --update po/es/linuxgitshell.po po/linuxgitshell.pot
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Do not build sentences by concatenating translated fragments. Add translator context when meaning is
ambiguous and check layouts with longer strings.
Plugin strings use the explicit `linuxgitshell` domain because Dolphin owns the host process. The
translation test must cover new plugin strings as well as application strings.
