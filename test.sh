#!/bin/bash

check_and_cleanup(){
  if [ "$1" = "nodash" ]
  then
      cp "prova-no-dash.csv" "prova.csv"
  elif [ "$1" = "timestamp" ]
  then
      cp "prova-timestamp.csv" "prova.csv"
  elif [ "$1" = "timestamp-no-dash" ]
  then
      cp "prova-timestamp-no-dash.csv" "prova.csv"
  else
      cp "prova-timestamp.csv" "prova.csv"
  fi
  check
}

check(){
  head -4 prova.csv
}

cp "prova-timestamp.csv" "prova.csv"

echo "==== (1) TEST di BASE ===="
echo "Il file prima di modificare"
check_and_cleanup timestamp
./simple_file_read_mmap prova.csv
echo "Risultato: "
check

echo "=== (2) TEST con formattazione esplicita ===="
echo "Il file prima di modificare"
check_and_cleanup timestamp
./simple_file_read_mmap prova.csv -yyyyMMdd
echo "Risultato"
check

echo "=== (3) TEST con TIMESTAMP per inserire nel database ===="
echo "Il file prima di modificare"
check_and_cleanup timestamp
./simple_file_read_mmap prova.csv -yyyyMMddhhmm
echo "Risultato"
check

echo "=== (4) TEST con DATE senza trattini ==="
echo "Il file prima di modificare"
check_and_cleanup nodash
./simple_file_read_mmap prova.csv -no-dash
echo "Risultato"
check

echo "=== (5) TEST con TIMESTAMP senza trattini"
echo "Il file prima di modificare"
check_and_cleanup "timestamp-no-dash"
./simple_file_read_mmap prova.csv -yyyyMMddhhmm -no-dash
echo "Risultato"
check