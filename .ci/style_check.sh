#!/usr/bin/env bash

RETURN=0
CLANG_FORMAT=$(which clang-format)
DIFF=$(which colordiff)

FILES=$(git ls-tree --full-tree -r HEAD | egrep "\.(c|h)\$" | cut -f 2)
for FILE in ${FILES}; do
    nf=`git checkout-index --temp $FILE | cut -f 1`
    tempdir=`mktemp -d` || exit 1
    newfile=`mktemp ${tempdir}/${nf}.XXXXXX` || exit 1
    basename=`basename $FILE`

    source="${tempdir}/${basename}"
    mv $nf $source
    cp .clang-format $tempdir
    
    $CLANG_FORMAT $source > $newfile 2>> /dev/null
    $DIFF -u -p -B --label="modified $FILE" --label="expected coding style" \
          "${source}" "${newfile}"
    r=$?
    rm -rf "${tempdir}"
    if [ $r != 0 ] ; then
        echo "[!] $FILE does not follow the consistent coding style." >&2
        RETURN=1
    fi
done

if [ $RETURN -eq 0 ] ; then 
    echo "pass all style check test!" >&2
fi
exit $RETURN
