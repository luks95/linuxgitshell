# Translations

English source strings are wrapped with KI18n in the C++ code. Spanish translations live in
`po/es/linuxgitshell.po` and are compiled to a Gettext catalog during a normal build.

After changing visible strings, regenerate or merge the catalog with standard Gettext tools, review
the translations manually, then build and test:

```bash
xgettext --from-code=UTF-8 --keyword=i18n --language=C++ \
  --package-name=LinuxGitShell -o po/linuxgitshell.pot gui/app/*.cpp
msgmerge --update po/es/linuxgitshell.po po/linuxgitshell.pot
cmake --build build -j
```

Do not build sentences by concatenating translated fragments. Add translator context when meaning is
ambiguous and check layouts with longer strings.
