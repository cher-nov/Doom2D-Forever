unit g_panel;

interface

uses
  MAPSTRUCT, BinEditor, g_textures;

type
  TAddTextureArray = Array of
    record
      Texture: Cardinal;
      Anim: Boolean;
    end;

  TPanel = Class (TObject)
  private
    FTextureWidth:    Word;
    FTextureHeight:   Word;
    FAlpha:           Byte;
    FBlending:        Boolean;
    FTextureIDs:      Array of
                        record
                          case Anim: Boolean of
                            False: (Tex: Cardinal);
                            True:  (AnTex: TAnimation);
                        end;

  public
    FCurTexture:      Integer; // ����� ������� ��������
    FCurFrame:        Integer;
    FCurFrameCount:   Byte;
    X, Y:             Integer;
    Width, Height:    Word;
    PanelType:        Word;
    SaveIt:           Boolean; // ��������� ��� SaveState?
    Enabled:          Boolean;
    Door:             Boolean;
    LiftType:         Byte;
    LastAnimLoop:     Byte;

    constructor Create(PanelRec: TPanelRec_1;
                       AddTextures: TAddTextureArray;
                       CurTex: Integer;
                       var Textures: TLevelTextureArray);
    destructor  Destroy(); override;

    procedure   Draw();
    procedure   Update();
    procedure   SetFrame(Frame: Integer; Count: Byte);
    procedure   NextTexture(AnimLoop: Byte = 0);
    procedure   SetTexture(ID: Integer; AnimLoop: Byte = 0);
    function    GetTextureID(): Cardinal;
    function    GetTextureCount(): Integer;

    procedure   SaveState(var Mem: TBinMemoryWriter);
    procedure   LoadState(var Mem: TBinMemoryReader);
  end;

  TPanelArray = Array of TPanel;

implementation

uses
  windows, g_basic, g_map, MAPDEF, g_game, e_graphics,
  g_console, g_language;

const
  PANEL_SIGNATURE = $4C4E4150; // 'PANL'

{ T P a n e l : }

constructor TPanel.Create(PanelRec: TPanelRec_1;
                          AddTextures: TAddTextureArray;
                          CurTex: Integer;
                          var Textures: TLevelTextureArray);
var
  i: Integer;
begin
  X := PanelRec.X;
  Y := PanelRec.Y;
  Width := PanelRec.Width;
  Height := PanelRec.Height;
  FAlpha := 0;
  FBlending := False;
  FCurFrame := 0;
  FCurFrameCount := 0;
  LastAnimLoop := 0;

// ��� ������:
  PanelType := PanelRec.PanelType;
  Enabled := True;
  Door := False;
  LiftType := 0;
  SaveIt := False;

  case PanelType of
    PANEL_OPENDOOR:
      begin
        Enabled := False;
        Door := True;
        SaveIt := True;
      end;
    PANEL_CLOSEDOOR:
      begin
        Door := True;
        SaveIt := True;
      end;
    PANEL_LIFTUP:
      SaveIt := True;
    PANEL_LIFTDOWN:
      begin
        LiftType := 1;
        SaveIt := True;
      end;
    PANEL_LIFTLEFT:
      begin
        LiftType := 2;
        SaveIt := True;
      end;
    PANEL_LIFTRIGHT:
      begin
        LiftType := 3;
        SaveIt := True;
      end;
  end;

// ���������:
  if ByteBool(PanelRec.Flags and PANEL_FLAG_HIDE) then
  begin
    SetLength(FTextureIDs, 0);
    FCurTexture := -1;
    Exit;
  end;
// ������, �� ������������ ��������:
  if ByteBool(PanelType and
    (PANEL_LIFTUP or
     PANEL_LIFTDOWN or
     PANEL_LIFTLEFT or
     PANEL_LIFTRIGHT or
     PANEL_BLOCKMON)) then
  begin
    SetLength(FTextureIDs, 0);
    FCurTexture := -1;
    Exit;
  end;

// ���� ��� �������� ��� �������� - ������������:
  if WordBool(PanelType and (PANEL_WATER or PANEL_ACID1 or PANEL_ACID2)) and
     (not ByteBool(PanelRec.Flags and PANEL_FLAG_WATERTEXTURES)) then
  begin
    SetLength(FTextureIDs, 1);
    FTextureIDs[0].Anim := False;

    case PanelRec.PanelType of
      PANEL_WATER:
        FTextureIDs[0].Tex := TEXTURE_SPECIAL_WATER;
      PANEL_ACID1:
        FTextureIDs[0].Tex := TEXTURE_SPECIAL_ACID1;
      PANEL_ACID2:
        FTextureIDs[0].Tex := TEXTURE_SPECIAL_ACID2;
    end;

    FCurTexture := 0;
    Exit;
  end;

  SetLength(FTextureIDs, Length(AddTextures));

  if CurTex < 0 then
    FCurTexture := -1
  else
    if CurTex >= Length(FTextureIDs) then
      FCurTexture := Length(FTextureIDs) - 1
    else
      FCurTexture := CurTex;

  for i := 0 to Length(FTextureIDs)-1 do
  begin
    FTextureIDs[i].Anim := AddTextures[i].Anim;
    if FTextureIDs[i].Anim then
      begin // ������������� ��������
        FTextureIDs[i].AnTex :=
           TAnimation.Create(Textures[AddTextures[i].Texture].FramesID,
                             True, Textures[AddTextures[i].Texture].Speed);
        FTextureIDs[i].AnTex.Blending := ByteBool(PanelRec.Flags and PANEL_FLAG_BLENDING);
        FTextureIDs[i].AnTex.Alpha := PanelRec.Alpha;
        SaveIt := True;
      end
    else
      begin // ������� ��������
        FTextureIDs[i].Tex := Textures[AddTextures[i].Texture].TextureID;
      end;
  end;

// ������� ��������� - ����� ��������� �������:
  if Length(FTextureIDs) > 1 then
    SaveIt := True;

// ���� �� ������������, �� ������ �������:
  if not g_Map_IsSpecialTexture(Textures[PanelRec.TextureNum].TextureName) then
  begin
    FTextureWidth := Textures[PanelRec.TextureNum].Width;
    FTextureHeight := Textures[PanelRec.TextureNum].Height;
    FAlpha := PanelRec.Alpha;
    FBlending := ByteBool(PanelRec.Flags and PANEL_FLAG_BLENDING);
  end;
end;

destructor TPanel.Destroy();
var
  i: Integer;
begin
  for i := 0 to High(FTextureIDs) do
    if FTextureIDs[i].Anim then
      FTextureIDs[i].AnTex.Free();
  SetLength(FTextureIDs, 0);

  Inherited;
end;

procedure TPanel.Draw();
var
  xx, yy: Integer;
  NoTextureID: DWORD;
  NW, NH: Word;
begin
  if Enabled and (FCurTexture >= 0) and
     (Width > 0) and (Height > 0) and (FAlpha < 255) and
     g_Collide(X, Y, Width, Height,
               sX, sY, sWidth, sHeight) then
  begin
    if FTextureIDs[FCurTexture].Anim then
      begin // ������������� ��������
        if FTextureIDs[FCurTexture].AnTex = nil then
          Exit;
  
        for xx := 0 to (Width div FTextureWidth)-1 do
          for yy := 0 to (Height div FTextureHeight)-1 do
            FTextureIDs[FCurTexture].AnTex.Draw(
              X + xx*FTextureWidth,
              Y + yy*FTextureHeight, M_NONE);
      end
    else
      begin // ������� ��������
        case FTextureIDs[FCurTexture].Tex of
          TEXTURE_SPECIAL_WATER:
            e_DrawFillQuad(X, Y, X+Width-1, Y+Height-1,
                           0, 0, 255, 0, B_FILTER);
          TEXTURE_SPECIAL_ACID1:
            e_DrawFillQuad(X, Y, X+Width-1, Y+Height-1,
                           0, 128, 0, 0, B_FILTER);
          TEXTURE_SPECIAL_ACID2:
            e_DrawFillQuad(X, Y, X+Width-1, Y+Height-1,
                           128, 0, 0, 0, B_FILTER);
          TEXTURE_NONE:
            if g_Texture_Get('NOTEXTURE', NoTextureID) then
            begin
              e_GetTextureSize(NoTextureID, @NW, @NH);
              e_DrawFill(NoTextureID, X, Y, Width div NW, Height div NH,
                         0, False, False);
            end else
            begin
              xx := X + (Width div 2);
              yy := Y + (Height div 2);
              e_DrawFillQuad(X, Y, xx, yy,
                             255, 0, 255, 0);
              e_DrawFillQuad(xx, Y, X+Width-1, yy,
                             255, 255, 0, 0);
              e_DrawFillQuad(X, yy, xx, Y+Height-1,
                             255, 255, 0, 0);
              e_DrawFillQuad(xx, yy, X+Width-1, Y+Height-1,
                             255, 0, 255, 0);
            end;

          else
            e_DrawFill(FTextureIDs[FCurTexture].Tex, X, Y,
                       Width div FTextureWidth,
                       Height div FTextureHeight,
                       FAlpha, True, FBlending);
        end;
      end;
  end;
end;

procedure TPanel.Update();
begin
  if Enabled and (FCurTexture >= 0) and
    (FTextureIDs[FCurTexture].Anim) and
    (FTextureIDs[FCurTexture].AnTex <> nil) and
    (Width > 0) and (Height > 0) and (FAlpha < 255) then
  begin
    FTextureIDs[FCurTexture].AnTex.Update();
    FCurFrame := FTextureIDs[FCurTexture].AnTex.CurrentFrame;
    FCurFrameCount := FTextureIDs[FCurTexture].AnTex.CurrentCounter;
  end;
end;

procedure TPanel.SetFrame(Frame: Integer; Count: Byte);

  function ClampInt(X, A, B: Integer): Integer;
  begin
    Result := X;
    if X < A then Result := A else if X > B then Result := B;
  end;

begin
  if Enabled and (FCurTexture >= 0) and
    (FTextureIDs[FCurTexture].Anim) and
    (FTextureIDs[FCurTexture].AnTex <> nil) and
    (Width > 0) and (Height > 0) and (FAlpha < 255) then
  begin
    FCurFrame := ClampInt(Frame, 0, FTextureIDs[FCurTexture].AnTex.TotalFrames);
    FCurFrameCount := Count;
    FTextureIDs[FCurTexture].AnTex.CurrentFrame := FCurFrame;
    FTextureIDs[FCurTexture].AnTex.CurrentCounter := FCurFrameCount;
  end;
end;

procedure TPanel.NextTexture(AnimLoop: Byte = 0);
begin
  Assert(FCurTexture >= -1, 'FCurTexture < -1');

// ��� �������:
  if Length(FTextureIDs) = 0 then
    FCurTexture := -1
  else
  // ������ ���� ��������:
    if Length(FTextureIDs) = 1 then
      begin
        if FCurTexture = 0 then
          FCurTexture := -1
        else
          FCurTexture := 0;
      end
    else
    // ������ ����� ��������:
      begin
      // ���������:
        Inc(FCurTexture);
      // ��������� ��� - ������� � ������:
        if FCurTexture >= Length(FTextureIDs) then
          FCurTexture := 0;
      end;

// ������������� �� ������� ����. ��������:
  if (FCurTexture >= 0) and FTextureIDs[FCurTexture].Anim then
  begin
    if (FTextureIDs[FCurTexture].AnTex = nil) then
    begin
      g_FatalError(_lc[I_GAME_ERROR_SWITCH_TEXTURE]);
      Exit;
    end;

    if AnimLoop = 1 then
      FTextureIDs[FCurTexture].AnTex.Loop := True
    else
      if AnimLoop = 2 then
        FTextureIDs[FCurTexture].AnTex.Loop := False;
        
    FTextureIDs[FCurTexture].AnTex.Reset();
  end;

  LastAnimLoop := AnimLoop;
end;

procedure TPanel.SetTexture(ID: Integer; AnimLoop: Byte = 0);
begin
// ��� �������:
  if Length(FTextureIDs) = 0 then
    FCurTexture := -1
  else
  // ������ ���� ��������:
    if Length(FTextureIDs) = 1 then
      begin
        if (ID = 0) or (ID = -1) then
          FCurTexture := ID;
      end
    else
    // ������ ����� ��������:
      begin
        if (ID >= -1) and (ID <= High(FTextureIDs)) then
          FCurTexture := ID;
      end;

// ������������� �� ������� ����. ��������:
  if (FCurTexture >= 0) and FTextureIDs[FCurTexture].Anim then
  begin
    if (FTextureIDs[FCurTexture].AnTex = nil) then
    begin
      g_FatalError(_lc[I_GAME_ERROR_SWITCH_TEXTURE]);
      Exit;
    end;

    if AnimLoop = 1 then
      FTextureIDs[FCurTexture].AnTex.Loop := True
    else
      if AnimLoop = 2 then
        FTextureIDs[FCurTexture].AnTex.Loop := False;
        
    FTextureIDs[FCurTexture].AnTex.Reset();
  end;

  LastAnimLoop := AnimLoop;
end;

function TPanel.GetTextureID(): DWORD;
begin
  Result := TEXTURE_NONE;

  if (FCurTexture >= 0) then
  begin
    if FTextureIDs[FCurTexture].Anim then
      Result := FTextureIDs[FCurTexture].AnTex.FramesID
    else
      Result := FTextureIDs[FCurTexture].Tex;
  end;
end;

function TPanel.GetTextureCount(): Integer;
begin
  Result := Length(FTextureIDs);
  if Enabled and (FCurTexture >= 0) then
     if (FTextureIDs[FCurTexture].Anim) and
        (FTextureIDs[FCurTexture].AnTex <> nil) and
        (Width > 0) and (Height > 0) and (FAlpha < 255) then
       Result := Result + 100;
end;

procedure TPanel.SaveState(Var Mem: TBinMemoryWriter);
var
  sig: DWORD;
  anim: Boolean;
begin
  if (not SaveIt) or (Mem = nil) then
    Exit;

// ��������� ������:
  sig := PANEL_SIGNATURE; // 'PANL'
  Mem.WriteDWORD(sig);
// �������/�������, ���� �����:
  Mem.WriteBoolean(Enabled);
// ����������� �����, ���� ����:
  Mem.WriteByte(LiftType);
// ����� ������� ��������:
  Mem.WriteInt(FCurTexture);
// ������������� �� ������� ��������:
  if (FCurTexture >= 0) and (FTextureIDs[FCurTexture].Anim) then
    begin
      Assert(FTextureIDs[FCurTexture].AnTex <> nil,
             'TPanel.SaveState: No animation object');
      anim := True;
    end
  else
    anim := False;
  Mem.WriteBoolean(anim);
// ���� �� - ��������� ��������:
  if anim then
    FTextureIDs[FCurTexture].AnTex.SaveState(Mem);
end;

procedure TPanel.LoadState(var Mem: TBinMemoryReader);
var
  sig: DWORD;
  anim: Boolean;
begin
  if (not SaveIt) or (Mem = nil) then
    Exit;

// ��������� ������:
  Mem.ReadDWORD(sig);
  if sig <> PANEL_SIGNATURE then // 'PANL'
  begin
    raise EBinSizeError.Create('TPanel.LoadState: Wrong Panel Signature');
  end;
// �������/�������, ���� �����:
  Mem.ReadBoolean(Enabled);
// ����������� �����, ���� ����:
  Mem.ReadByte(LiftType);
// ����� ������� ��������:
  Mem.ReadInt(FCurTexture);
// ������������� �� ������� ��������:
  Mem.ReadBoolean(anim);
// ���� �� - ��������� ��������:
  if anim then
  begin
    Assert((FCurTexture >= 0) and
           (FTextureIDs[FCurTexture].Anim) and
           (FTextureIDs[FCurTexture].AnTex <> nil),
           'TPanel.LoadState: No animation object');
    FTextureIDs[FCurTexture].AnTex.LoadState(Mem);
  end;
end;

end.
