#!/bin/bash

## Functions
function write_to_4bytes_binary()
{
	HEX=`echo "obase=16; $1" | bc`

	NUM=$((8-${#HEX}))

	ZERO="00000000"
	SUB=${ZERO:0:$NUM}

	HEX=$SUB$HEX

	for str in $(echo $HEX | sed 's/../& /g' | rev); do
		str=$(echo -en $str | rev)
		echo -en "\x$str"
	done > $2
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

	echo -en " | PAD: $PAD_SIZE[B]\n"
}

function help() {
	echo "sprd_mkdzimage.sh [OPTIONS] -o <output file> -k <kernel file>"
	echo "  options:"
	echo "  --output-file/-o    output file"
	echo "  --kernel/-k         kernel file"
	echo "  --devicetree/-d     devicetree file"
	echo "  --help/-h           this help screen"
	exit 0
}


## Defines
OUT="dzImage"
OUT_TMP="dzImage.tmp"
OUT_DIR="./arch/arm/boot"

MAGIC="NZIT"		# 0x54495A4E
KERNEL_ADDR=32768	# 0x00008000
ATAGS_ADDR=31457280	# 0x01e00000

PAD=2048


if [ "$#" -eq "0" ]; then
	help
fi


## Input parameters
params="$(getopt -o o:k:d:h -l output-file:,:kernel:,devicetree:,help --name "$0" -- "$@")"
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
		-k|--kernel)
			KERNEL=$2
			shift 2
			;;
		-d|--devicetree)
			DEVICETREE=$2
			shift 2
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

if [ "z${KERNEL}" == "z" ] || [ "z${DEVICETREE}" == "z" ]; then
	help
fi

if [ ! -e "${KERNEL}" ]; then
	echo -en "kernel '${KERNEL}' not found.\nexit\n"
	exit -1
fi

if [ ! -e "${DEVICETREE}" ]; then
	echo -en "devicetree '${DEVICETREE}' not found.\nexit\n"
	exit -1
fi


## Header
rm -f $OUT
rm -f $OUT_TMP
touch $OUT_TMP

HEADER_SIZE=28

echo -en " *HEADER "
echo -en "$HEADER_SIZE[B]\n"

echo -en $MAGIC > $OUT
cat $OUT >> $OUT_TMP
write_to_4bytes_binary $KERNEL_ADDR $OUT
cat $OUT >> $OUT_TMP

FILE="${KERNEL}"
SIZE=`du -b $FILE | awk '{print $1}'`
write_to_4bytes_binary $SIZE $OUT
cat $OUT >> $OUT_TMP

DTB_ADDR=$(($KERNEL_ADDR + $SIZE))
write_to_4bytes_binary $DTB_ADDR $OUT
cat $OUT >> $OUT_TMP

FILE="${DEVICETREE}"
SIZE=`du -b $FILE | awk '{print $1}'`
write_to_4bytes_binary $SIZE $OUT
cat $OUT >> $OUT_TMP

write_to_4bytes_binary $ATAGS_ADDR $OUT
cat $OUT >> $OUT_TMP
write_to_4bytes_binary $PAD $OUT
cat $OUT >> $OUT_TMP

write_to_padding_binary $HEADER_SIZE
cat $OUT_TMP padding > $OUT


## Kernel Binary
FILE="${KERNEL}"
FILE_PATH=`dirname ${KERNEL}`
FILE_NAME=${FILE#${FILE_PATH}}
FILE_NAME=${FILE_NAME//\//}

echo -en " *${FILE_NAME} "
cat $OUT $FILE > $OUT_TMP

SIZE=`du -b $FILE | awk '{print $1}'`
echo -en "$SIZE[B]\n"

write_to_padding_binary $SIZE
cat $OUT_TMP padding > $OUT


## Devicetree Binary
FILE="${DEVICETREE}"
FILE_PATH=`dirname ${DEVICETREE}`
FILE_NAME=${FILE#${FILE_PATH}}
FILE_NAME=${FILE_NAME//\//}

echo -en " *${FILE_NAME} "
cat $OUT $FILE > $OUT_TMP

SIZE=`du -b $FILE | awk '{print $1}'`
echo -en "$SIZE[B]\n"

write_to_padding_binary $SIZE
cat $OUT_TMP padding > $OUT


## END
rm -f $OUT_TMP
rm -f padding
rm -f $OUT_DIR/$OUT
mv -f $OUT $OUT_DIR/

S=`du -b $OUT_DIR/$OUT | awk '{print $1}'`
S_K=$(($S/1024))
echo -en "## OUT: $OUT size: $S[B]; $S_K[KB]\n"
