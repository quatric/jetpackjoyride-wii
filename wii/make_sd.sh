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
spr "$S/pickupCoinSpecial.png" "$O/pkcoin.spr"; spr "$S/pickupBoost.png" "$O/pkboost.spr"; spr "$S/pickupSlowMo.png" "$O/pkslow.spr"; spr "$S/pickupShield.png" "$O/pkshield.spr"; spr "$S/Barry.png" "$O/barry.spr";  spr "$S/CoinAlt.png" "$O/coin.spr"
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
spr "$V/sled/sleigh.png" "$O/sled.spr"
spr "$V/Jetski/jetski.png" "$O/jski.spr"
spr "$P/textures/menus/buttonBacking.png" "$O/pbk.spr"; spr "$P/textures/menus/popupBacking.png" "$O/popup.spr"; spr "$P/textures/menus/paused/buttonExit.png" "$O/pq.spr"; spr "$P/textures/menus/paused/buttonRestart.png" "$O/pr.spr"; spr "$P/textures/menus/paused/buttonResume.png" "$O/pg.spr"; spr "$P/textures/menus/main/buttonSmallGreen.png" "$O/sg.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/coinMagnet.png" "$O/magnet.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/slowMissile.png" "$O/smiss.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/dudMissile.png" "$O/dmiss.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/rubberBall.png" "$O/rub.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/gem.png" "$O/gem.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/vehicleStart.png" "$O/vstart.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/gravityBelt.png" "$O/gbelt.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/dudZappies.png" "$O/dzap.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/xraySpecs.png" "$O/xray.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/brick.png" "$O/brick.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/fruitMissile.png" "$O/fruit.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/icySkin.png" "$O/icy.spr"; spr "$P/textures/Characters/RobotDog/robodog-alt.png" "$O/rdog.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/robotDog.png" "$O/rdogi.spr"; spr "$P/textures/RewardSystem/Tokens/pickup.png" "$O/tok.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/tokenMagnet.png" "$O/tmag.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/tokenGift.png" "$O/tgift.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/boostToken.png" "$O/boost.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/luckyToken.png" "$O/lucky.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/flyingPig.png" "$O/flypig.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/airBarrys.png" "$O/airb.spr"; spr "$P/textures/Entities/distanceMarkerPig.png" "$O/pig.spr"; spr "$P/textures/menus/shop/ItemIcons/gadgets/nerdRepellent.png" "$O/nerd.spr"; spr "$P/textures/menus/shop/ItemIcons/skip.png" "$O/hs.spr"; spr "$P/textures/menus/shop/ItemIcons/bomb.png" "$O/fb.spr"; for k in scientist:sci worker:wrk; do spr "$P/textures/Characters/Barry/${k%%:*}Body.png" "$O/${k##*:}b.spr"; spr "$P/textures/Characters/Barry/${k%%:*}Head.png" "$O/${k##*:}h.spr"; done; spr "$P/textures/Characters/Vehicles/dragon/Colossatron/Head-Top.png" "$O/colh.spr"; spr "$P/textures/Characters/Vehicles/dragon/Colossatron/Blue.png" "$O/colb.spr"; spr "$P/textures/Characters/Vehicles/dragon/Colossatron/sonicBoom.png" "$O/boom.spr"; spr "$P/textures/menus/backings/hazzardTile.png" "$O/hz.spr"; spr "$P/textures/menus/paused/titleBar.png" "$O/tb.spr"
spr "$V/dragon/dragonHead.png" "$O/drh.spr"; spr "$V/dragon/dragonBody.png" "$O/drb.spr"; spr "$V/dragon/dragonFlap.png" "$O/drf.spr"
tmp=$(mktemp -d)   # title logo, downscaled so it stays small in RAM
python3 -I - "$P/textures/logo/default" "$tmp" <<'PY'
import sys
from PIL import Image
d, t = sys.argv[1:3]
Image.open(d + '/logo_text.png').convert('RGBA').resize((360, 218), Image.LANCZOS).save(t + '/t.png')
Image.open(d + '/logo_barry.png').convert('RGBA').resize((192, 270), Image.LANCZOS).save(t + '/b.png')
PY
spr "$tmp/t.png" "$O/logot.spr"; spr "$tmp/b.png" "$O/logob.spr"; rm -rf "$tmp"
spr --cmpr "$P/textures/levels/bgTitle0.png" "$O/title.spr" 56 64 672 512   # the lab room the title screen sits in (bgTitle atlas top-left, minus its dark outer margin)
python3 -I "$T/mklasers.py" "$A/xml/LazerPatterns.xml" "$O/lasers.txt"
python3 -I "$T/mkmissile.py" "$A/xml/MissileProbability.xml" "$O/missile.txt"
python3 -I "$T/mkzappers.py" "$A/xml/ObstaclePatterns.xml" "$O/zappers.txt"
# per-vehicle obstacle files (LevelPacing.xml streaks), by app vehicle index: mech hog bird gsuit tele hover skate ecto delorean starship sled jetski ("-" = none shipped)
i=0; for n in MECH HOG - GRAV - HOVER ICESKATES ECT DEL - SLED JETSKI; do [ "$n" = - ] || python3 -I "$T/mkzappers.py" "$A/xml/Streaks/${n}Obstacles.xml" "$O/zv$i.txt" >/dev/null; i=$((i+1)); done
# per-vehicle missile files (Ecto uses the Delorean's, as in LevelPacing.xml; ice skates and the Delorean spell it Missiles / Missles)
i=0; for n in MECHMissles HOGMissles - GRAVMissles - HOVERMissles ICESKATESMissiles DELMissles DELMissles - SLEDMissles JETSKIMissles; do [ "$n" = - ] || python3 -I "$T/mkmissile.py" "$A/xml/Streaks/$n.xml" "$O/mv$i.txt"; i=$((i+1)); done
python3 -I "$T/mkgadgets.py" "$A/StringTables" "$O/gadgets.txt"
python3 -I "$T/mkmissions.py" "$A" "$O/missions.txt"
python3 -I "$T/mkpatterns.py" "$A/patterns" "$O/pattern.txt"
# per-vehicle coin pattern sets (Configs/coins_patterns/Streaks_<V>Coins_1.json), indexed by VEH like zv<N>.txt; HOG/CFT/SAM have none here
i=0; for n in MECH - - GRAV - HOVER ICESKATES ECTO DEL - - JETSKI; do [ "$n" = - ] || python3 -I "$T/mkcoinset.py" "$A/Configs/coins_patterns/Streaks_${n}Coins_1.json" "$A/patterns" "$O/cv$i.txt"; i=$((i+1)); done
python3 -I "$T/mklevels.py" "$A/xml/LevelFiles" "$S" "$O/levels"
cp "$A/Music_Level.ogg" "$O/music.ogg"; cp "$A/Music_Shop.ogg" "$O/mshop.ogg"; cp "$A/Music_Hog.ogg" "$O/mhog.ogg"; cp "$A/Music_Colossatron.ogg" "$O/mcolo.ogg"
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
