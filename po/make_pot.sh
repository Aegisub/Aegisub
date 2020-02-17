#!/bin/sh

maybe_append() {
  while read -r msg; do
    msgfile=$(echo $msg | cut -d'|' -f1)
    msgline=$(echo $msg | cut -d'|' -f2)
    msgid=$(echo $msg | cut -d'|' -f3-)

    if ! grep -Fq "msgid $msgid" aegisub.pot; then
      echo "\n#: $msgfile:$msgline\nmsgid $msgid\nmsgstr \"\"\n" >> aegisub.pot
    fi
  done
}

find ../src ../src/command -name \*.cpp -o -name \*.h \
  | xgettext --files-from=- -o - --c++ -k_ -kSTR_MENU -kSTR_DISP -kSTR_HELP -kfmt_tl -kfmt_plural:2,3 -kwxT -kwxPLURAL:1,2 \
  | sed 's/SOME DESCRIPTIVE TITLE./Aegisub 3.2/' \
  | sed 's/YEAR/2005-2014/' \
  | sed "s/THE PACKAGE'S COPYRIGHT HOLDER/Rodrigo Braz Monteiro, Niels Martin Hansen, Thomas Goyne et. al./" \
  | sed 's/PACKAGE/Aegisub/' \
  | sed 's/VERSION/3.2.0/' \
  | sed 's/FIRST AUTHOR <EMAIL@ADDRESS>/Niels Martin Hansen <nielsm@aegisub.org>/' \
  | sed 's/CHARSET/UTF-8/' \
  > aegisub.pot

sed '/"text"/!d;s/^.*"text" : \("[^"]\+"\).*$/default_menu.json|0|\1/' ../src/libresrc/default_menu.json \
  | maybe_append

sed '/"text"/!d;s/^.*"text" : \("[^"]\+"\).*$/default_menu.json|0|\1/' ../src/libresrc/osx/default_menu.json \
  | maybe_append

grep '"[A-Za-z ]\+" : {' -n ../src/libresrc/default_hotkey.json \
  | sed 's/^\([0-9]\+:\).*\("[^"]\+"\).*$/default_hotkey.json|\1|\2/' \
  | maybe_append

find ../automation -name *.lua \
  | xargs grep tr\"[^\"]\*\" -o -n \
  | sed 's/\(.*\):\([0-9]\+\):tr\(".*"\)/\1|\2|\3/' \
  | sed 's/\\/\\\\\\\\/g' \
  | maybe_append

find ../automation -name *.moon \
  | xargs grep tr\"[^\"]\*\" -o -n \
  | sed 's/\(.*\):\([0-9]\+\):tr\(".*"\)/\1|\2|\3/' \
  | sed 's/\\/\\\\\\\\/g' \
  | maybe_append

for i in 'Name' 'GenericName' 'Comment' 'Keywords'
do
  grep ^_$i -n ../packages/desktop/aegisub.desktop.template.in \
    | sed 's/\([0-9]\+\):[^=]\+=\(.*\)$/aegisub.desktop|\1|"\2"/' \
    | maybe_append
done

if which xmlstarlet >/dev/null 2>&1 && which jq >/dev/null 2>&1; then
  for i in 'name' 'summary' 'p' 'li' 'caption'; do
    xmlstarlet sel -t -v "//_$i" ../packages/desktop/aegisub.appdata.xml.template.in | jq -R .
  done | nl -v0 -w1 -s'|' | sed -re 's/^/aegisub.appdata.xml|/' | maybe_append
fi

grep '^_[A-Za-z0-9]*=.*' ../packages/win_installer/fragment_strings.iss.in | while read line
do
  echo "$line" \
    | sed 's/[^=]*=\(.*\)/packages\/win_installer\/fragment_strings.iss|1|"\1"/' \
    | maybe_append
done
