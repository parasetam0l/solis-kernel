#!/bin/bash

## Functions
function write_hex_to_4bytes_binary()
{
	HEX=$1

	HEX=${HEX#0x}
	NUM=$((8-${#HEX}))

	ZERO="00000000"
	SUB=${ZERO:0:$NUM}

	HEX=$SUB$HEX

	for str in $(echo $HEX | sed 's/../& /g' | rev); do
		str=$(echo -en $str | rev)
		echo -en "\x$str"
	done > $2
}

function write_to_4bytes_binary()
{
	HEX=`echo "obase=16; $1" | bc`

	write_hex_to_4bytes_binary $HEX $2
}

function write_to_padding_binary()
{
	rm -f padding

	PAD_SIZE=$(($(($PAD - $(($1 % $PAD)))) % $PAD))
	if [ $PAD_SIZE -gt 0 ]; then
		dd if=/dev/zero of=./padding bs=1 count=$PAD_SIZE 2>/dev/zero
	else
		touch padding
	fi

	if [ "${VERBOSE}" -eq "1" ]; then
		echo -en " | PAD: $PAD_SIZE[B]\n"
	fi
}

function get_dtb_size()
{
	SIZE=`du -b $1 | awk '{print $1}'`
	PAD_SIZE=$(($(($PAD - $(($SIZE % $PAD)))) % $PAD))
	DTB_SIZE=$(($SIZE + $PAD_SIZE))
}

function help() {
	echo "exynos_dtbtool.sh [OPTIONS] -o <output file> <input DTB path>"
	echo "  options:"
	echo "  --output-file/-o    output file"
	echo "  --dtc-path/-p       path to dtc"
	echo "  --page-size/-s      page size in bytes"
	echo "  --verbose/-v        verbose"
	echo "  --help/-h           this help screen"
	exit 0
}


## Defines
OUT="merged-dtb"
OUT_TMP="multi.tmp"

OUT_DIR="./arch/arm/boot"
DTC_PATH=scripts/dtc

EXYNOS_MAGIC="EXYN"
EXYNOS_VERSION=1
EXYNOS_TAG="samsung,exynos-id"

ENDOFHEADER=0

let MAX_PAD_SIZE=1024*1024
PAD=2048
VERBOSE=0

if [ "$#" -eq "0" ]; then
	help
fi


## Input parameters
params="$(getopt -o o:p:s:vh -l output-file:,dtc-path:,page-size:,verbose,help --name "$0" -- "$@")"
eval set -- "$params"
while true
do
	case "$1" in
		-o|--output-file)
			OUTPUT_FILE=$2
			shift 2
			OUT_DIR=`dirname $OUTPUT_FILE`
			if [ ! -d "$OUT_DIR" ]; then
				echo "invalid output path '$OUT_DIR'."
				exit 1
			fi
			OUT=${OUTPUT_FILE#${OUT_DIR}}
			OUT=${OUT//\//}
			OUT_TMP=${OUT}.tmp
			;;
		-p|--dtc-path)
			DTC_PATH=$2
			shift 2
			;;
		-s|--page-size)
			PAD=$2
			shift 2
			R=${PAD#-}
			R=${R//[0-9]/}
			# page size should be number, and larger than 0, and smaller than 1024*1024
			if [ ! -z "$R" ] || [ "$PAD" -lt "0" ] || [ "$PAD" -gt "$MAX_PAD_SIZE" ]; then
				echo "invalid page size."
				exit 1
			fi
			;;
		-v|--verbose)
			VERBOSE=1
			shift
			;;
		-h|--help)
			help
			;;
		--)
			shift
			break
			;;
		*)
			help
			;;
	esac
done

if [ "$#" -eq "0" ]; then
	help
fi

DTS_DIR=$1
DTS_DIR=${DTS_DIR%\/}
if [ ! -d "$DTS_DIR" ]; then
	echo "invalid DTB input path '$DTS_DIR'."
	exit 1
else
	DTB=(`find ${DTS_DIR} -name "*.dtb"`)
	for i in ${DTB[*]}; do
		if [ -e $i ]; then
			TAG_CHECK=`${DTC_PATH}/dtc -I dtb -O dts $i 2>/dev/null | grep "${EXYNOS_TAG}"`
			if [ "z${TAG_CHECK}" == "z" ]; then
				# no tag in the dtb file
				DTB=(${DTB[@]/$i})
			fi
		fi
	done
fi

if [ ! -f "$DTC_PATH/dtc" ]; then
	echo "no DTC file in '$DTC_PATH'."
	exit 1
fi

if [ "${VERBOSE}" -eq "1" ]; then
	echo "DTB input path is '${DTS_DIR}'"
	echo "output path is '${OUT_DIR}', output file is '${OUT}'"
	echo "page size is '${PAD}'"
fi


## Header
rm -f $OUT
rm -f $OUT_TMP
touch $OUT_TMP

DTB_CNT=${#DTB[@]}
HEADER_SIZE=$((12 + 20 * $DTB_CNT + 4))
DTB_OFFSET=$PAD

if [ "${VERBOSE}" -eq "1" ]; then
	echo -en " *HEADER "
	echo -en "$HEADER_SIZE[B]\n"
fi

echo -en $EXYNOS_MAGIC > $OUT
cat $OUT >> $OUT_TMP
write_to_4bytes_binary $EXYNOS_VERSION $OUT
cat $OUT >> $OUT_TMP
write_to_4bytes_binary $DTB_CNT $OUT
cat $OUT >> $OUT_TMP

for i in ${DTB[*]}; do
	FILE="$i"
	if [ -e $FILE ]; then
		TAG_CHECK=`${DTC_PATH}/dtc -I dtb -O dts $i 2>/dev/null | grep "${EXYNOS_TAG}"`
		TAG_CHECK=${TAG_CHECK#*<}
		TAG_CHECK=${TAG_CHECK%>*}
		CHIPSET=`echo ${TAG_CHECK} | awk '{ print $1 }'`
		PLATFORM=`echo ${TAG_CHECK} | awk '{ print $2 }'`
		REV=`echo ${TAG_CHECK} | awk '{ print $3 }'`

		write_hex_to_4bytes_binary $CHIPSET $OUT
		cat $OUT >> $OUT_TMP

		write_hex_to_4bytes_binary $PLATFORM $OUT
		cat $OUT >> $OUT_TMP

		write_hex_to_4bytes_binary $REV $OUT
		cat $OUT >> $OUT_TMP

		write_to_4bytes_binary $DTB_OFFSET $OUT
		cat $OUT >> $OUT_TMP

		get_dtb_size $FILE
		write_to_4bytes_binary $DTB_SIZE $OUT
		cat $OUT >> $OUT_TMP

		DTB_OFFSET=$(($DTB_OFFSET + $DTB_SIZE))
	else
		echo -en "$i not found.\nexit\n"
		exit -1
	fi
done

write_to_4bytes_binary $ENDOFHEADER $OUT
cat $OUT >> $OUT_TMP

write_to_padding_binary $HEADER_SIZE
cat $OUT_TMP padding > $OUT


## DTB
for i in ${DTB[*]}; do
	FILE="$i"
	if [ -e $FILE ]; then
		NAME=`echo $i`
		SIZE=`du -b $FILE | awk '{print $1}'`

		cat $OUT $FILE > $OUT_TMP
		if [ "${VERBOSE}" -eq "1" ]; then
			echo -en " *$NAME "
			echo -en "$SIZE[B]\n"
		fi

		write_to_padding_binary $SIZE
		cat $OUT_TMP padding > $OUT
	else
		echo -en "$i not found.\nexit\n"
		exit -1
	fi
done


## End
rm -f $OUT_TMP
rm -f padding
rm -f $OUT_DIR/$OUT
mv -f $OUT $OUT_DIR/

S=`du -b $OUT_DIR/$OUT | awk '{print $1}'`
S_K=$(($S/1024))
if [ "${VERBOSE}" -eq "1" ]; then
	echo -en "## OUT: $OUT size: $S[B]; $S_K[KB]\n"
fi
echo "$OUT_DIR/$OUT (size: ${S_K}KB) is created."
