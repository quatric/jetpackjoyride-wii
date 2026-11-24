#!/bin/sh
# Build the app's data folder from the extracted APK assets (nothing here is redistributable).
# usage: make_sd.sh <assets> <png> <sprites> <out>
#   <assets>  extracted assets/ folder of the APK (xml/, sound/, patterns/, font/, Music_*.ogg)
#   <png>     output of tools/tex2png.py <assets> <png>
#   <sprites> output of tools/atlas_extract.py <assets> <sprites> [libmortargame.so]
#   <out>     e.g. /path/to/sd/apps/jetpackjoyride  (copy boot.dol in yourself or use package.sh)
set -e
A=$1; P=$2; S=$3; O=$4; T="$(cd "$(dirname "$0")" && pwd)/tools"
[ -d "$A/xml" ] && [ -d "$P" ] && [ -d "$S" ] && [ -n "$O" ] || { sed -n 2,8p "$0"; exit 1; }
mkdir -p "$O"; spr() { python3 -I "$T/mkspr.py" "$@"; }
V=$P/textures/Characters/Vehicles
spr "$S/Barry.png" "$O/barry.spr";  spr "$S/CoinAlt.png" "$O/coin.spr"
spr "$S/laserWarning.png" "$O/beam.spr";  spr "$S/laser.png" "$O/emit.spr" 0 0 64 85
spr "$S/missile.png" "$O/missile.spr";  spr "$S/missileAlert.png" "$O/alert.spr" 0 0 256 64
spr "$P/textures/Entities/zapEffects.png" "$O/bolt.spr"
spr "$S/textures/effects/jetpack/standardflame.png" "$O/flame.spr"
spr "$P/textures/Characters/Barry/defaultBody.png" "$O/body.spr";  spr "$P/textures/Characters/Barry/defaultHead.png" "$O/head.spr"
spr "$P/font/vehicle_font.png" "$O/font.spr";  cp "$A/font/vehicle_font.fnt" "$O/font.fnt"
spr "$V/Mech/vehicleMech.png" "$O/mech.spr";  spr "$V/Hog/vehicleHog.png" "$O/hog.spr";  spr "$V/Bird/vehicleBird.png" "$O/bird.spr"
spr "$V/GravitySuit/vehicleGravitysuit.png" "$O/gsuit.spr";  spr "$V/Teleporter/vehicleTeleporter.png" "$O/tele.spr"
spr "$V/HoverBoard/HoverBoard/hoverboard.png" "$O/hover.spr"
spr "$V/IceSkates/IceSkates/iceskates.png" "$O/skate.spr"
spr "$V/Ecto/Ecto/ecto.png" "$O/ecto.spr"
spr "$V/delorean/delorean/delorean.png" "$O/dlr.spr"
spr "$V/starship/starship/starship.png" "$O/ship.spr"
tmp=$(mktemp -d)   # title logo, downscaled so it stays small in RAM
python3 -I - "$P/textures/logo/default" "$tmp" <<'PY'
import sys
from PIL import Image
d, t = sys.argv[1:3]
Image.open(d + '/logo_text.png').convert('RGBA').resize((360, 218), Image.LANCZOS).save(t + '/t.png')
Image.open(d + '/logo_barry.png').convert('RGBA').resize((192, 270), Image.LANCZOS).save(t + '/b.png')
PY
spr "$tmp/t.png" "$O/logot.spr"; spr "$tmp/b.png" "$O/logob.spr"; rm -rf "$tmp"
python3 -I "$T/mklasers.py" "$A/xml/LazerPatterns.xml" "$O/lasers.txt"
python3 -I "$T/mkmissile.py" "$A/xml/MissileProbability.xml" "$O/missile.txt"
python3 -I "$T/mkzappers.py" "$A/xml/ObstaclePatterns.xml" "$O/zappers.txt"
python3 -I "$T/mkpatterns.py" "$A/patterns" "$O/pattern.txt"
python3 -I "$T/mklevels.py" "$A/xml/LevelFiles" "$S" "$O/levels"
cp "$A/Music_Level.ogg" "$O/music.ogg"
# sound events (names from xml/SoundFiles.xml packages): <ogg in sound/> <name used by the app>
while read src dst; do [ -n "$src" ] && cp "$A/sound/$src.ogg" "$O/$dst.ogg"; done <<'L'
coin_pickup_1 coin
coin_pickup_2 coin2
coin_pickup_3 coin3
Player_hurt_1 hurt
laser_warning laserw
laser_start lstart
missile_warning missw
missile_launch missl
rocket_explode_1 mexp
jetpack_plain_lp jet
jetpack_plain_start jstart
jetpack_plain_stop jstop
run_metal_left_1 step1
run_metal_right_1 step2
jetpack_land_1 land
vehicle_form_complete mmount
bike_start hmount
bird_vox_1 bmount
mech_jump mjump
bike_jump hjump
bird_flap_1 bflap
mech_explode vdie
mech_step_1 mstep
grav_change_1 gchg
CFT_activate cft
dud_zapper_pop zap
L
echo "done: $O"
