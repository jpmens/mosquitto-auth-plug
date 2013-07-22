#! /bin/sh

# tests.sh: This script will run tests for cdb.
# Execute with ./tests.sh ./cdb
# (first arg if present gives path to cdb tool to use, default is `cdb').
#
# This file is a part of tinycdb package by Michael Tokarev, mjt@corpit.ru.
# Public domain.

case "$1" in
  "") cdb=cdb ;;
  *) cdb="$1" ;;
esac

do_csum() {
  echo checksum may fail if no md5sum program
  md5sum $1 | sed -e 's|[ 	].*||' -e 'y|[ABCDEF]|[abcdef]|'
}

rm -f 1.cdb 1a.cdb

echo Create simple db
echo "+3,4:one->here
+1,1:a->b
+1,3:b->abc
+3,4:one->also

" | $cdb -c 1.cdb
echo $?
do_csum 1.cdb

echo Dump simple db
$cdb -d 1.cdb
echo $?

echo Stats for simple db
$cdb -s 1.cdb
echo $?

echo "Query simple db (two records match)"
$cdb -q 1.cdb one
echo "
$?"

echo Query for non-existed key
$cdb -q 1.cdb none
echo $?

echo Doing 600 repeated records
(
 for i in 0 1 2 3 4 5 ; do
  for j in 0 1 2 3 4 5 6 7 8 9 ; do
   for k in 0 1 2 3 4 5 6 7 8 9 ; do
    echo "+1,3:a->$i$j$k"
   done
  done
 done
 echo "+1,5:b->other"
 echo
) | $cdb -c 1.cdb
echo $?
do_csum 1.cdb
echo cdb stats should show 601 record
$cdb -s 1.cdb
echo $?

echo Querying key
$cdb -q 1.cdb b
echo "
"$?

echo Dumping and re-creating db
$cdb -d 1.cdb | $cdb -c 1a.cdb
echo $?
cmp 1.cdb 1a.cdb

$cdb -d -m 1.cdb | $cdb -c -m 1a.cdb
echo $?
cmp 1.cdb 1a.cdb

echo Handling large key size
echo "+123456789012,1:" | $cdb -c 1.cdb
echo $?

echo Handling large value size
echo "+1,123456789012:" | $cdb -c 1.cdb
echo $?

echo "Handling invalid input format (short file)"
echo "+10,10:" | $cdb -c 1.cdb
echo $?

echo Creating db with eol in key and value
echo "+2,2:a
->b

" | $cdb -c 1.cdb
echo $?
do_csum 1.cdb

echo Querying key-value with eol
$cdb -q 1.cdb "a
"
echo $?

echo Handling file size limits
(
 ulimit -f 4
 trap '' 25
 (
  for i in 0 1 2 3 4 5 6 7 8 9 ; do
   for j in 0 1 2 3 4 5 6 7 8 9 ; do
    for k in 0 1 2 3 4 5 6 7 8 9 ; do
     echo "+4,4:k$i$j$k->v$i$j$k"
    done
   done
  done
  echo
 ) | $cdb -c 1.cdb
 echo $?
)

if false ; then # does not work for now, bugs in libc
echo Handling oom condition
(
 for i0 in 0 1 2 3 4 5 6 7 8 9 ; do
  for i1 in 0 1 2 3 4 5 6 7 8 9 ; do
   for i2 in 0 1 2 3 4 5 6 7 8 9 ; do
    for i3 in 0 1 2 3 4 5 6 7 8 9 ; do
     for i4 in 0 1 2 3 4 5 6 7 8 9 ; do
      echo "+5,0:$i0$i1$i2$i3$i4->"
     done
    done
   done
  done
 done
 echo
) | (ulimit -v 1900; $cdb -c 1.cdb)
echo $?
fi

rm -rf 1.cdb 1a.cdb 1.cdb.tmp
exit 0
