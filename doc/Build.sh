#! /bin/bash

set -e
set -o pipefail

if [ "x$1" != 'xdoxy' ] && [ -e build/xml ] ; then
  echo "Doxygen not rerun: 'doxy' was not provided as an argument"
else
  set -x
  rm -rf build/xml source/api/
  (cd source; doxygen 2>&1; cd ..) | (grep -v "is not documented." || true) # XXXXX Reduce the verbosity for now
  set +x
fi

PYTHONPATH=../lib sphinx-build -M html source build ${SPHINXOPTS} 2>&1

set +x

set +e # Don't fail
if [ -e /usr/bin/linkchecker ] ; then
    linkchecker --no-status -o csv --ignore-url='.*\.css$' --ignore-url=build/html/_modules build/html \
     | grep -v '^#' \
     | grep -v 'urlname;parentname;baseref;result;warningstring'
  echo "done."
else
  echo "Install linkchecker to have it executed when you build the doc."
fi
