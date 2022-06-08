#!/bin/bash

INDIR=$1
OUTDIR=$2
PROG=$3
shift 3
PARGS=${*:--oOPFS}

INPUTS=`seq 1 11` # seq 1 11
ALGOS=" f r c e a w" # f r c e a w
FRAMES="16 32"

for I in ${INPUTS}; do
   for A in ${ALGOS}; do
      for F in ${FRAMES}; do
        OUTF="${OUTDIR}/out${I}_${F}_${A}"
        echo "${PROG} -f${F} -a${A} ${PARGS} ${INDIR}/in${I} ${INDIR}/rfile > ${OUTF}"
        ${PROG} -f${F} -a${A} ${PARGS} ${INDIR}/in${I} ${INDIR}/rfile > ${OUTF}
        OUTPUT=`egrep "^TOTAL" ${OUTF}`
        echo "in${I}_${F}_${A}: ${OUTPUT}"
      done
   done
done

