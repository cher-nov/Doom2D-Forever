(* Copyright (C) 2016 - The Doom2D.org team & involved community members <http://www.doom2d.org>.
 * This file is part of Doom2D Forever.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of
 * the GNU General Public License as published by the Free Software Foundation, version 3 of
 * the License ONLY.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 *)

interface

uses
{$IFDEF USE_MEMPOOL}mempool,{$ENDIF}
{$IFDEF USE_SDL}
  SDL, SDL_mixer,
  {$DEFINE SDL1MIXER}
  {$UNDEF SDL2MIXER}
{$ELSE}
  SDL2, SDL2_mixer,
  {$DEFINE SDL2MIXER}
  {$UNDEF SDL1MIXER}
{$ENDIF}
  envvars, e_log, SysUtils;

type
  TSoundID = PtrInt;

  TSoundRec = record
    Data: Pointer;
    Sound: PMix_Chunk;
    Music: PMix_Music;
    isMusic: Boolean;
    Loops: Boolean;
    nRefs: Integer;
  end;

  TBasicSound = class{$IFDEF USE_MEMPOOL}(TPoolObject){$ENDIF}
  private
    FChanNum: Integer; // <0: no channel allocated

  protected
    FID: TSoundID;
    FMusic: Boolean;
    FPosition: DWORD;
    FPriority: Integer;

    function RawPlay(Pan: Single; Volume: Single; aPos: DWORD): Boolean;
    function GetChan (): Integer;

    property Channel: Integer read GetChan;

  public
    constructor Create();
    destructor Destroy(); override;
    procedure SetID(ID: TSoundID);
    procedure FreeSound();
    function IsPlaying(): Boolean;
    procedure Stop();
    function IsPaused(): Boolean;
    procedure Pause(Enable: Boolean);
    function GetVolume(): Single;
    procedure SetVolume(Volume: Single);
    function GetPan(): Single;
    procedure SetPan(Pan: Single);
    function IsMuted(): Boolean;
    procedure Mute(Enable: Boolean);
    function GetPosition(): DWORD;
    procedure SetPosition(aPos: DWORD);
    procedure SetPriority(priority: Cardinal);
  end;

const
  NO_SOUND_ID = TSoundID(-1);

function e_InitSoundSystem(NoOutput: Boolean = False): Boolean;

function e_LoadSound(FileName: String; var ID: TSoundID; isMusic: Boolean;
  ForceNoLoop: Boolean = False): Boolean;
function e_LoadSoundMem(pData: Pointer; Length: Integer; var ID: TSoundID; isMusic: Boolean;
  ForceNoLoop: Boolean = False): Boolean;

// returns channel number or -1
function e_PlaySound(ID: TSoundID): Integer;
function e_PlaySoundPan(ID: TSoundID; Pan: Single): Integer;
function e_PlaySoundVolume(ID: TSoundID; Volume: Single): Integer;
function e_PlaySoundPanVolume(ID: TSoundID; Pan, Volume: Single): Integer;

procedure e_ModifyChannelsVolumes(SoundMod: Single; setMode: Boolean);
procedure e_MuteChannels(Enable: Boolean);
procedure e_StopChannels();

procedure e_DeleteSound(ID: TSoundID);
procedure e_RemoveAllSounds();
procedure e_ReleaseSoundSystem();
procedure e_SoundUpdate();

var
  e_SoundsArray: array of TSoundRec;

  e_TimidityDecoder: Boolean; (* sdl_mixer special *)
  e_SoundFont: String = 'data/banks/default.sf2';

implementation

uses
  g_window, g_options;

const
  N_CHANNELS = 512;
  N_MUSCHAN = N_CHANNELS+42;

type
  TChanInfo = record
    id: TSoundID;
    muted: Boolean;
    oldvol: Integer; // for muted
    pan: Single;
  end;

var
  SoundMuted: Boolean = False;
  SoundInitialized: Boolean = False;
  ChanSIds: array[0..N_CHANNELS] of TChanInfo;
  MusVolume: Integer = MIX_MAX_VOLUME;


procedure chanFinished (chan: Integer); cdecl;
begin
  //e_WriteLog(Format('chanFinished: %d', [chan]), TMsgType.Notify);
  if (chan >= 0) and (chan < N_CHANNELS) then
  begin
    if ChanSIds[chan].id <> NO_SOUND_ID then
    begin
      if (ChanSIds[chan].id <= High(e_SoundsArray)) and (e_SoundsArray[ChanSIds[chan].id].nRefs > 0) then
      begin
        Dec(e_SoundsArray[ChanSIds[chan].id].nRefs);
      end;
      ChanSIds[chan].id := NO_SOUND_ID;
    end;
  end;
end;


procedure dumpMusicType (ms: PMix_Music);
begin
  if ms = nil then
  begin
    e_WriteLog('MUSIC FORMAT: NONE', TMsgType.Notify);
  end
  else
  begin
    case Mix_GetMusicType(ms^) of
      TMix_MusicType.MUS_NONE:
        e_WriteLog('MUSIC FORMAT: NONE', TMsgType.Notify);
      TMix_MusicType.MUS_CMD:
        e_WriteLog('MUSIC FORMAT: CMD', TMsgType.Notify);
      TMix_MusicType.MUS_WAV:
        e_WriteLog('MUSIC FORMAT: WAV', TMsgType.Notify);
      TMix_MusicType.MUS_MOD:
        e_WriteLog('MUSIC FORMAT: MOD', TMsgType.Notify);
      TMix_MusicType.MUS_MID:
        e_WriteLog('MUSIC FORMAT: MID', TMsgType.Notify);
      TMix_MusicType.MUS_OGG:
        e_WriteLog('MUSIC FORMAT: OGG', TMsgType.Notify);
      TMix_MusicType.MUS_MP3:
        e_WriteLog('MUSIC FORMAT: MP3', TMsgType.Notify);
      TMix_MusicType.MUS_MP3_MAD:
        e_WriteLog('MUSIC FORMAT: MP3_MAD', TMsgType.Notify);
      TMix_MusicType.MUS_FLAC:
        e_WriteLog('MUSIC FORMAT: FLAC', TMsgType.Notify);
      TMix_MusicType.MUS_MODPLUG:
        e_WriteLog('MUSIC FORMAT: MODPLUG', TMsgType.Notify);
      otherwise
        e_WriteLog('MUSIC FORMAT: UNKNOWN', TMsgType.Notify);
    end;
  end;
end;

function e_InitSoundSystem(NoOutput: Boolean = False): Boolean;
var
  res, i: Integer;
  rfreq: Integer;
  rformat: UInt16;
  rchans: Integer;
  flags: Integer;
begin
  if SoundInitialized or NoOutput then Exit(True);
  Result := False;

  // wow, this is actually MIDI player!
  // we need module player
  flags := MIX_INIT_FLAC or MIX_INIT_MOD or MIX_INIT_MP3 or MIX_INIT_OGG or MIX_INIT_FLUIDSYNTH;
  {$IFDEF SDL2MIXER}
    flags := flags or MIX_INIT_MODPLUG;
  {$ENDIF}
  res := Mix_Init(flags);
  e_WriteLog(Format('SDL: res=0x%x', [res]), TMsgType.Notify);
  if (res and MIX_INIT_FLAC) <> 0 then e_WriteLog('SDL: FLAC playback is active', TMsgType.Notify);
  if (res and MIX_INIT_MOD) <> 0 then e_WriteLog('SDL: MOD playback is active', TMsgType.Notify);
  {$IFDEF SDL2MIXER}
    if (res and MIX_INIT_MODPLUG) <> 0 then e_WriteLog('SDL: MODPLUG playback is active', TMsgType.Notify);
  {$ENDIF}
  if (res and MIX_INIT_MP3) <> 0 then e_WriteLog('SDL: MP3 playback is active', TMsgType.Notify);
  if (res and MIX_INIT_OGG) <> 0 then e_WriteLog('SDL: OGG playback is active', TMsgType.Notify);
  if (res and MIX_INIT_FLUIDSYNTH) <> 0 then e_WriteLog('SDL: FLUIDSYNTH playback is active', TMsgType.Notify);

  e_WriteLog(Format('SDL: initializing mixer at %d with buffer %d', [gsSDLSampleRate, gsSDLBufferSize]), TMsgType.Notify);
  res := Mix_OpenAudio(gsSDLSampleRate, MIX_DEFAULT_FORMAT, 2, gsSDLBufferSize);
  if res = -1 then
  begin
    e_WriteLog('Error initializing SDL mixer:', TMsgType.Fatal);
    e_WriteLog(Mix_GetError(), TMsgType.Fatal);
    Exit;
  end;

  if Mix_QuerySpec(@rfreq, @rformat, @rchans) > 0 then
  begin
    e_WriteLog(Format('SDL: frequency=%d; format=%u; channels=%d', [rfreq, rformat, rchans]), TMsgType.Notify);
  end;

  for i := 0 to Mix_GetNumChunkDecoders()-1 do
  begin
    e_WriteLog(Format('SDL: chunk decoder %s is avalable', [Mix_GetChunkDecoder(i)]), TMsgType.Notify);
  end;

  e_TimidityDecoder := false;
  for i := 0 to Mix_GetNumMusicDecoders()-1 do
  begin
    case AnsiString(Mix_GetMusicDecoder(i)) of
      'TIMIDITY': e_TimidityDecoder := true;
    end;
    e_WriteLog(Format('SDL: music decoder %s is avalable', [Mix_GetMusicDecoder(i)]), TMsgType.Notify);
  end;

  Mix_AllocateChannels(N_CHANNELS);
  Mix_ChannelFinished(chanFinished);

  for i := 0 to N_CHANNELS-1 do
  begin
    ChanSIds[i].id := NO_SOUND_ID;
    ChanSIds[i].muted := SoundMuted;
    ChanSIds[i].oldvol := MIX_MAX_VOLUME;
    ChanSIds[i].pan := 1.0;
  end;
  MusVolume := MIX_MAX_VOLUME;

  SoundInitialized := True;
  Result := True;
end;

function e_isMusic (id: TSoundID): Boolean;
begin
  Result := False;
  if (e_SoundsArray <> nil) and (id <= High(e_SoundsArray)) then
  begin
    Result := (e_SoundsArray[id].Music <> nil);
  end;
end;

function e_isSound (id: TSoundID): Boolean;
begin
  Result := False;
  if (e_SoundsArray <> nil) and (id <= High(e_SoundsArray)) then
  begin
    Result := (e_SoundsArray[id].Sound <> nil);
  end;
end;

function FindESound(): TSoundID;
var
  i: TSoundID;
begin
  if e_SoundsArray <> nil then
  begin
    for i := 0 to High(e_SoundsArray) do
      if (e_SoundsArray[i].Sound = nil) and (e_SoundsArray[i].Music = nil) then
      begin
        Result := i;
        Exit;
      end;
  end;
  if e_SoundsArray = nil then
  begin
    SetLength(e_SoundsArray, 16);
    Result := 0;
  end
  else
  begin
    Result := High(e_SoundsArray) + 1;
    SetLength(e_SoundsArray, Length(e_SoundsArray) + 16);
  end;
end;

function e_LoadSound(FileName: String; var ID: TSoundID; isMusic: Boolean;
  ForceNoLoop: Boolean): Boolean;
var
  find_id: TSoundID;
begin
  ID := NO_SOUND_ID;
  Result := False;
  if not SoundInitialized then Exit;

  if isMusic then e_WriteLog('Loading music '+FileName+'...', TMsgType.Notify)
  else e_WriteLog('Loading sound '+FileName+'...', TMsgType.Notify);

  {
  if isMusic then
  begin
    e_WriteLog('IGNORING MUSIC FROM FILE', TMsgType.Warning);
    Exit;
  end;
  }

  find_id := FindESound();

  e_SoundsArray[find_id].Data := nil;
  e_SoundsArray[find_id].isMusic := isMusic;
  e_SoundsArray[find_id].Loops := isMusic and not ForceNoLoop;
  e_SoundsArray[find_id].nRefs := 0;

  if isMusic then
  begin
    e_WriteLog(Format('  MUSIC SLOT: %d', [find_id]), TMsgType.Notify);
    e_SoundsArray[find_id].Music := Mix_LoadMUS(PAnsiChar(FileName));
    if e_SoundsArray[find_id].Music = nil then
    begin
      e_WriteLog(Format('ERROR LOADING MUSIC: %d', [find_id]), TMsgType.Warning);
      e_WriteLog(Mix_GetError(), TMsgType.Warning);
      Exit;
    end;
    dumpMusicType(e_SoundsArray[find_id].Music);
  end
  else
  begin
    e_SoundsArray[find_id].Sound := Mix_LoadWAV(PAnsiChar(FileName));
    if e_SoundsArray[find_id].Sound = nil then Exit;
  end;

  ID := find_id;
  Result := True;
end;

function e_LoadSoundMem(pData: Pointer; Length: Integer; var ID: TSoundID; isMusic: Boolean;
  ForceNoLoop: Boolean): Boolean;
var
  find_id: TSoundID;
  rw: PSDL_RWops;
  //pc: PChar;
  isid3: Boolean;
begin
  ID := NO_SOUND_ID;
  Result := False;
  if not SoundInitialized then Exit;
  isid3 := False;

  {
  if isMusic then
  begin
    e_WriteLog('IGNORING MUSIC FROM MEMORY', TMsgType.Warning);
    Exit;
  end;
  }

  //FIXME: correctly skip ID3
  {
  pc := PChar(pData);
  if (Length > $400) and (pc[0] = 'I') and (pc[1] = 'D') and (pc[2] = '3') then
  begin
    isid3 := True;
    Inc(pc, $400);
    pData := Pointer(pc);
    Dec(Length, $400);
    e_WriteLog('MUSIC: MP3 ID3 WORKAROUND APPLIED!', TMsgType.Warning);
  end;
  }

  rw := SDL_RWFromConstMem(pData, Length);
  if rw = nil then Exit;

  find_id := FindESound();

  e_SoundsArray[find_id].Data := pData;
  if isid3 then e_SoundsArray[find_id].Data := nil;
  e_SoundsArray[find_id].isMusic := isMusic;
  e_SoundsArray[find_id].Loops := isMusic and not ForceNoLoop;
  e_SoundsArray[find_id].nRefs := 0;

  if isMusic then
  begin
    e_WriteLog(Format('  MUSIC SLOT: %d', [find_id]), TMsgType.Notify);
    e_SoundsArray[find_id].Music := Mix_LoadMUS_RW(rw {$IFNDEF SDL1MIXER}, 0{$ENDIF} );
    if e_SoundsArray[find_id].Music = nil then
    begin
      e_WriteLog(Format('ERROR LOADING MUSIC: %d', [find_id]), TMsgType.Warning);
      e_WriteLog(Mix_GetError(), TMsgType.Warning);
    end
    else
    begin
      dumpMusicType(e_SoundsArray[find_id].Music);
    end;
    //SDL_FreeRW(rw);
    {
    if e_SoundsArray[find_id].Music <> nil then
    begin
      Mix_FreeMusic(e_SoundsArray[find_id].Music);
    end;
    e_SoundsArray[find_id].Music := nil;
    Exit;
    }
  end
  else
  begin
    e_SoundsArray[find_id].Sound := Mix_LoadWAV_RW(rw, 0);
  end;
  //SDL_FreeRW(rw); // somehow it segfaults...
  if (e_SoundsArray[find_id].Sound = nil) and (e_SoundsArray[find_id].Music = nil) then
  begin
    e_SoundsArray[find_id].Data := nil;
    Exit;
  end;

  ID := find_id;
  Result := True;
end;

function e_PlaySound (ID: TSoundID): Integer;
var
  res: Integer = -1;
  loops: Integer = 0;
begin
  Result := -1;
  if not SoundInitialized then Exit;

  if e_isSound(ID) then
  begin
    if e_SoundsArray[ID].nRefs >= gMaxSimSounds then Exit;
    Inc(e_SoundsArray[ID].nRefs);
    if e_SoundsArray[ID].Loops then loops := -1;
    res := Mix_PlayChannel(-1, e_SoundsArray[ID].Sound, loops);
    if res >= 0 then
    begin
      ChanSIds[res].id := ID;
      ChanSIds[res].muted := SoundMuted;
      if SoundMuted then Mix_Volume(res, 0) else Mix_Volume(res, ChanSIds[res].oldvol);
      {
      if e_SoundsArray[ID].isMusic then
        res := Mix_PlayChannel(-1, e_SoundsArray[ID].Sound, -1)
      else
        res := Mix_PlayChannel(-1, e_SoundsArray[ID].Sound, 0);
      }
    end;
  end
  else
  begin
    if not e_isMusic(ID) then Exit;
    Mix_HaltMusic();
    if e_SoundsArray[ID].Loops then loops := -1;
    res := Mix_PlayMusic(e_SoundsArray[ID].Music, loops);
    if res >= 0 then res := N_MUSCHAN;
    if SoundMuted then Mix_VolumeMusic(0) else Mix_VolumeMusic(MusVolume);
    Result := res;
  end;

  Result := res;
end;

function e_chanSetPan (chan: Integer; Pan: Single): Boolean;
var
  l, r: UInt8;
begin
  Result := True;
  if chan = N_MUSCHAN then
  begin
    // no panning for music
  end
  else if chan >= 0 then
  begin
    if Pan < -1.0 then Pan := -1.0 else if Pan > 1.0 then Pan := 1.0;
    Pan := Pan+1.0; // 0..2
    l := trunc(127.0*(2.0-Pan));
    r := trunc(127.0*Pan);
    Mix_SetPanning(chan, l, r);
    ChanSIds[chan].pan := Pan;
  end
  else
  begin
    Result := False;
  end;
end;

function e_chanSetVol (chan: Integer; Volume: Single): Boolean;
var
  vol: Integer;
begin
  Result := True;
  if Volume < 0 then Volume := 0 else if Volume > 1 then Volume := 1;
  vol := trunc(Volume*MIX_MAX_VOLUME);
  if chan = N_MUSCHAN then
  begin
    MusVolume := vol;
    if SoundMuted then Mix_VolumeMusic(0) else Mix_VolumeMusic(vol);
  end
  else if chan >= 0 then
  begin
    ChanSIds[chan].oldvol := vol;
    if ChanSIds[chan].muted then Mix_Volume(chan, 0) else Mix_Volume(chan, vol);
  end
  else
  begin
    Result := False;
  end;
end;

function e_PlaySoundPan(ID: TSoundID; Pan: Single): Integer;
var
  chan: Integer;
begin
  Result := -1;
  chan := e_PlaySound(ID);
  e_chanSetPan(chan, Pan);
  Result := chan;
end;

function e_PlaySoundVolume(ID: TSoundID; Volume: Single): Integer;
var
  chan: Integer;
begin
  Result := -1;
  chan := e_PlaySound(ID);
  e_chanSetVol(chan, Volume);
  Result := chan;
end;

function e_PlaySoundPanVolume(ID: TSoundID; Pan, Volume: Single): Integer;
var
  chan: Integer;
begin
  Result := -1;
  chan := e_PlaySound(ID);
  e_chanSetPan(chan, Pan);
  e_chanSetVol(chan, Volume);
  Result := chan;
end;

procedure e_DeleteSound(ID: TSoundID);
var
  i: Integer;
begin
  if ID > High(e_SoundsArray) then Exit;
  if (e_SoundsArray[ID].Sound = nil) and (e_SoundsArray[ID].Music = nil) then Exit;

  for i := 0 to N_CHANNELS-1 do
  begin
    if ChanSIds[i].id = ID then
    begin
      ChanSIds[i].id := NO_SOUND_ID;
      Mix_HaltChannel(i);
    end;
  end;

  if e_SoundsArray[ID].Sound <> nil then Mix_FreeChunk(e_SoundsArray[ID].Sound);
  if e_SoundsArray[ID].Music <> nil then Mix_FreeMusic(e_SoundsArray[ID].Music);
  FreeMem(e_SoundsArray[ID].Data);

  e_SoundsArray[ID].Sound := nil;
  e_SoundsArray[ID].Music := nil;
  e_SoundsArray[ID].Data := nil;
  e_SoundsArray[ID].nRefs := 0;
end;

procedure e_ModifyChannelsVolumes(SoundMod: Single; setMode: Boolean);
var
  i: Integer;
  vol: Single;
  ovol: Integer;
begin
  for i := 0 to N_CHANNELS-1 do
  begin
    ovol := ChanSIds[i].oldvol;
    if setMode then
    begin
      vol := SoundMod;
    end
    else
    begin
      vol := (MIX_MAX_VOLUME+0.0)/ovol;
      vol := vol*SoundMod;
    end;
    if vol < 0 then vol := 0 else if vol > 1 then vol := 1;
    ChanSIds[i].oldvol := trunc(vol*MIX_MAX_VOLUME);
    //if i = 0 then e_WriteLog(Format('modifying volumes: vol=%f; newvol=%d', [vol, ChanSIds[i].oldvol]), TMsgType.Warning);
    if ChanSIds[i].muted then Mix_Volume(i, 0) else Mix_Volume(i, ChanSIds[i].oldvol);
  end;
  ovol := Mix_VolumeMusic(-1);
  if ovol >= 0 then
  begin
    if setMode then
    begin
      vol := SoundMod;
    end
    else
    begin
      vol := (MIX_MAX_VOLUME+0.0)/ovol;
      vol := vol * SoundMod;
    end;
    if vol < 0 then vol := 0 else if vol > 1 then vol := 1;
    MusVolume := trunc(vol*MIX_MAX_VOLUME);
    if SoundMuted then Mix_VolumeMusic(0) else Mix_VolumeMusic(MusVolume);
  end;
end;

procedure e_MuteChannels(Enable: Boolean);
var
  i: Integer;
begin
  //if Enable = SoundMuted then Exit;
  SoundMuted := Enable;
  for i := 0 to N_CHANNELS-1 do
  begin
    if ChanSIds[i].muted <> SoundMuted then
    begin
      ChanSIds[i].muted := SoundMuted;
      //e_WriteLog(Format('gmuting sound for channel %d', [i]), TMsgType.Warning);
      if ChanSIds[i].muted then Mix_Volume(i, 0) else Mix_Volume(i, ChanSIds[i].oldvol);
    end;
  end;
  //if SoundMuted then e_WriteLog('muting music', TMsgType.Notify) else e_WriteLog(Format('unmuting music (%d)', [MusVolume]), TMsgType.Notify);
  if SoundMuted then Mix_VolumeMusic(0) else Mix_VolumeMusic(MusVolume);
end;

procedure e_StopChannels();
var
  i: Integer;
begin
  Mix_HaltChannel(-1);
  Mix_HaltMusic();
  for i := 0 to High(e_SoundsArray) do e_SoundsArray[i].nRefs := 0;
  for i := 0 to N_CHANNELS-1 do ChanSIds[i].id := NO_SOUND_ID;
end;

procedure e_RemoveAllSounds();
var
  i: Integer;
begin
  if SoundInitialized then e_StopChannels();
  for i := 0 to High(e_SoundsArray) do e_DeleteSound(i);
  SetLength(e_SoundsArray, 0);
  e_SoundsArray := nil;
end;

procedure e_ReleaseSoundSystem();
begin
  e_RemoveAllSounds();
  if SoundInitialized then
  begin
    Mix_CloseAudio();
    SoundInitialized := False;
  end;
end;

procedure e_SoundUpdate();
begin
  //FMOD_System_Update(F_System);
end;


{ TBasicSound: }

constructor TBasicSound.Create();
begin
  FID := NO_SOUND_ID;
  FMusic := False;
  FChanNum := -1;
  FPosition := 0;
  FPriority := 128;
end;

destructor TBasicSound.Destroy();
begin
  FreeSound();
  inherited;
end;

function TBasicSound.GetChan (): Integer;
begin
  if (FID <> NO_SOUND_ID) and (FChanNum >= 0) and (FChanNum < N_CHANNELS) then
  begin
    if ChanSIds[FChanNum].id <> FID then FChanNum := -1;
  end
  else if e_isMusic(FID) then
  begin
    FChanNum := N_MUSCHAN;
  end;
  Result := FChanNum;
end;

procedure TBasicSound.FreeSound();
begin
  if FID = NO_SOUND_ID then Exit;
  Stop();
  FID := NO_SOUND_ID;
  FMusic := False;
  FPosition := 0;
  FChanNum := -1;
end;

// aPos: msecs
function TBasicSound.RawPlay(Pan: Single; Volume: Single; aPos: DWORD): Boolean;
begin
  Result := False;
  if (FID = NO_SOUND_ID) or not SoundInitialized then Exit;
  FChanNum := e_PlaySoundPanVolume(FID, Pan, Volume);
  Result := (FChanNum >= 0);
  //if e_isMusic(FID) then e_WriteLog(Format('playing music (%u)', [FID]), TMsgType.Notify);
  //TODO: aPos
end;

procedure TBasicSound.SetID(ID: TSoundID);
begin
  FreeSound();
  FID := ID;
  if ID <> NO_SOUND_ID then
  begin
    FMusic := e_SoundsArray[ID].isMusic;
  end;
  FChanNum := -1;
end;

function TBasicSound.IsPlaying(): Boolean;
var
  chan: Integer;
begin
  Result := False;
  if e_isSound(FID) then
  begin
    //e_WriteLog(Format('IsPlaying: FID=%u; FChanNum=%d', [FID, FChanNum]), TMsgType.Warning);
    chan := Channel;
    if chan < 0 then
    begin
      //e_WriteLog(Format('IsPlaying: FID=%u; FALSE', [FID]), TMsgType.Warning);
      Exit;
    end;
    //Result := (Mix_Playing(chan) > 0)
    //e_WriteLog(Format('IsPlaying: FID=%u; TRUE', [FID]), TMsgType.Warning);
    Result := True;
  end
  else if e_isMusic(FID) then
  begin
    Result := (Mix_PlayingMusic() > 0);
  end;
end;

procedure TBasicSound.Stop();
var
  chan: Integer;
begin
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan >= 0 then
    begin
      //GetPosition();
      Mix_HaltChannel(chan);
    end;
  end
  else if e_isMusic(FID) then
  begin
    Mix_HaltMusic();
  end;
  FChanNum := -1;
end;

function TBasicSound.IsPaused(): Boolean;
var
  chan: Integer;
begin
  Result := False;
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    Result := (Mix_Paused(chan) > 0);
  end
  else if e_isMusic(FID) then
  begin
    Result := (Mix_PausedMusic() > 0);
  end;
end;

procedure TBasicSound.Pause(Enable: Boolean);
var
  chan: Integer;
  pl: Boolean;
begin
  Enable := not Enable; // fuckin' double negation
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    pl := not (Mix_Paused(chan) > 0);
    if pl <> Enable then
    begin
      if Enable then Mix_Resume(chan) else Mix_Pause(chan);
    end;
  end
  else if e_isMusic(FID) then
  begin
    pl := not (Mix_PausedMusic() > 0);
    if pl <> Enable then
    begin
      if Enable then Mix_ResumeMusic() else Mix_PauseMusic();
    end;
  end;
  {
  if Enable then
  begin
    res := FMOD_Channel_GetPosition(FChanNum, FPosition, FMOD_TIMEUNIT_MS);
    if res <> FMOD_OK then
    begin
    end;
  end;
  }
end;

function TBasicSound.GetVolume(): Single;
var
  chan: Integer;
begin
  Result := 0.0;
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    Result := (ChanSIds[chan].oldvol+0.0)/(MIX_MAX_VOLUME+0.0);
  end
  else if e_isMusic(FID) then
  begin
    Result := (MusVolume+0.0)/(MIX_MAX_VOLUME+0.0);
  end;
end;

procedure TBasicSound.SetVolume(Volume: Single);
var
  chan: Integer;
begin
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    //e_WriteLog(Format('SetVolume: chan=%d; Volume=%f', [chan, Volume]), TMsgType.Warning);
    e_chanSetVol(chan, Volume);
  end
  else if e_isMusic(FID) then
  begin
    //e_WriteLog(Format('SetVolume: chan=MUSIC; Volume=%f', [Volume]), TMsgType.Warning);
    e_chanSetVol(N_MUSCHAN, Volume);
  end;
end;

function TBasicSound.GetPan(): Single;
var
  chan: Integer;
begin
  Result := 1.0;
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    Result := ChanSIds[chan].pan;
  end;
end;

procedure TBasicSound.SetPan(Pan: Single);
var
  chan: Integer;
begin
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    e_chanSetPan(chan, Pan);
  end;
end;

function TBasicSound.IsMuted(): Boolean;
var
  chan: Integer;
begin
  Result := False;
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    Result := ChanSIds[chan].muted;
  end
  else if e_isMusic(FID) then
  begin
    Result := SoundMuted;
  end;
end;

procedure TBasicSound.Mute(Enable: Boolean);
var
  chan: Integer;
begin
  if e_isSound(FID) then
  begin
    chan := Channel;
    if chan < 0 then Exit;
    if ChanSIds[chan].muted <> Enable then
    begin
      //e_WriteLog(Format('muting sound for channel %d', [cnan]), TMsgType.Warning);
      ChanSIds[chan].muted := Enable;
      if ChanSIds[chan].muted then Mix_Volume(chan, 0) else Mix_Volume(chan, ChanSIds[chan].oldvol);
    end;
  end
  else if e_isMusic(FID) then
  begin
    if Enable then Mix_VolumeMusic(0) else Mix_VolumeMusic(MusVolume);
  end;
end;

//TODO
function TBasicSound.GetPosition(): DWORD;
begin
  Result := 0;
{
  if FChanNum < 0 then Exit;
  res := FMOD_Channel_GetPosition(FChanNum, FPosition, FMOD_TIMEUNIT_MS);
  if res <> FMOD_OK then
  begin
    Exit;
  end;
  Result := FPosition;
}
end;

//TODO
procedure TBasicSound.SetPosition(aPos: DWORD);
begin
  FPosition := aPos;
{
  if FChanNum < 0 then Exit;
  res := FMOD_Channel_SetPosition(FChanNum, FPosition, FMOD_TIMEUNIT_MS);
  if res <> FMOD_OK then
  begin
  end;
}
end;

//TODO
procedure TBasicSound.SetPriority(priority: Cardinal);
begin
{
  if (FChanNum <> nil) and (FPriority <> priority) and (priority <= 256) then
  begin
    FPriority := priority;
    res := FMOD_Channel_SetPriority(FChanNum, priority);
    if res <> FMOD_OK then
    begin
    end;
  end;
}
end;

end.
