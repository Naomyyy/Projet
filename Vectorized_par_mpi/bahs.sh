#!/bin/bash

# Caminho do executável
PROG="./ant_simu.exe"  # substitua pelo seu binário

# Loop de threads: 2 a 16, de 4 em 4
for N in 2 6 10 14 16
do
    echo "=============================="
    echo "Executando com N THREADS = $N"
    export OMP_NUM_THREADS=$N
    $PROG
    echo "=============================="
    echo ""
done