---------------------------- OTServ configuration ------------------------------

-- datadir
datadir = "data/"

-- sets what map to load
map = "data/world/test.otbm"

-- OTBM for binary, SQL for SQL map, XML for OTX maps
mapkind = "OTBM"

-- SQL for acc/player info from SQL, XML for acc/player info from XML
sourcedata = "SQL"

-- the message the player gets when he logs in
loginmsg = "Welcome to YurOTS"

-- the port otserv listens on
port = "7171"

-- Jaki priorytet ma ustawic OTS podczas startu(0-normal, 1-rzeczywisty, 2-wysoki, 3-najwyzszy)?
priorytet = "1"

-- name of our server
servername = "YurOTS"

-- name of the owner of our server
ownername = "Yurez"

-- email of the owner of our server
owneremail = ""

-- the url for more server info
url = ""

-- the location of the server
location = "Poland"

-- the ip the server should redirect too (auto - set global ip, last one is a local ip)
ip = "auto"

-- The messagebox you sometimes get before you choose characters
motd = "Welcome to YurOTS"
motdnum="0"

-- use md5 passwords for accounts, yes/no
md5passwords = "no"

-- world type. options: pvp, no-pvp, pvp-enforced
worldtype = "pvp"

-- max number of players allowed
maxplayers = "28"

-- exhausted time in ms (1000 = 1sec)
exhausted = 2000

-- exhaustion time for healing spells (1000 = 1sec)
exhaustedheal = 1000

-- how many ms to add if the player is already exhausted and tries to cast a spell (1000 = 1sec)
exhaustedadd = 300

-- how long does the player has to stay out of fight to get pz unlocked in ms (1000 = 1sec)
pzlocked = 10*1000

-- allow multiple logins of the same char
allowclones = 0

-- vocation names
vocations = {"a sorcerer", "a druid", "a paladin", "a knight"}
promoted_vocations = {"a master sorcerer", "an elder druid", "a royal paladin", "an elite knight"}


--- SQL part
sql_host = "localhost"
sql_user = "root"
sql_pass = ""
sql_db   = "otserv"


--- SQL MAP part
sqlmap = "test_map"
map_host = "localhost"
map_user = "root"
map_pass = ""
map_db   = ""

------------------------- YurOTS basic configuration ---------------------------

-- name of your world (seen on the character list when logging in)
worldname = "YurOTS"

-- how often do server saves it's state (0 = off, 5 = 5min, 60 = hour)
autosave = 10

-- Dynamiczny Zapis Postaci (co ile sekund postaæ ma byæ zapisywana od jej zalogowania?)
-- 0 - wy³¹cz, inna wartoœæ ni¿ 0 powoduje wy³¹czenie standardowego autoSavePlayers
savePlayerTime = 60

-- builtin account maker (rook/main/none)
accmaker = "main"

-- do you want to enable cap system? (yes/no)
capsystem = "no"

-- do you want players to learn spells before they can use them? (yes/no)
learnspells = "no"

-- do you want to give premium account to all players? (yes/no)
freepremmy = "no"

-- do you want premium players to wait in queue as others? (yes/no)
queuepremmy = "yes"

-- how much % of {exp, mana, skill, eq, bp} do you lose when dying
diepercent = {"7", "7", "7", "7", "100"}

-- how many summons player can have
maxsummons = 2

-- do you want to give summons for all vocations? (yes/no)
summonsallvoc = "no"

-- chance of losing a spear when shot (0 - none lost, 100000 - all lost)
spearlosechance = 50

-- anti-afk - maximum idle time to kick player (1 = 1min)
kicktime = 15

-- maximum number of items player can keep in his/her depot
maxdepotitem = 1000
-- for premium
maxdepotpremmy = 2000

-- maximum number of items on a house tile (including those in containers)
maxhousetileitems = 10

-- how much death entries are saved in players file (old are deleted)
maxdeathentries = 10

-- mana consumend by {snakebite, moonlight, volcanic, quagmire, tempest} rod
rodmana = {"2", "3", "5", "8", "13"}

-- mana consumed by wand of {vortex, dragonbreath, plague, cosmic energy, inferno}
wandmana = {"2", "3", "5", "8", "13"}

-- shooting range of {snakebite, moonlight, volcanic, quagmire, tempest} rod
rodrange = {"4", "3", "2", "1", "3"}

-- shooting range of  wand of {vortex, dragonbreath, plague, cosmic energy, inferno}
wandrange = {"4", "3", "2", "1", "3"}

-- damage of burst arrows blast
-- default is from (1*lvl+5*mlvl)*0.24 to (1*lvl+5*mlvl)*0.55
burstarrowdmg = {"1.0", "5.0", "0.24", "0.55"}

--------------------------- Multipliers and gains ------------------------------
-- experience stages(yes/no)
stages_enable = "yes"

-- experience multiplier if experience stages = "no" (how much faster you got exp from monsters)
expmul = "50"

-- experience multiplier for pvp-enforced (how much faster you got exp from players)
expmulpvp = "20"

-- drop multiplier (for drop items from monsters)
droprate = 2

-- speed of attack in seconds (no-voc, sorc, druid, pally, knight)
speed = {"1.8", "1.1", "1.0", "0.7", "0.8"}

-- skill multipliers: 1 - rl tibia, 10 - ten times faster etc. {no-voc, sorcerer, druid, paladin, knight}
weaponmul = {"1", "1", "1", "1", "80"}
distmul = {"1", "1", "1", "80", "1"}
shieldmul = {"1", "1", "1", "50", "80"}
manamul = {"1", "3", "3", "2", "2"}

-- how much points of life,mana,cap do you get when advancing {no-voc, sorcerer, druid, paladin, knight}
capgain = {"10", "10", "10", "20", "25"}
managain = {"5", "30", "30", "15", "5"}
hpgain = {"5", "5", "5", "10", "15"}

-- how much faster do you recovery life,mana after eating food (1 - rl tibia)
healthtickmul = 1000
manatickmul = 2500

-------------------------------- Skull system ----------------------------------

-- how many unjustified kills to get a red skull
redunjust = 3

-- how many unjustified kills to get banned
banunjust = 6

-- how long do you have to ban for too much unjustified kills (1 = 1hour)
bantime = 7*24

-- how long do you have white skull after attacking player (1 = 1min)
hittime = 1

-- how long do you have white skull after killing player (1 = 1min)
whitetime = 15

-- how long do you have red skull (1 = 1min)
redtime = 6*60

-- how long do you have to wait to lose 1 unjustified kill (1 = 1min)
fragtime = 12*60

------------------------------ GM access rights --------------------------------

-- access to walk into houses and open house doors
accesshouse = 3

-- access to login without waiting in the queue or when server is closed
accessenter = 2

-- access to ignore damage, exhaustion, cap limit and be ignored by monsters
accessprotect = 2

-- access to broadcast messages and talk in colors (#c blabla - in public channels)
accesstalk = 1

-- access to move distant items from/to distant locations
accessremote = 3

-- access to see id and position of the item you are looking at
accesslook = 2

-- access to see id and position of the item you are looking at
accessteam = 2

-- players can send message to gamemaster's?
gmmsg = "yes"

--------------------------- Other -----------------------------------
--czy efekty s¹ w³¹czone (yes/no)
effects_emblamed = "no"
--czy kolor textu jest losowy(yes/no)
random_colour = "yes"
--czy efekt jest losowy(yes/no)
random_effect = "yes"
--co jaki czas sie wyœwietla efekt w sekundach
co_ile = 1
-- {text="tekst", pos={x=pozycja_x, y=pozycja_y, z=pozycja_z}, color=kolor_textu, effect=nr_efektu}
efekty = {
                {text="Depo", pos={x=160, y=49, z=6}, color=1, effect=12},
                {text="Ammo", pos={x=155, y=49, z=6}, color=2, effect=13},
                {text="Aol", pos={x=165, y=49, z=6}, color=3, effect=14}
               }


----------------------  Max MagicLvL for each Voc  ---------------------------

-- knight / elite knight
maglev4 = 15

-- paladin / royal paladin
maglev3 = 45

-- druid / elder druid
maglev2 = 160

-- sorcerer / master sorcerer
maglev1 = 160

-- Light Spells By Baabuseek
-- seconds to lose 1 lightlevel
lighttime = 20

-- Baabuseek AutoClean
-- (0 - off, 1 - one minute, 60 - one hour)
autoclean = 60

-- czy pokazywany jest animowany tekst z si³¹ leczenia exury etc.(yes/no)
showHealingDamage = "yes"

---- BBK Training System
-- Training Code
trainingticks = 5  -- in minutes
rewriteticks = 30 -- in seconds

-- How much % chance add lootring?
lootringpercent = 25

----  Suspend System
-- Suspend time period (in minuts)
suspend_time_max = 10
-- Acc tries to be suspended in the period
suspend_acc_tries = 2
-- Same IP loggin tries to be suspended in the period
suspend_ip_tries = 4
-- Message to user when he got suspended
suspendmsg = "Your IP/ACC has been suspended for 10 minuts."


-- Minimun level for player can yell
minlvltoyell = 500

------ Auto Boardcaster
-- time (0 - off, 1 - one minute, 60 - one hour)
boardcasttime = 15
-- Message
boardcastmessage = "Nie daj sie oszukac, nie wysylaj sms'ow byle gdzie! Zdazaja sie gracze podszywajacy sie pod administracje, pamietaj 100% pewnosci daje Ci tylko SMS Shop!"

-- config animated text
animtext = "~"

-------------------------Other--------------------------
-- Muted System
mutedsystem = "yes"
mutedtime = 4

-- Life Ring Configuration
LifeRingHealth = "15"
LifeRingMana = "15"

-- ROH Configuration
RoHHealth = "30"
RoHMana = "30" 

-- Ile HP i many maja dodawac Soft Boots??
softHealth = 1 
softMana = 1

-- !buyhouse/!domek enabled?
buyhouse = "yes"
-- house price for 1 sqm
priceforsqm = 1000
-- Level to buy house using command !buyhouse
buyhouselvl = 100
-- max houses for player
maxhouses = 1

-- Ile % magicznych obrazen ma zwiekszac Sapphire Set?
sapphireset = 8
-- Ile % ataku dodawac ma jungle bp?
MagicBpAtk = 10
-- Na jaki outfit ma zmieniac po zalozeniu jungle bp?
MagicBpLt = 68 

-- Czas w sekundach po jakim mozna otwierac ciala (LOOT OWNER)
ownerTime = 10

-- pvp stages(yes/no)
pvp_stages_enable = "yes"


