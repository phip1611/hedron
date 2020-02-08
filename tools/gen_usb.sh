#!/usr/bin/env bash

set -e -u

die () {
    echo >&2 "ERROR: $@"
    echo "Usage: ./gen_usb.sh IMG_FILE_NAME PATH_TO_NOVA GRUB_CONFIG [MODULE_DIR]"
    exit 1
}

# Returns true, if a program is available.
have_exec() {
    command -v "$1" > /dev/null 2>&1
}

if [ "$#" -eq "0" ] ; then
    die "no parameters provided."
fi

[ "$#" -eq 3 ] || [ "$#" -eq 4 ] || die "invalid number of argument $#."

img_filename=$1
nova_filename=$2
grub_config=$3
grub_modules=${4:-"/usr/lib/grub/x86_64-efi"}

if have_exec grub2-mkrescue; then
    mkrescue=grub2-mkrescue
elif have_exec grub-mkrescue; then
    mkrescue=grub-mkrescue
else
    echo >&2 "No grub-mkrescue found."
    exit 1
fi

tmp_dir=$(mktemp -d -t filesystem.XXXXXX) || die "unable to create temp directory."
trap 'rm -r "$tmp_dir"' EXIT

echo "======== Build details ========"
echo "Image:       $img_filename"
echo "NOVA:        $nova_filename"
echo "Grub config: $grub_config"
echo "Tmp-dir:     $tmp_dir"
echo "Grub mod:    $grub_modules"
echo "==============================="

if [ ! -f "$nova_filename" ] ; then
    die "NOVA file does not exist."
fi

if [ ! -f "$grub_config" ] ; then
    die "Grub config file does not exist."
fi

if [ -f "$img_filename" ] ; then
    rm "$img_filename"
fi

mkdir -p "$tmp_dir/boot/grub"

cp "$nova_filename" "$tmp_dir/boot/hypervisor-x86_64"
cp "$grub_config" "$tmp_dir/boot/grub/grub.cfg"

$mkrescue -d "$grub_modules" -o "$img_filename" "$tmp_dir"

if [ -f "$img_filename" ] ; then
  echo "$img_filename is ready!"
  exit 0
fi

echo >&2 "ERROR: $img_filename should exist now, but does not!"
exit 1
