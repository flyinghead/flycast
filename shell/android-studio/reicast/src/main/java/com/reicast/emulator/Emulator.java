package com.reicast.emulator;

import android.app.Activity;
import android.app.Application;
import android.content.SharedPreferences;
import android.support.v7.app.AppCompatDelegate;

import com.android.util.DreamTime;
import com.reicast.emulator.emu.JNIdc;

public class Emulator extends Application {

    public static final String pref_dynarecopt = "dynarec_opt";
    public static final String pref_unstable = "unstable_opt";
    public static final String pref_dynsafemode = "dyn_safemode";
    public static final String pref_cable = "dc_cable";
    public static final String pref_dcregion = "dc_region";
    public static final String pref_broadcast = "dc_broadcast";
    public static final String pref_limitfps = "limit_fps";
    public static final String pref_nosound = "sound_disabled";
    public static final String pref_mipmaps = "use_mipmaps";
    public static final String pref_widescreen = "stretch_view";
    public static final String pref_frameskip = "frame_skip";
    public static final String pref_pvrrender = "pvr_render";
    public static final String pref_syncedrender = "synced_render";
    public static final String pref_modvols = "modifier_volumes";
    public static final String pref_bootdisk = "boot_disk";
    public static final String pref_usereios = "use_reios";

    public static boolean dynarecopt = true;
    public static boolean idleskip = true;
    public static boolean unstableopt = false;
    public static boolean dynsafemode = false;
    public static int cable = 3;
    public static int dcregion = 3;
    public static int broadcast = 4;
    public static boolean limitfps = true;
    public static boolean nobatch = false;
    public static boolean nosound = false;
    public static boolean mipmaps = true;
    public static boolean widescreen = false;
    public static boolean subdivide = false;
    public static int frameskip = 0;
    public static boolean pvrrender = false;
    public static boolean syncedrender = false;
    public static boolean modvols = true;
    public static String bootdisk = null;
    public static boolean usereios = false;

    /**
     * Load the user configuration from preferences
     *
     */
    public void getConfigurationPrefs(SharedPreferences mPrefs) {
        Emulator.dynarecopt = mPrefs.getBoolean(pref_dynarecopt, dynarecopt);
        Emulator.unstableopt = mPrefs.getBoolean(pref_unstable, unstableopt);
        Emulator.cable = mPrefs.getInt(pref_cable, cable);
        Emulator.dcregion = mPrefs.getInt(pref_dcregion, dcregion);
        Emulator.broadcast = mPrefs.getInt(pref_broadcast, broadcast);
        Emulator.limitfps = mPrefs.getBoolean(pref_limitfps, limitfps);
        Emulator.nosound = mPrefs.getBoolean(pref_nosound, nosound);
        Emulator.mipmaps = mPrefs.getBoolean(pref_mipmaps, mipmaps);
        Emulator.widescreen = mPrefs.getBoolean(pref_widescreen, widescreen);
        Emulator.frameskip = mPrefs.getInt(pref_frameskip, frameskip);
        Emulator.pvrrender = mPrefs.getBoolean(pref_pvrrender, pvrrender);
        Emulator.syncedrender = mPrefs.getBoolean(pref_syncedrender, syncedrender);
        Emulator.bootdisk = mPrefs.getString(pref_bootdisk, bootdisk);
        Emulator.usereios = mPrefs.getBoolean(pref_usereios, usereios);
    }

    /**
     * Write configuration settings to the emulator
     *
     */
    public void loadConfigurationPrefs() {
        JNIdc.dynarec(Emulator.dynarecopt ? 1 : 0);
        JNIdc.idleskip(Emulator.idleskip ? 1 : 0);
        JNIdc.unstable(Emulator.unstableopt ? 1 : 0);
        JNIdc.safemode(Emulator.dynsafemode ? 1 : 0);
        JNIdc.cable(Emulator.cable);
        JNIdc.region(Emulator.dcregion);
        JNIdc.broadcast(Emulator.broadcast);
        JNIdc.limitfps(Emulator.limitfps ? 1 : 0);
        JNIdc.nobatch(Emulator.nobatch ? 1 : 0);
        JNIdc.nosound(Emulator.nosound ? 1 : 0);
        JNIdc.mipmaps(Emulator.mipmaps ? 1 : 0);
        JNIdc.widescreen(Emulator.widescreen ? 1 : 0);
        JNIdc.subdivide(Emulator.subdivide ? 1 : 0);
        JNIdc.frameskip(Emulator.frameskip);
        JNIdc.pvrrender(Emulator.pvrrender ? 1 : 0);
        JNIdc.syncedrender(Emulator.syncedrender ? 1 : 0);
        JNIdc.modvols(Emulator.modvols ? 1 : 0);
        JNIdc.usereios(Emulator.usereios ? 1 : 0);
        JNIdc.bootdisk(Emulator.bootdisk);
        JNIdc.dreamtime(DreamTime.getDreamtime());
    }

    public void loadGameConfiguration(String gameId) {
        SharedPreferences mPrefs = getSharedPreferences(gameId, Activity.MODE_PRIVATE);
        JNIdc.dynarec(mPrefs.getBoolean(pref_dynarecopt, dynarecopt) ? 1 : 0);
        JNIdc.unstable(mPrefs.getBoolean(pref_unstable, unstableopt) ? 1 : 0);
        JNIdc.safemode(mPrefs.getBoolean(pref_dynsafemode, dynsafemode) ? 1 : 0);
        JNIdc.frameskip(mPrefs.getInt(pref_frameskip, frameskip));
        JNIdc.pvrrender(mPrefs.getBoolean(pref_pvrrender, pvrrender) ? 1 : 0);
        JNIdc.syncedrender(mPrefs.getBoolean(pref_syncedrender, syncedrender) ? 1 : 0);
        JNIdc.modvols(mPrefs.getBoolean(pref_modvols, modvols) ? 1 : 0);
        JNIdc.bootdisk(mPrefs.getString(pref_bootdisk, bootdisk));
    }

    public int isVGACompatible(String gameId) {
        int vgaMode; // -1 = None / Unknown, 0 = VGA, 1 = Patchable, 2 = TV
        switch (gameId) {
            case "T-36803N": //102 Dalmatians puppies to the Rescue
            case "T-36813D-05": //102 Dalmatians puppies to the Rescue
            case "51064": // 18 Wheeler American Pro Trucker
            case "MK-51064": // 18 Wheeler American Pro Trucker
            case "T-9708N": // 4 Wheel Thunder
            case "T-9706D": // 4 Wheel Thunder
            case "T-41903N": // 4x4 Evolution
            case "MK-51190": // 90 Minutes Championship Football
            case "T-40201N": // Aerowings
            case "T-40202D-50": // Aerowings
            case "T-40210N": // Aerowings 2
            case "MK-51171": // Alien Front Online
            case "T-15117N": // Alone in the Dark The New Nightmare
            case "T-15112D-05": // Alone in the Dark The New Nightmare
            case "T-40301N": // Armada
            case "T-15130N": // Atari Aniversary Edition
            case "T-44102N": // Bang! Gunship Elite
            case "T-13001N": // Blue Stinger
            case "T-13001D-58": // Blue Stinger
            case "51065": // Bomberman Online
            case "T-13007N": // Buzz Lightyear of Star Command
            case "T-13005D-05": // Buzz Lightyear of Star Command
            case "T-1215N": // Cannon Spike
            case "T-46601D-50": // Cannon Spike
            case "T-1218N": // Capcom Vs. SNK
            case "T-5701N": // Carrier
            case "T-44901D-50": // Carrier
            case "T-40602N": // Centepede
            case "T-41403N": // Championship Surfer
            case "T-41402D-50": // Championship Surfer
            case "T-15127N": // Charge'N Blast
            case "T-44902D-50": // Charge'N Blast
            case "T-36811N": // Chicken Run
            case "T-36814D-05": // Chicken Run
            case "51049": // ChuChu Rocket!
            case "MK-51049-50": // ChuChu Rocket!
            case "51160": // Confidential Mission
            case "MK-51160-50": // Confidential Mission
            case "T-46603D-71": // Conflict Zone
            case "51035": // Crazy Taxi
            case "MK-51035-50": // Crazy Taxi
            case "51136": // Crazy Taxi 2
            case "MK-51136-50": // Crazy Taxi 2
            case "51036": // D-2
            case "T-8120N": // Dave Mirra BMX
            case "T-8120D-59": // Dave Mirra BMX
            case "51037": // Daytona USA
            case "MK-51037-50": // Daytona USA 2001
            case "T-3601N": // Dead or Alive 2
            case "T-8116D-05": // Dead or Alive 2 & ECW Hardcore Revolution
            case "T-2401N": // Death Crimson OX
            case "T-15112N": // Demolition Racer
            case "T-17717N": // Dinosaur
            case "T-17719N": // Donald Duck: Goin' Quackers
            case "T-17714D-05": // Donald Duck: Quack Attack
            case "T-40203N": // Draconus: Cult of the Wyrm
            case "T-17720N": // Dragon Riders: Chronicles of Pern
            case "T-17716D-91": // Dragon Riders: Chronicles of Pern
            case "T-8113N": // Ducati World Racing Challenge
            case "T-8121D-05": // Ducati World
            case "51013": // Dynamite Cop!
            case "MK-51013-50": // Dynamite Cop!
            case "51033": // Ecco the Dolphin: Defender of the Future
            case "MK-51033-50": // Ecco the Dolphin: Defender of the Future
            case "T-41601N": // Elemental Gimmick Gear
            case "T-9509N": // ESPN International Track n Field
            case "T-9504D-50": // ESPN International Track n Field
            case "T-9505N": // ESPN NBA2Night
            case "T-7015D-50": // European Super League
            case "T-46605D-71": // Evil Twin Cyprien's Chronicles
            case "T-17706N": // Evolution: The World of Sacred Device
            case "T-17711N": // Evolution 2: Far Off Promise
            case "T-45005D-50": // Evolution 2: Far Off Promise
            case "T-22903D-50": // Exhibition of Speed
            case "T-15101D-05": // Millenium Soldier: Expendable
            case "T-17706D-50": // F1 Racing Championship
            case "T-3001N": // F1 World Grand Prix
            case "T-3001D-50": // F1 World Grand Prix
            case "T-3002D-50": // F1 World Grand Prix II
            case "T-8119N": // F355 Challenge: Passione Rossa
            case "T-8118D-05": // F355 Challenge: Passione Rossa
            case "T-44306N": // Fatal Fury: Mark of the Wolf
            case "T-35801N": // Fighting Force 2
            case "T-36802D-05": // Fighting Force 2
            case "MK-51154-50": // Fighting Vipers 2
            case "51007": // Flag to Flag
            case "51114": // Floigan Brothers Episode 1
            case "MK-51114-50": // Floigan Brothers Episode 1
            case "T-40604N": // Frogger 2 Swampies Revenge
            case "T-8107N": // Fur Fighters
            case "T-8113D-05": // Fur Fighters
            case "T-9710N": // Gauntlet Legends
            case "T-9707D-51": // Gauntlet Legends
            case "T-1209N": // Giga Wing
            case "T-7008D-50": // Giga Wing
            case "T-1222N": // Giga Wing 2
            case "T-42102N": // Grand Theft Auto II
            case "T-40502D-61": // Grand Theft Auto II
            case "T-17716N": // Grandia II
            case "T-17715D-05": // Grandia II
            case "T-9512N": // Grintch, The
            case "T-9503D-76": // Grintch, The
            case "T-13301N": // Gundam Side Story 0079
            case "51041": // Headhunter
            case "T-1223N": // Heavy Metal: Geomatrix
            case "T-46602D-50": // Heavy Metal: Geomatrix
            case "51002": // House of the Dead 2
            case "MK-51045-50": // House of the Dead 2
            case "T-11008N": // Hoyle Casino
            case "T-46001N": // Illbleed
            case "T-12503N": // Incoming
            case "T-40701D-50": // Incoming
            case "T-41302N": // Industrial Spy: Operation Espionage
            case "T-8104N": // Jeremy McGrath Supercross 2000
            case "T-8114D-05": // Jeremy McGrath Supercross 2000
            case "BKL83176.01-ENG": // Jeremy McGrath Supercross 2000
            case "51058": // Jet Grind Radio
            case "MK-51058-50": // Jet Set Radio
            case "T-7001D": // Jimmy White's 2: Cueball
            case "T-22903N": // Kao the Kangeroo
            case "T-22902D-50": // Kao the Kangeroo
            case "T-41901N": // KISS: Psycho Circus: The Nightmare Child
            case "T-40506D-50": // KISS: Psycho Circus: The Nightmare Child
            case "T-36802N": // Legacy of Kain: Soul Reaver
            case "T-36803D-05": // Legacy of Kain: Soul Reaver
            case "T-15116N": // Loony Toons Space Race
            case "T-15108D-50": // Loony Toons Space Race
            case "T-40208N": // Mag Force Racing
            case "T-40207D-50": // Mag Force Racing
            case "T-36804N": // Magical Racing Tour
            case "T-36809D-50": // Magical Racing Tour
            case "51050": // Maken X
            case "MK-51050-50": // Maken X
            case "T-1221N": // Mars Matrix
            case "T-1202N": // Marvel vs. Capcom: Clash of Super Heroes
            case "T-7002D-61": // Marvel vs. Capcom: Clash of Super Heroes
            case "T-1212N": // Marvel vs. Capcom 2
            case "T-7010D-50": // Marvel vs. Capcom 2: New Age of Heroes
            case "T-13005N": // Mat Hoffman's Pro BMX
            case "T-41402N": // Max Steel Covert Missions
            case "T-11002N": // Maximum Pool
            case "T-12502N": // MDK2
            case "T-12501D-61": // MDK2
            case "51012": // Metropolis Street Racer
            case "MK-51022-50": // Metropolis Street Racer
            case "T-9713N": // Midway Greatest Arcade Hits Volume 1
            case "T-9710D-50": // Midway Greatest Arcade Hits Volume 1
            case "T-9714N": // Midway Greatest Arcade Hits Volume 2
            case "T-40508D": // Moho (Ball Breakers)
            case "T-17701N": // Monaco Grand Prix
            case "T-45006D-50": // Racing Simulation 2: On-line Monaco Grand Prix
            case "T-9701D-61": // Mortal Kombat Gold
            case "T-1402N": // Mr Driller
            case "T-7020D-50": // Mr Driller
            case "T-1403N": // Namco Museum
            case "51004": // NBA 2K
            case "MK-51004-53": // NBA 2K
            case "51063": // NBA 2K1
            case "51178": // NBA 2K2
            case "MK-51178-50": // NBA 2K2
            case "T-9709N": // NBA Hoopz
            case "T-9713D-61": // NBA Hoopz
            case "51176": // NCAA College Football 2K2
            case "51003": // NFL 2K
            case "51062": // NFL 2K1
            case "51168": // NFL 2K2
            case "T-9703N": // NFL Blitz 2000
            case "T-9712N": // NFL Blitz 2001
            case "T-8101N": // NFL Quarterback Club 2000
            case "T-8102D-05": // NFL Quarterback Club 2000
            case "T-8115N": // NFL Quarterback Club 2001
            case "51025": // NHL 2K
            case "MK-51025-89": // NHL 2K
            case "51182": // NHL 2K2
            case "T-9504N": // Nightmare Creatures II
            case "T-9502D-76": // Nightmare Creatures II
            case "T-36807N": // Omikron The Nomad Soul
            case "T-36805D-09": // Nomad Soul, The
            case "51140": // Ooga Booga
            case "51102": // OutTrigger: International Counter Terrorism Special Force
            case "MK-51102-50": // OutTrigger: International Counter Terrorism Special Force
            case "T-15105N": // Pen Pen TriIcelon
            case "51100": // Phantasy Star Online
            case "MK-51100-50": // Phantasy Star Online
            case "51193": // Phantasy Star Online Ver.2
            case "MK-51193-50": // Phantasy Star Online Ver.2
            case "MK-51148-64": // Planet Ring
            case "T-17713N": // POD: Speedzone
            case "T-17710D-50": // Pod 2 Multiplayer Online
            case "T-1201N": // Power Stone
            case "T-36801D-64": // Power Stone
            case "T-1211N": // Power Stone 2
            case "T-36812D-61": // Power Stone 2
            case "T-41405N": // Prince of Persia Arabian Nights
            case "T-30701D": // Pro Pinball Trilogy
            case "T-1219N": // Project Justice
            case "T-7022D-50": // Project Justice: Rival Schools 2
            case "51061": // Quake III Arena
            case "MK-51061-50": // Quake III Arena
            case "T-41902N": // Railroad Tycoon II: Gold Edition
            case "T-17704N": // Rayman 2
            case "T-17707D-50": // Rayman 2
            case "T-40219N": // Razor Freestyle Scooter
            case "T-46604D-50": // Freestyle Scooter
            case "T-8109N": // Re-Volt
            case "T-8107D-05": // Re-Volt
            case "T-9704N": // Ready 2 Rumble Boxing
            case "T-9704D-50": // Ready 2 Rumble Boxing
            case "T-9717N": // Ready 2 Rumble Boxing Round 2
            case "T-9711D-50": // Ready 2 Rumble Boxing Round 2
            case "T-40218N": // Record of Lodoss War
            case "T-7012D-97": // Record of Lodoss War
            case "T-40215N": // Red Dog: Superior Firepower
            case "MK-51021-50": // Red Dog: Superior Firepower
            case "T-1205N": // Resident Evil 2
            case "T-7004D-61": // Resident Evil 2
            case "T-1220N": // Resident Evil 3: Nemesis
            case "T-7021D-56": // Resident Evil 3: Nemesis
            case "T-1204N": // Resident Evil Code: Veronica
            case "MK-51192-50": // REZ
            case "T-22901N": // Roadsters
            case "T-22901D-05": // Roadsters
            case "51092": // Samba De Amigo
            case "MK-51092-50": // Samba De Amigo
            case "T-9709D-61": // San Fransisco Rush 2049
            case "51048": // SeaMan
            case "51006": // Sega Bass Fishing
            case "MK-51006-05": // Sega Bass Fishing
            case "51166": // Sega Bass Fishing 2
            case "51053": // Sega GT
            case "MK-51053-50": // Sega GT
            case "51096": // Sega Marine Fishing
            case "51019": // Sega Rally Championship 2
            case "MK-51019-50": // Sega Rally 2
            case "51146": // Sega Smash Pack 1
            case "MK-51083-50": // Sega Worldwide Soccer 2000 Euro Edition
            case "T-41301N": // Seventh Cross Evolution
            case "T-8106N": // Shadowman
            case "51059": // Shenmue
            case "MK-51059-50": // Shenmue
            case "MK-51184-50": // Shenmue 2
            case "T-9507N": // Silent Scope
            case "T-9505D-76": // Silent Scope
            case "T-15108N": // Silver
            case "T-15109D-91": // Silver
            case "51052": // Skies of Arcadia
            case "T-15106N": // Slave Zero
            case "T-15104D-59": // Slave Zero
            case "T-40207N": // Sno-Cross Championship Racing
            case "T-40212N": // Soldier of Fortune
            case "T-17726D-50": // Soldier of Fortune
            case "51000": // Sonic Adventure
            case "MK-51000-53": // Sonic Adventure
            case "51014": // Sonic Adventure (Limited Edition)
            case "51117": // Sonic Adventure 2
            case "MK-51117-50": // Sonic Adventure 2
            case "51060": // Sonic Shuffle
            case "MK-51060-50": // Sonic Shuffle
            case "T-1401D-50": // SoulCalibur
            case "T-8112D-05": // South Park Rally
            case "T-8105N": // South Park: Chef's Luv Shack
            case "51051": // Space Channel 5
            case "MK-51051-50": // Space Channel 5
            case "T-1216N": // Spawn: In the Demon's Hand
            case "T-41704N": // Spec Ops II: Omega Squad
            case "T-45004D-50": // Spec Ops II: Omega Squad
            case "T-17702N": // Speed Devils
            case "T-17702D-50": // Speed Devils
            case "T-17718N": // Speed Devils Online Racing
            case "T-17713D-50": // Speed Devils Online Racing
            case "T-13008N": // Spider-man
            case "T-13011D-05": // Spider-man
            case "T-8118N": // Spirit of Speed 1937
            case "T-8117D-59": // Spirit of Speed 1937
            case "T-44304N": // Sports Jam
            case "T-23003N": // Star Wars: Demolition
            case "T-13010D-50": // Star Wars: Demolition
            case "T-23002N": // Star Wars: Episode 1 Jedi Power Battles
            case "T-23002D-50": // Star Wars: Episode 1 Jedi Power Battles
            case "T-23001N": // Star Wars: Episode 1 Racer
            case "T-13006D-50": // Star Wars: Episode 1 Racer
            case "T-40209N": // StarLancer
            case "T-17723D-50": // StarLancer
            case "T-1203N": // Street Fighter Alpha3
            case "T-7005D-50": // Street Fighter Alpha3
            case "T-1213N": // Street Fighter III: 3rd Strike
            case "T-7013D-50": // Street Fighter III: 3rd Strike
            case "T-1210N": // Street Fighter III: Double Impact
            case "T-7006D-50": // Street Fighter III: Double Impact
            case "T-15111N": // Striker Pro 2000
            case "T-15102D-50": // UEFA Striker
            case "T-22904D": // Stunt GP
            case "T-17708N": // Stupid Invaders
            case "T-17711D-71": // Stupid Invaders
            case "T-40206N": // Super Magnetic Neo
            case "T-40206D-50": // Super Magnetic Neo
            case "T-12511N": // Super Runabout: San Francisco Edition
            case "T-7014D-50": // Super Runabout: The Golden State
            case "T-40216N": // Surf Rocket Racers
            case "T-17721D-50": // Surf Rocket Racers
            case "T-17703N": // Suzuki Alstare Extreme Racing
            case "T-17703D-05": // Suzuki Alstare Extreme Racing
            case "T-36805N": // Sword of the Berserk: Guts' Rage
            case "T-36807D-05": // Sword of the Berserk: Guts' Rage
            case "T-17708D": // Taxi 2
            case "T-1208N": // Tech Romancer
            case "T-7009D-50": // Tech Romancer
            case "T-8108N": // Tee Off
            case "51186": // Tennis 2K2
            case "MK-51186-50": // Virtua Tennis 2
            case "T-15102N": // Test Drive 6
            case "T-15123N": // Test Drive Le Mans
            case "T-15111D-91": // Le Mans 24 Hours
            case "T-15110N": // Test Drive V-Rally
            case "T-15105D-05": // V-Rally 2: Expert Edition
            case "51011": // Time Stalkers
            case "MK-51011-53": // Time Stalkers
            case "T-13701N": // TNN Motorsports HardCore Heat
            case "T-40202N": // Tokyo Extreme Racer
            case "T-40201D-50": // Tokyo Highway Challenge
            case "T-40211N": // Tokyo Extreme Racer 2
            case "T-17724D-50": // Tokyo Highway Challenge 2
            case "T-40402N": // Tom Clancy's Rainbow Six Rouge Spear
            case "T-45002D-61": // Tom Clancy's Rainbow Six Rouge Spear
            case "T-36812N": // Tomb Raider: Chronicles
            case "T-36815D-05": // Tomb Raider: Chronicles
            case "T-36806N": // Tomb Raider: The Last Revelation
            case "T-36804D-05": // Tomb Raider: The Last Revelation
            case "T-40205N": // Tony Hawks Pro Skater
            case "T-40204D-50": // Tony Hawk's Skateboarding
            case "T-13006N": // Tony Hawks Pro Skater 2
            case "T-13008D-05": // Tony Hawks Pro Skater 2
            case "51020": // Toy Comander
            case "MK-51020-50": // Toy Comander
            case "51149": // Toy Racer
            case "T-13003N": // Toy Story 2: Buzz Lightyear to the Rescue!
            case "T-13003D-05": // Toy Story 2: Buzz Lightyear to the Rescue!
            case "T-8102N": // TrickStyle
            case "T-8101D-05": // TrickStyle
            case "51144": // Typing of the Dead
            case "MK-51095-05": // UEFA Dream Soccer
            case "T-40204N": // Ultimate Fighting Championship
            case "T-40203D-50": // Ultimate Fighting Championship
            case "T-15125N": // Unreal Tornament
            case "T-36801D-50": // Unreal Tornament
            case "T-36810N": // Urban Chaos
            case "T-36810D-50": // Urban Chaos
            case "T-8110N": // Vanishing Point
            case "T-8110D-05": // Vanishing Point
            case "T-13002N": // Vigilante 8: 2nd Offense
            case "T-13002D-71": // Vigilante 8: 2nd Offense
            case "T-44301N": // Virtua Athlete 2000
            case "MK-51094-50": // Virtua Athlete 2K
            case "51001": // Virtua Fighter 3tb
            case "MK-51001-53": // Virtua Fighter 3tb
            case "51028": // Virtua Striker 2
            case "MK-51028-50": // Virtua Striker 2 Ver. 2000.1
            case "51054": // Virtua Tennis
            case "MK-51054-50": // Virtua Tennis
            case "T-13004N": // Virtual On: Oratorio Tangram
            case "T-15113N": // Wacky Races
            case "T-15106D-05": // Wacky Races
            case "T-8111N": // Wetrix+
            case "T-40504D-64": // Wetrix+
            case "T-36811D": // Who Wants To Be A Millianare
            case "T-42101N": // Wild Metal
            case "T-40501D-64": // Wild Metal
            case "51055": // World Series Baseball 2K1
            case "51152": // World Series Baseball 2K2
            case "T-40601N": // Worms Armageddon
            case "T-40601D-79": // Worms Armageddon
            case "T-22904N": // Worms World Party
            case "T-7016D-50": // Worms World Party
            case "T-10005N": // WWF Royal Rumble
            case "T-10003D-50": // WWF Royal Rumble
            case "T-15126N": //  Xtreme Sports
            case "MK-51081-50": // Sega Extreme Sports
            case "51038": // Zombie Revenge
            case "MK-51038-50": // Zombie Revenge
            // Unlicensed Games
            case "T-26702N": // PBA Tour Bowling 2001
            case "43011": // Bleem!Cast - Gran Turismo 2
            case "43021": // Bleem!Cast - Metal Gear Solid
            case "43031": // Bleem!Cast - Tekken 3
            // Other Software
                /*case "": // Codebreaker & Loader */
            case "T0000": // DC VCD Player (Joy Pad hack)
                vgaMode = 0;

            case "T-40509D-50": // Aqua GT
            case "T-9715N": // Army Men Sarges Heroes
            case "T-9708D-61": // Army Men Sarges Heroes
            case "T-8117N": // Bust-A-Move 4
            case "T-8109D-05": // Bust-A-Move 4
            case "T-44903D-50": // Coaster Works
            case "T-17721N": // Conflict Zone
            case "T-1217N": // Dino Crisis
            case "T-7019D-05": // Dino Crisis
            case "T-12503D-61": // Dragon's Blood
            case "T-10003N": // Evil Dead Hail to the King
            case "T-10005D-05": // Evil Dead Hail to the King
            case "T-17705SD-50": // Evolution: The World of Sacred Device
            case "T-15104N": // Expendable
            case "T-45401D-50": // Giant Killers
            case "T-1214N": // Gun Bird 2
            case "T-7018D-50": // Gun Bird 2
            case "T-40502-N": // Hidden and Dangerous
            case "T-40503D-64": // Hidden and Dangerous
            case "T-9702D-61": // Hydro Thunder
            case "T-9701N": // Mortal Kombat Gold
            case "T-1404N": // Ms Pacman Maze Madness
            case "T-10001D-50": //  MTV Sports: Skateboarding feat. Andy McDonald
            case "T-9706N": // NBA Showdown: NBA on NBC
            case "T-9705D-50": // NBA Showdown: NBA on NBC
            case "T-40214N": // Next Tetris, The
            case "T-9703D-50": // NFL Blitz 2000
            case "T-40403N": // Q*bert
            case "T-44303N": // Reel Fishing: Wild
            case "51010": // Rippin' Riders
            case "T-9707N": // San Fransisco Rush 2049
            case "MK-51031-50": // Sega Worldwide Soccer 2000
            case "T-8104D-05": // Shadowman
            case "MK-51052-50": // Skies of Arcadia
            case "T-17722D-50": // Sno-Cross Championship Racing
            case "T-1401N": // SoulCalibur
            case "T-8116N": // South Park Rally
            case "T-8105D-50": // South Park: Chef's Luv Shack
            case "T-36816D-05": // Spawn: In the Demon's Hand
            case "T-36808D-05": // Sydney 2000
            case "T-8108D-05": // Tee Off
            case "MK-54040-50": // TNN Motorsports Buggy Heat
            case "T-40401N": // Tom Clancy's Rainbow Six
            case "T-45001D-05": // Tom Clancy's Rainbow Six
            case "T-11011N": // Who Wants To Beat Up A Millianare
                vgaMode = 1;

            case "T-40209D-50": // Aerowings 2
            case "T-9501N": // Air Force Delta
            case "T-9501D-76": // Air Force Delta
            case "T-40217N": // Bangai-O
            case "T-7011D": // Bangai-O
            case "T-12504N": // Ceasars Palace 2000: Millennium Gold Edition
            case "T-12502D-61": // Ceasars Palace 2000: Millennium Gold Edition
            case "T-7017D-50": // Capcom Vs. SNK
            case "T-15128N": // Coaster Works
            case "T-17718D-84": // Dinosaur
            case "T-8114N": // ECW Anarchy Rulz
            case "T-8119D-59": // ECW Anarchy Rulz
            case "T-8112N": // ECW Hardcore Revolution
            case "BKL83203.01-ENG": // ECW Hardcore Revolution
            case "T-9702N": // Hydro Thunder
            case "T-15129-N": // Iron Aces
            case "T-44904D-50": // Iron Aces
            case "T-1206N": // Jojo's Bizarre Adventure
            case "T-7007D-50": // Jojo's Bizarre Adventure
            case "T-44302N": // The King of Fighters '99 Evolution
            case "T-3101N": // The King of Fighters: Dream Match 1999
            case "T-44305N": // Last Blade II Heart of a Samarai
            case "T-10004N": // MTV Sports: Skateboarding feat. Andy McDonald
                /*case "T-10001D-50": // MTV Sports: Skateboarding feat. Andy McDonald*/
            case "T-17717D-50": // Next Tetris, The
            case "T-15103D-61": // Pen Pen
            case "T-1207N": // Plasma Sword
            case "T-7003D-50": // Plasma Sword
            case "T-31101N": // Psychic Force 2012
            case "T-8106D-05": // Psychic Force 2012
            case "T-40505D-50": // Railroad Tycoon II: Gold Edition
            case "T-36806D-05": // Resident Evil Code: Veronica
            case "T-15122N": //  Ring, The: Terror's Realm
            case "MK-51010-50": // Rippin' Riders
                /*case "MK-51052-50": // Skies of Arcadia*/
            case "T-41401N": // Soul Fighter
            case "T-41401D-61": // Soul Fighter
            case "T-36808N": // Sydney 2000
            case "T-8103N": // WWF Attitude
            case "T-8103D-50": // WWF Attitude
                vgaMode = 2;

            default:
                vgaMode = -1;

                // Unlicensed / Demo / No ID:
                /* =============================================================
                Sega Swirl - [PAL-E]
                Ball Breakers - [Beta] [NTSC-U]
                Flintstones, The: Viva Rock Vegas - [BETA]
                Half-Life & Blue Shift - [BETA]
                Hell Gate - [BETA]
                Propeller Arena - [BETA]
                System Shock 2 - [ALPHA]
                Cool Herders - [Unlicenced]
                Dream Para Para - [Unlicenced]
                DUX - [Unlicenced]
                Feet of Fury - [Unlicenced]
                Frog Feast - [Unlicenced]
                Inhabitants - [Unlicenced]
                Irides: Master of Blocks - [Unlicenced]
                Last Hope - [Unlicenced]
                Last Hope: Pink Bullets - [Unlicenced]
                Maqiupai - [Unlicenced]
                Rush Rush Rally Racing - [Unlicenced]
                Rush Rush Rally Racing (Deluxe Edition) - [Unlicenced]
                Super Boom Tread Troopers - [Unlicenced]
                Wind and Water: Puzzle Battles - [Unlicenced]
                ================================================================ */

                // Unknown / Untested Games:
                /* =============================================================
                PAL-E - Conflict Zone - GOT - [DCCM]
                NTSC-U - Donald Duck: Goin' Quackers - GOT - [DCCM/RDC]
                NTSC-U - ESPN International Track n Field - GOT -[DCCM]
                PAL-E - Millenium Soldier: Expendable (Expendable) - GOT - [DCP]
                PAL-E - Heavy Metal: Geomatrix - GOT - [DCP]
                NTSC-U - House of the Dead 2 - GOT - [DCCM]
                NTSC-U - Loony Toons Space Race - GOT - [DCRES/OVERRiDE]
                PAL-E - Midway Greatest Arcade Hits Volume 1 - GOT - [DCCM]
                PAL-E - Mortal Kombat Gold - GOT - [DCP]
                PAL-E - NBA Hoopz - GOT - [PULSAR]
                NTSC-U - NHL 2K - GOT - [DCCM]
                PAL-E Freestyle Scooter [Razor Freestyle Scooter]
                NTSC-U Record of Lodoss War
                NTSC-U Roadsters
                PAL-E San Fransisco Rush 2049
                NTSC-U Sega Bass Fishing
                NTSC-U Sega Rally Championship 2
                NTSC-U Silent Scope
                PAL-E Speed Devils Online Racing
                PAL-E Star Wars: Demolition
                PAL-E Star Wars: Episode 1 Jedi Power Battles
                PAL-E Star Wars: Episode 1 Racer
                PAL-E Street Fighter III: 3rd Strike
                PAL-E Suzuki Alstare Extreme Racing
                PAL-E Tech Romancer
                PAL-E Tokyo Highway Challenge 2 [Tokyo Extreme Racer 2]
                NTSC-U Toy Comander
                NTSC-U Toy Story 2: Buzz Lightyear to the Rescue!
                PAL-E TrickStyle
                PAL-E Urban Chaos
                NTSC-U Vigilante 8: 2nd Offense
                NTSC-U Wetrix+
                NTSC-U Xtreme Sports
                NTSC-U Zombie Revenge
                ================================================================ */

                // List provided by Zorlon
                // https://www.epforums.org/showthread.php?56169-Dreamcast-VGA-Compatability-list-amp-Guide
        }
        return vgaMode;
    }

    static {
        AppCompatDelegate.setCompatVectorFromResourcesEnabled(true);
    }
}
