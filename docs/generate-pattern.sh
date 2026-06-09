#! /bin/bash

set -e

d0="$(cd "$(dirname "$0")" && pwd)"
outdir=.

while (($# > 0)); do
	case "$1" in
		-outdir)
			outdir="$2"
			shift 2;;
		*)
			echo "Error: unknown argument: $1" >&2
			exit 1;;
	esac
done

videogen="$d0/../tool/videogen.py"
avoffsetgen="$d0/../tool/avoffsetgen.py"

mkdir -p "$outdir"
tmpbase="$outdir/.avoffsetgen-work"
rm -rf "$tmpbase"
mkdir -p "$tmpbase"
trap 'rm -rf "$tmpbase"' EXIT

"$videogen" --vr 60         --ar 48000 'q=4,f=884,c=2' -o "$outdir/sync-pattern-6000.mp4"
"$videogen" --vr 60000/1001 --ar 48000 'q=4,f=884,c=2' -o "$outdir/sync-pattern-5994.mp4"
"$videogen" --vr 50         --ar 48000 'q=4,f=884,c=2' -o "$outdir/sync-pattern-5000.mp4"
"$videogen" --vr 24         --ar 48000 'q=2,f=884,c=2' -o "$outdir/sync-pattern-2400.mp4"
"$videogen" --vr 24000/1001 --ar 48000 'q=2,f=884,c=2' -o "$outdir/sync-pattern-2398.mp4"
"$videogen" --vr 30         --ar 44100 'q=4,f=884,c=2' -o "$outdir/sync-pattern-3000-small.mp4" --size 320x180

avoffset_common=(
	--ar 48000
	--duration 30
	--interval 2
	--audio-codec pcm_s16le
	--video-codec libx264
	--no-intra-video
	--b-frames 0
)

"$avoffsetgen" --vr 60         "${avoffset_common[@]}" -w "$tmpbase/6000" -o "$outdir/av-offset-pattern-6000.mov"
"$avoffsetgen" --vr 60000/1001 "${avoffset_common[@]}" -w "$tmpbase/5994" -o "$outdir/av-offset-pattern-5994.mov"
"$avoffsetgen" --vr 50         "${avoffset_common[@]}" -w "$tmpbase/5000" -o "$outdir/av-offset-pattern-5000.mov"
"$avoffsetgen" --vr 30         "${avoffset_common[@]}" -w "$tmpbase/3000" -o "$outdir/av-offset-pattern-3000.mov"
"$avoffsetgen" --vr 30000/1001 "${avoffset_common[@]}" -w "$tmpbase/2997" -o "$outdir/av-offset-pattern-2997.mov"
"$avoffsetgen" --vr 24         "${avoffset_common[@]}" -w "$tmpbase/2400" -o "$outdir/av-offset-pattern-2400.mov"
"$avoffsetgen" --vr 24000/1001 "${avoffset_common[@]}" -w "$tmpbase/2398" -o "$outdir/av-offset-pattern-2398.mov"
