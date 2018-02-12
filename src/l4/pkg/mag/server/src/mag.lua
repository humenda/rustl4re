-------------------------------------------------
-- exported functions used by the mag logic
-------------------------------------------------

local Mag = Mag
local string = require "string"
local table = require "table"

Mag.user_state = Mag.get_user_state();

-- handle an incoming event
function handle_event(ev)
  -- print("handle_event", ev[1], ev[2], ev[3], ev[4], ev[5], ev[6]);
  -- 1: type, 2: code, 3: value, 4: time, 5: device_id, 6: source
  local l4re_syn_stream_cfg = (ev[1] == 0 and ev[2] == 0x80);
  local l4re_close_stream = l4re_syn_stream_cfg and ev[3] == 1;
  local source = ev[6];
  local stream = ev[5];
  local ddh = Mag.sources[source];
  if not ddh then
    if (l4re_close_stream) then
      return
    end
    Mag.sources[source] = {};
    ddh =  Mag.sources[source];
  end

  local dh = ddh[stream];
  if not dh then
    if l4re_close_stream then
      return
    end
    dh = Mag.create_input_device(source, stream);
  end

  -- print ("EV: ", ev[1], ev[2], ev[3], ev[4], ev[5], ev[6]);
  -- print(string.format("src=[%s:%s]: type=%x code=%x val=%x (time=%d)", tostring(ev[6]), tostring(ev[5]), ev[1], ev[2], ev[3], ev[4]));
  dh:process_event(ev);
  if l4re_close_stream then
    dh:delete();
  end
end

-- mag requests infos about a specific input event stream
function input_source_info(global_id)
  -- print "input_source_info";
  local device = Mag.streams[global_id];
  if not device then
    return -22;
  end

  return device.handler.get_stream_info(device);
end

-- mag requests infos about a specific input event stream
function input_source_abs_info(global_id, ...)
  -- print "input_source_abs_info";
  local device = Mag.streams[global_id];
  if not device then
    return -22;
  end

  return device.handler.get_abs_info(device, ...);
end



-------------------------------------------------
-- The internals of our event handling module
-------------------------------------------------
Mag.Mag = Mag;
Mag._G = _G;
local globals = _G;
local Mag_mt = getmetatable(Mag);
local orig_Mag_index = Mag_mt.__index;
local function Mag_index(table, key)
  local o = orig_Mag_index(table, key);
  if (not o) then
    o = globals[key];
  end
  return o;
end
Mag_mt.__index = Mag_index;
_ENV = Mag;

-- mag will initialize the variable 'user_state' to refer to
-- mag's user state object.

Event = {
  Type   = 1,
  Code   = 2,
  Value  = 3,
  Time   = 4,
  Device = 5,
  Stream = 6,

  SYN = 0;
  KEY = 1;
  REL = 2;
  ABS = 3;
  MSC = 4;
  SW  = 5;

  Syn = {
    REPORT    = 0,
    CONFIG    = 1,
    MT_REPORT = 2,
  };

  Key = {
    RESERVED         = 0,
    ESC              = 1,
    KEY_1            = 2,
    KEY_2            = 3,
    KEY_3            = 4,
    KEY_4            = 5,
    KEY_5            = 6,
    KEY_6            = 7,
    KEY_7            = 8,
    KEY_8            = 9,
    KEY_9            = 10,
    KEY_0            = 11,
    MINUS            = 12,
    EQUAL            = 13,
    BACKSPACE        = 14,
    TAB              = 15,
    Q                = 16,
    W                = 17,
    E                = 18,
    R                = 19,
    T                = 20,
    Y                = 21,
    U                = 22,
    I                = 23,
    O                = 24,
    P                = 25,
    LEFTBRACE        = 26,
    RIGHTBRACE       = 27,
    ENTER            = 28,
    LEFTCTRL         = 29,
    A                = 30,
    S                = 31,
    D                = 32,
    F                = 33,
    G                = 34,
    H                = 35,
    J                = 36,
    K                = 37,
    L                = 38,
    SEMICOLON        = 39,
    APOSTROPHE       = 40,
    GRAVE            = 41,
    LEFTSHIFT        = 42,
    BACKSLASH        = 43,
    Z                = 44,
    X                = 45,
    C                = 46,
    V                = 47,
    B                = 48,
    N                = 49,
    M                = 50,
    COMMA            = 51,
    DOT              = 52,
    SLASH            = 53,
    RIGHTSHIFT       = 54,
    KPASTERISK       = 55,
    LEFTALT          = 56,
    SPACE            = 57,
    CAPSLOCK         = 58,
    F1               = 59,
    F2               = 60,
    F3               = 61,
    F4               = 62,
    F5               = 63,
    F6               = 64,
    F7               = 65,
    F8               = 66,
    F9               = 67,
    F10              = 68,
    NUMLOCK          = 69,
    SCROLLLOCK       = 70,
    KP7              = 71,
    KP8              = 72,
    KP9              = 73,
    KPMINUS          = 74,
    KP4              = 75,
    KP5              = 76,
    KP6              = 77,
    KPPLUS           = 78,
    KP1              = 79,
    KP2              = 80,
    KP3              = 81,
    KP0              = 82,
    KPDOT            = 83,
    ZENKAKUHANKAKU   = 85,
    KEY_102ND        = 86,
    F11              = 87,
    F12              = 88,
    RO               = 89,
    KATAKANA         = 90,
    HIRAGANA         = 91,
    HENKAN           = 92,
    KATAKANAHIRAGANA = 93,
    MUHENKAN         = 94,
    KPJPCOMMA        = 95,
    KPENTER          = 96,
    RIGHTCTRL        = 97,
    KPSLASH          = 98,
    SYSRQ            = 99,
    RIGHTALT         = 100,
    LINEFEED         = 101,
    HOME             = 102,
    UP               = 103,
    PAGEUP           = 104,
    LEFT             = 105,
    RIGHT            = 106,
    END              = 107,
    DOWN             = 108,
    PAGEDOWN         = 109,
    INSERT           = 110,
    DELETE           = 111,
    MACRO            = 112,
    MUTE             = 113,
    VOLUMEDOWN       = 114,
    VOLUMEUP         = 115,
    POWER            = 116,
    KPEQUAL          = 117,
    KPPLUSMINUS      = 118,
    PAUSE            = 119,
    KPCOMMA          = 121,
    HANGEUL          = 122,
    HANGUEL          = 122,
    HANJA            = 123,
    YEN              = 124,
    LEFTMETA         = 125,
    RIGHTMETA        = 126,
    COMPOSE          = 127,
    STOP             = 128,
    AGAIN            = 129,
    PROPS            = 130,
    UNDO             = 131,
    FRONT            = 132,
    COPY             = 133,
    OPEN             = 134,
    PASTE            = 135,
    FIND             = 136,
    CUT              = 137,
    HELP             = 138,
    MENU             = 139,
    CALC             = 140,
    SETUP            = 141,
    SLEEP            = 142,
    WAKEUP           = 143,
    FILE             = 144,
    SENDFILE         = 145,
    DELETEFILE       = 146,
    XFER             = 147,
    PROG1            = 148,
    PROG2            = 149,
    WWW              = 150,
    MSDOS            = 151,
    COFFEE           = 152,
    DIRECTION        = 153,
    CYCLEWINDOWS     = 154,
    MAIL             = 155,
    BOOKMARKS        = 156,
    COMPUTER         = 157,
    BACK             = 158,
    FORWARD          = 159,
    CLOSECD          = 160,
    EJECTCD          = 161,
    EJECTCLOSECD     = 162,
    NEXTSONG         = 163,
    PLAYPAUSE        = 164,
    PREVIOUSSONG     = 165,
    STOPCD           = 166,
    RECORD           = 167,
    REWIND           = 168,
    PHONE            = 169,
    ISO              = 170,
    CONFIG           = 171,
    HOMEPAGE         = 172,
    REFRESH          = 173,
    EXIT             = 174,
    MOVE             = 175,
    EDIT             = 176,
    SCROLLUP         = 177,
    SCROLLDOWN       = 178,
    KPLEFTPAREN      = 179,
    KPRIGHTPAREN     = 180,
    NEW              = 181,
    REDO             = 182,
    F13              = 183,
    F14              = 184,
    F15              = 185,
    F16              = 186,
    F17              = 187,
    F18              = 188,
    F19              = 189,
    F20              = 190,
    F21              = 191,
    F22              = 192,
    F23              = 193,
    F24              = 194,
    PLAYCD           = 200,
    PAUSECD          = 201,
    PROG3            = 202,
    PROG4            = 203,
    SUSPEND          = 205,
    CLOSE            = 206,
    PLAY             = 207,
    FASTFORWARD      = 208,
    BASSBOOST        = 209,
    PRINT            = 210,
    HP               = 211,
    CAMERA           = 212,
    SOUND            = 213,
    QUESTION         = 214,
    EMAIL            = 215,
    CHAT             = 216,
    SEARCH           = 217,
    CONNECT          = 218,
    FINANCE          = 219,
    SPORT            = 220,
    SHOP             = 221,
    ALTERASE         = 222,
    CANCEL           = 223,
    BRIGHTNESSDOWN   = 224,
    BRIGHTNESSUP     = 225,
    MEDIA            = 226,
    SWITCHVIDEOMODE  = 227,
    KBDILLUMTOGGLE   = 228,
    KBDILLUMDOWN     = 229,
    KBDILLUMUP       = 230,
    SEND             = 231,
    REPLY            = 232,
    FORWARDMAIL      = 233,
    SAVE             = 234,
    DOCUMENTS        = 235,
    UNKNOWN          = 240,
    OK               = 0x160,
    SELECT           = 0x161,
    GOTO             = 0x162,
    CLEAR            = 0x163,
    POWER2           = 0x164,
    OPTION           = 0x165,
    INFO             = 0x166,
    TIME             = 0x167,
    VENDOR           = 0x168,
    ARCHIVE          = 0x169,
    PROGRAM          = 0x16a,
    CHANNEL          = 0x16b,
    FAVORITES        = 0x16c,
    EPG              = 0x16d,
    PVR              = 0x16e,
    MHP              = 0x16f,
    LANGUAGE         = 0x170,
    TITLE            = 0x171,
    SUBTITLE         = 0x172,
    ANGLE            = 0x173,
    ZOOM             = 0x174,
    MODE             = 0x175,
    KEYBOARD         = 0x176,
    SCREEN           = 0x177,
    PC               = 0x178,
    TV               = 0x179,
    TV2              = 0x17a,
    VCR              = 0x17b,
    VCR2             = 0x17c,
    SAT              = 0x17d,
    SAT2             = 0x17e,
    CD               = 0x17f,
    TAPE             = 0x180,
    RADIO            = 0x181,
    TUNER            = 0x182,
    PLAYER           = 0x183,
    TEXT             = 0x184,
    DVD              = 0x185,
    AUX              = 0x186,
    MP3              = 0x187,
    AUDIO            = 0x188,
    VIDEO            = 0x189,
    DIRECTORY        = 0x18a,
    LIST             = 0x18b,
    MEMO             = 0x18c,
    CALENDAR         = 0x18d,
    RED              = 0x18e,
    GREEN            = 0x18f,
    YELLOW           = 0x190,
    BLUE             = 0x191,
    CHANNELUP        = 0x192,
    CHANNELDOWN      = 0x193,
    FIRST            = 0x194,
    LAST             = 0x195,
    AB               = 0x196,
    NEXT             = 0x197,
    RESTART          = 0x198,
    SLOW             = 0x199,
    SHUFFLE          = 0x19a,
    BREAK            = 0x19b,
    PREVIOUS         = 0x19c,
    DIGITS           = 0x19d,
    TEEN             = 0x19e,
    TWEN             = 0x19f,
    DEL_EOL          = 0x1c0,
    DEL_EOS          = 0x1c1,
    INS_LINE         = 0x1c2,
    DEL_LINE         = 0x1c3,
    FN               = 0x1d0,
    FN_ESC           = 0x1d1,
    FN_F1            = 0x1d2,
    FN_F2            = 0x1d3,
    FN_F3            = 0x1d4,
    FN_F4            = 0x1d5,
    FN_F5            = 0x1d6,
    FN_F6            = 0x1d7,
    FN_F7            = 0x1d8,
    FN_F8            = 0x1d9,
    FN_F9            = 0x1da,
    FN_F10           = 0x1db,
    FN_F11           = 0x1dc,
    FN_F12           = 0x1dd,
    FN_1             = 0x1de,
    FN_2             = 0x1df,
    FN_D             = 0x1e0,
    FN_E             = 0x1e1,
    FN_F             = 0x1e2,
    FN_S             = 0x1e3,
    FN_B             = 0x1e4,
    MAX              = 0x1ff,
  };

  Rel = {
    X      = 0x00,
    Y      = 0x01,
    Z      = 0x02,
    RX     = 0x03,
    RY     = 0x04,
    RZ     = 0x05,
    HWHEEL = 0x06,
    DIAL   = 0x07,
    WHEEL  = 0x08,
    MISC   = 0x09,
    MAX    = 0x0f,
  };

  Abs = {
    X          = 0x00,
    Y          = 0x01,
    Z          = 0x02,
    RX         = 0x03,
    RY         = 0x04,
    RZ         = 0x05,
    THROTTLE   = 0x06,
    RUDDER     = 0x07,
    WHEEL      = 0x08,
    GAS        = 0x09,
    BRAKE      = 0x0a,
    HAT0X      = 0x10,
    HAT0Y      = 0x11,
    HAT1X      = 0x12,
    HAT1Y      = 0x13,
    HAT2X      = 0x14,
    HAT2Y      = 0x15,
    HAT3X      = 0x16,
    HAT3Y      = 0x17,
    PRESSURE   = 0x18,
    DISTANCE   = 0x19,
    TILT_X     = 0x1a,
    TILT_Y     = 0x1b,
    TOOL_WIDTH = 0x1c,
    VOLUME     = 0x20,
    MISC       = 0x28,
    MT_SLOT        = 0x2f,
    MT_TOUCH_MAJOR = 0x30,
    MT_TOUCH_MINOR = 0x31,
    MT_WIDTH_MAJOR = 0x32,
    MT_WIDTH_MINOR = 0x33,
    MT_ORIENTATION = 0x34,
    MT_POSITION_X  = 0x35,
    MT_POSITION_Y  = 0x36,
    MT_TOOL_TYPE   = 0x37,
    MT_BLOB_ID     = 0x38,
    MT_TRACKING_ID = 0x39,
    MT_PRESSURE    = 0x3a,
    MT_DISTANCE    = 0x3b,
    MAX        = 0x3f,
  };

  Btn = {
    MISC           = 0x100,
    BTN_0          = 0x100,
    BTN_1          = 0x101,
    BTN_2          = 0x102,
    BTN_3          = 0x103,
    BTN_4          = 0x104,
    BTN_5          = 0x105,
    BTN_6          = 0x106,
    BTN_7          = 0x107,
    BTN_8          = 0x108,
    BTN_9          = 0x109,
    MOUSE          = 0x110,
    LEFT           = 0x110,
    RIGHT          = 0x111,
    MIDDLE         = 0x112,
    SIDE           = 0x113,
    EXTRA          = 0x114,
    FORWARD        = 0x115,
    BACK           = 0x116,
    TASK           = 0x117,
    JOYSTICK       = 0x120,
    TRIGGER        = 0x120,
    THUMB          = 0x121,
    THUMB2         = 0x122,
    TOP            = 0x123,
    TOP2           = 0x124,
    PINKIE         = 0x125,
    BASE           = 0x126,
    BASE2          = 0x127,
    BASE3          = 0x128,
    BASE4          = 0x129,
    BASE5          = 0x12a,
    BASE6          = 0x12b,
    DEAD           = 0x12f,
    GAMEPAD        = 0x130,
    A              = 0x130,
    B              = 0x131,
    C              = 0x132,
    X              = 0x133,
    Y              = 0x134,
    Z              = 0x135,
    TL             = 0x136,
    TR             = 0x137,
    TL2            = 0x138,
    TR2            = 0x139,
    SELECT         = 0x13a,
    START          = 0x13b,
    MODE           = 0x13c,
    THUMBL         = 0x13d,
    THUMBR         = 0x13e,
    DIGI           = 0x140,
    TOOL_PEN       = 0x140,
    TOOL_RUBBER    = 0x141,
    TOOL_BRUSH     = 0x142,
    TOOL_PENCIL    = 0x143,
    TOOL_AIRBRUSH  = 0x144,
    TOOL_FINGER    = 0x145,
    TOOL_MOUSE     = 0x146,
    TOOL_LENS      = 0x147,
    TOUCH          = 0x14a,
    STYLUS         = 0x14b,
    STYLUS2        = 0x14c,
    TOOL_DOUBLETAP = 0x14d,
    TOOL_TRIPLETAP = 0x14e,
    WHEEL          = 0x150,
    GEAR_DOWN      = 0x150,
    GEAR_UP        = 0x151,
  };
};

-- stores all kown input devices
streams = {};
sources = {};
Input_device = {
  Devs = {};
  quirks = {};
  Bus = {
    [0x01] = "pci",
    [0x02] = "isapnp",
    [0x03] = "usb",
    [0x04] = "hil",
    [0x05] = "bluetooth",
    [0x06] = "virtual",

    [0x10] = "isa",
    [0x11] = "i8042",
    [0x12] = "xtkbd",
    [0x13] = "rs232",
    [0x14] = "gameport",
    [0x15] = "parport",
    [0x16] = "amiga",
    [0x17] = "adb",
    [0x18] = "i2c",
    [0x19] = "host",
    [0x1A] = "gsc",
    [0x1B] = "atari",

    PCI		= 0x01,
    ISAPNP	= 0x02,
    USB		= 0x03,
    HIL		= 0x04,
    BLUETOOTH	= 0x05,
    VIRTUAL	= 0x06,

    ISA		= 0x10,
    I8042	= 0x11,
    XTKBD	= 0x12,
    RS232	= 0x13,
    GAMEPORT	= 0x14,
    PARPORT	= 0x15,
    AMIGA	= 0x16,
    ADB		= 0x17,
    I2C		= 0x18,
    HOST	= 0x19,
    GSC		= 0x1A,
    ATARI	= 0x1B,
  };
};

function Input_device:drop_event(e)
  return true;
end

function Input_device:get_stream_info()
  return self.source:get_stream_info(self.stream);
end

function Input_device:get_abs_info(...)
  return self.source:get_abs_info(self.stream, ...);
end


function Input_device:init()
  self.current_report = Hid_report(self.global_id, 0x10, 0x29, 0x8, 0x10, 0x10 );
end

function Input_device:process_event(ev)
  local r = self.current_report;
  local t = ev[1];
  local c = ev[2];
  if t == 3 then
    if c < 0x2f then
      r:set(3, c, ev[3]);
    else
      r:mt_set(c, ev[3]);
    end
  elseif t == 1 then
    r:add_key(c, ev[3]);
  elseif t > 1 and t <=5 then
    r:set(t, c, ev[3]);
  elseif t == 0 and c == 2 then -- mt sync
    r:submit_mt();
  elseif t == 0 and c == 0 then -- full sync
    r:sync(ev[4]);
    self:process(r);
    r:clear();
  elseif t == 0 and c == 0x80 then -- l4re syn cfg
    r:clear();
    for s in Mag.core_api:sessions() do
      s:put_event(self.global_id, t, c, ev[3], ev[4]);
    end
  end
end

-- ----------------------------------------------------------------------
--  Generic pointer device handling.
--
--  Handles absolute and relative motion events and the buttons
--  corresponding to a pointer device, such as a mouse.
-- ----------------------------------------------------------------------
Input_device.Devs.pointer = {};

Input_device.Devs.pointer.drag_focus    = View_proxy(user_state);

function Input_device.new_virt_ctrl(_axes, _core)
  local ctrl = { axes = _axes, core = _core };
  for _, a in pairs(_axes) do
    a.ctrl = ctrl;
  end
  return ctrl;
end

local screen_width, screen_height = user_state:screen_size()

Input_device.core_ctrl = Input_device.new_virt_ctrl(
  { x = { idx = Event.Abs.X, min = 0, max = screen_width,
          fuzz = 0, flat = 0, mode = 1, v = 0 },
    y = { idx = Event.Abs.Y, min = 0, max = screen_height,
          fuzz = 0, flat = 0, mode = 2, v = 0 } }, true);

function Input_device.Devs.pointer:init()
  Input_device.init(self);
  self.pointed_view  = View_proxy(user_state);

  -- get core pointer control descriptions for x and y axis
  local core_ctrl = Input_device.core_ctrl;

  self.xfrm_abs_axes = {};
  
  local d_info = self.info;

  if d_info:get_absbit(0) and d_info:get_absbit(1) then
    -- we transfrom the absolute x and y axes of this device to core control
    -- coordinates
    self.xfrm_abs_axes[#self.xfrm_abs_axes] =
      { x = Event.Abs.X, y = Event.Abs.Y,
        -- we have our abs x and abs y axis controling our core pointer
        core_x = core_ctrl.axes.x, core_y = core_ctrl.axes.y,
        xfrm = self:get_pointer_transform_func(Event.Abs.X, Event.Abs.Y) };
  end

  self.virt_ctrls = { core_ctrl };

  self.vaxes = { [Event.Abs.X] = core_ctrl.axes.x,
                 [Event.Abs.Y] = core_ctrl.axes.y };

  self.rel_to_abs_axes = { [Event.Rel.X] = { va = core_ctrl.axes.x, ctrl = core_ctrl },
                           [Event.Rel.Y] = { va = core_ctrl.axes.y, ctrl = core_ctrl } };

  local abs_info = Axis_info_vector(0x40);
  self.current_report:set_abs_info(abs_info);
  self.abs_info = abs_info;
  for a = 0, 0x3f do
    if d_info:get_absbit(a) then
      local r, i = self:get_abs_info(a);
      if r == 0 then
	abs_info:set(a, i);
      end
    end
  end

  if abs_info[0] then
    abs_info[0].mode = 1;
  end
  if abs_info[1] then
    abs_info[1].mode = 2;
  end

  if d_info:get_absbit(0x35) and d_info:get_absbit(0x36) then
    local mt_ctrl = Input_device.new_virt_ctrl(
      { x = { idx = 0x35, min = 0, max = screen_width,
              fuzz = 0, flat = 0, mode = 1, v = 0 },
        y = { idx = 0x36, min = 0, max = screen_height,
              fuzz = 0, flat = 0, mode = 2, v = 0 } })
    self.xfrm_abs_axes[#self.xfrm_abs_axes + 1] =
      { x = 0x35, y = 0x36, xfrm = self:get_pointer_transform_func(0x35, 0x36) };
    self.vaxes[0x35] = mt_ctrl.axes.x;
    self.vaxes[0x36] = mt_ctrl.axes.y;
    abs_info[0x35].mode = 1;
    abs_info[0x36].mode = 2;
  end

  for a, i in pairs(self.vaxes) do
    if (not abs_info[a]) then
      local ai = abs_info:create(a);
      ai.value = i.value or 0;
      ai.min   = i.min or 0;
      ai.max   = i.max or 0;
      ai.fuzz  = i.fuzz or 0;
      ai.resolution = i.resolution or 0;
      ai.delta = i.delta or 0;
      ai.mode  = i.mode or 0;
    end
  end
end

function Input_device.Devs.pointer:get_stream_info()
  local r, info = self:get_stream_info();

  if r < 0 then
    return r;
  end

  -- This device is translated to a core pointer
  -- this means it sends always absolute X,Y axis pairs
  if self.vaxes then
    info:set_evbit(Event.ABS, true);
    for a, _ in pairs(self.vaxes) do
      info:set_absbit(a, true);
    end
  end

  if self.rel_to_abs_axes then
    for a, _ in pairs(self.rel_to_abs_axes) do
      info:set_relbit(a, false);
    end
  end

  return r, info;
end

function Input_device.Devs.pointer:get_abs_info(...)
  local axes = {...};
  local pass = true;
  local vaxes = self.vaxes;
  local r = {};

  for _, s in ipairs(axes) do
    if vaxes[s] then
      r[_] = vaxes[s];
    else
      r[_] = self.abs_info[s];
      if not r[_] then
	return -22;
      end
    end
  end
  return (vaxes ~= nil), table.unpack(r);
end

-- generic transformationof absolute pointer events to screen coordinates
function Input_device:get_pointer_transform_func(ax, ay)
  -- transformation of raw coordinates to screen coordinates
  local w, h = user_state:screen_size();
  return function(self, p)
    if self.abs_info then
      return (p[1] * w) // self.abs_info[ax].delta,
             (p[2] * h) // self.abs_info[ay].delta;
    else
      return p[1], p[2];
    end
  end;
end

-- ----------------------------------------------------------------
-- sync a set of events from a pointer device
--
-- - converts motion events to the core mouse pointer of mag
-- - manages dragging and keyboard focus


Input_device.Default = {};

function Input_device.Default:axis_xfrm(report)
  local function do_xfrm_axes(xfrm_axes, axes)
    if not axes then return end
    for i, cax in pairs(xfrm_axes) do
      local x = axes[cax.x];
      local y = axes[cax.y];
      if x and y then
        x, y = cax.xfrm(self, {x, y});
        axes[cax.x] = x;
        axes[cax.y] = y;
        local ca = cax.core_x;
        if ca then
          ca.v = x;
          ca.ctrl.update = true;
        end
        local ca = cax.core_y;
        if ca then
          ca.v = y;
          ca.ctrl.update = true;
        end
      end
    end
  end

  -- scale and rotate absolute axes
  local abs = report[3];
  do_xfrm_axes(self.xfrm_abs_axes, abs);
  for id = 0, 10 do
    do_xfrm_axes(self.xfrm_abs_axes, report:get_mt_vals(id));
  end

  -- transform relative to absolute axes if needed
  local rel = report[2];
  if rel then
    for a, inf in pairs(self.rel_to_abs_axes) do
      local v = rel[a];
      if v then
	rel:inv(a);
	local vctrl = inf.ctrl;
	local va = inf.va;
	v = va.v + v;
	if v < va.min then
	  v = va.min;
	end
	if v > va.max then
	  v = va.max;
	end
	va.v = v;
	vctrl.dirty = true;
      end
    end
  end

  -- write back virtual controls if they are dirty
  if abs then
    for c, vc in pairs(self.virt_ctrls) do
	for i, va in pairs(vc.axes) do
	  abs:set(va.idx, va.v);
	end
      if vc.dirty then
	vc.update = true; -- if its a core control we need an update
	vc.dirty = false;
      end
    end
  end
end

function Input_device.Default:foreach_key(report, func)
  local i = 0;
  while true do
    local c, v = report:get_key_event(i);
    if not c then
      break;
    end
    if func(self, c, v) then
      return true;
    end
    i = i + 1;
  end
end

function Input_device.Default:do_core_ctrl()
  local cctrl = Input_device.core_ctrl;
  if cctrl.update then
    cctrl.update = false;
    local x, y = cctrl.axes.x.v, cctrl.axes.y.v;
    user_state:set_pointer(x, y, self.hide_cursor or false);
    user_state:find_pointed_view(self.pointed_view); -- should need x and y here
  end
end

function Input_device.Default:core_keys(report)
  local toggle_mode = 0;
  local function xray_key(s, c, v)
    if v ~= 1 then
      return false;
    end
    return (c == 70) or (c == Event.Key.NEXTSONG);
  end

  local function kill_key(s, c, v)
    if v ~= 1 then
      return false;
    end
    return (c == 210) or (c == 99);
  end

  local fek = Input_device.Default.foreach_key;

  if fek(self, report, xray_key) then
    toggle_mode = 1;
  elseif fek(self, report, kill_key) then
    toggle_mode = 2;
  end
  if toggle_mode ~= 0 then
    user_state:toggle_mode(toggle_mode);
    return true;
  end
  return false;
end

function Input_device.Default:dnd_handling()
  local ptr = Input_device.Devs.pointer;
  if ptr.dragging then
    if ptr.dragging == self and self.stop_drag then
      ptr.dragging = false;
    end
    self.stop_drag = false;
    return ptr.drag_focus;
  else
    return self.pointed_view;
  end
end

function Input_device.Devs.pointer:hook(report)
  local n_press = 0;
  local n_release = 0;
  local function k(s, c, v)
    if v == 1 then
      n_press = n_press + 1;
    elseif v == 0 then
      n_release = n_release + 1;
    end
  end
  Input_device.Default.foreach_key(self, report, k);

  local n = self.pressed_keys + n_press - n_release;
  if n < 0 then
    n = 0;
  end

  self.pressed_keys = n;

  local ptr = Input_device.Devs.pointer;

  if (not ptr.dragging) and n_press > 0 and n == n_press then
    ptr.dragging = self;
    ptr.drag_focus:set(self.pointed_view);
    self.set_focus = true;
  elseif n_release > 0 and n == 0 then
    self.stop_drag = true;
  end
end

function Input_device.Devs.pointer:process(report)
  local update = false;
  local sink;
  local dfl = Input_device.Default;

  dfl.axis_xfrm(self, report);
  dfl.do_core_ctrl(self);

  if self.handler.hook then
    self.handler.hook(self, report);
  end

  sink = dfl.dnd_handling(self);
  -- for keyboards: update = dfl.core_keys(self, report);
  if self.set_focus then
    update = user_state:set_kbd_focus(self.pointed_view) or update;
    self.set_focus = false;
  end
  user_state:post_event(sink, report, update, true);
end


Input_device.Devs.touchscreen = {};
setmetatable(Input_device.Devs.touchscreen, { __index = Input_device.Devs.pointer});

function Input_device.Devs.touchscreen:init()
  Input_device.Devs.pointer.init(self);
  self.hide_cursor = true;
end

function Input_device.Devs.touchscreen:get_stream_info()
  local r, info = Input_device.Devs.pointer.get_stream_info(self);

  if r < 0 then
    return r;
  end

  -- additionally we have a left button, but no longer the touch button
  --info:set_keybit(Event.Btn.TOUCH, false);
  --info:set_keybit(Event.Btn.LEFT, true);
  return r, info;
end

function Input_device.Devs.touchscreen:button(e)
  local c = e[2];
  local v = e[3];
  if c == Event.Btn.TOUCH then
    e[2] = Event.Btn.LEFT;
  end
  Input_device.Devs.pointer.button(self, e);
end

-- ----------------------------------------------------------------------
--  Touchpad pointing device handling
--  Inherits from pointer device and adds functionality to:
--  - transform absolute finger positions to relative movements
--  - tapping button emulation (missing)
--  - scroll wheel emulation etc (missing)
-- ----------------------------------------------------------------------
Input_device.Devs.touchpad = {};
setmetatable(Input_device.Devs.touchpad, { __index = Input_device.Devs.pointer});

function Input_device.Devs.touchpad:init()
  Input_device.Devs.pointer.init(self);
  self.touchdev = {
    touch      = false;
    release    = false;
    touch_drag = false;
    tap_time   = 150 * 1000; -- 150 ms
    btn_report = Hid_report(self.global_id,0,0,0,0,0);
  };
  local tdev = self.touchdev;
  tdev.p    = { Axis_buf(4), Axis_buf(4) };
  tdev.pkts = 0;
end

function Input_device.Devs.touchpad:get_stream_info()
  local r, info = Input_device.Devs.pointer.get_stream_info(self);

  if r < 0 then
    return r;
  end

  -- additionally we have a left button, but no longer the touch button
  info:set_keybit(Event.Btn.LEFT, true);
  info:set_keybit(Event.Btn.TOOL_FINGER, false);
  info:set_keybit(Event.Btn.TOOL_PEN, true);

  -- hm, X does not like to have MT values if we do not send them
  info:set_absbit(0x2f, false);
  info:set_absbit(0x30, false);
  info:set_absbit(0x31, false);
  info:set_absbit(0x32, false);
  info:set_absbit(0x33, false);
  info:set_absbit(0x34, false);
  info:set_absbit(0x35, false);
  info:set_absbit(0x36, false);
  info:set_absbit(0x37, false);
  info:set_absbit(0x38, false);
  info:set_absbit(0x39, false);
  info:set_absbit(0x3a, false);
  info:set_absbit(0x3b, false);

  return r, info;
end

function Input_device.Devs.touchpad:button(r)
  local c, v = r:find_key_event(Event.Btn.TOUCH);
  if not v or v == 2 then
    return
  end

  local time = r:time();
  local p = self.touchdev;

  -- drop the touch event, we convert it to left mouse button
  --r[1][Event.Btn.TOUCH] = nil;
  r:remove_key_event(Event.Btn.TOOL_FINGER);

  if v == 1 then
    if p.release and p.release + p.tap_time > time then
      p.touch_drag = true;
      r:add_key(Event.Btn.LEFT, 1);
    end
    p.touch = time;
    p.release = false;
  else
    if p.touch_drag then
      p.touch_drag = false;
      r:add_key(Event.Btn.LEFT, 0);
    elseif p.touch and p.touch + p.tap_time > time then
      p.release = time;

      local dr = p.btn_report;

      dr:clear();
      dr:sync(time);
      dr:add_key(Event.Btn.LEFT, 1);
      self:process(dr);

      r:add_key(Event.Btn.LEFT, 0);
    end
    p.pkts = 0;
    p.touch = false;
  end
end

function Input_device.Devs.touchpad:motion(r)
  local p = self.touchdev;
  if not p.touch then
    -- drop event, if not touched
    r[2]:clear();
    r[3]:clear();
    return
  end

  local abs = r[3];
  for c = 1, 2 do
    local v = abs[c-1];
    if v then
      local cnt = p.pkts;
      p.p[c][cnt] = v;
      if cnt >= 2 then
        local size = self.abs_info[c-1].delta;
        r[2]:set(c-1, (v - p.p[c]:get(cnt - 2)) * 256 // size);
      end
      abs:inv(c-1);
    end
  end

  local nc = p.pkts + 1;
  p.pkts = nc;
  p.p[1]:copy(nc, nc - 1);
  p.p[2]:copy(nc, nc - 1);
end

function Input_device.Devs.touchpad:process(r)
  Input_device.Devs.touchpad.button(self, r);
  Input_device.Devs.touchpad.motion(self, r);
  Input_device.Devs.pointer.process(self, r)
end

-- ----------------------------------------------------------------------
--  Keyboard device handling
-- ----------------------------------------------------------------------
Input_device.Devs.keyboard = {};
setmetatable(Input_device.Devs.keyboard, { __index = Input_device });

function Input_device.Devs.keyboard:get_stream_info()
  return self:get_stream_info();
end

function Input_device.Devs.keyboard:get_abs_info(...)
  return self:get_abs_info(...);
end

function Input_device.Devs.keyboard:process(report)
  local dfl = Input_device.Default;

  if self.handler.hook then
    self.handler.hook(self, report);
  end
  local update = dfl.core_keys(self, report);
  user_state:post_event(nil, report, update, true);
end

Input_device.Devs.unknown = Input_device;

function Input_device:find_device_type()
  local bus, vend, prod, ver = self.info:get_device_id();
  self.dev_id = { bus = bus, vendor = vend, product = prod, version = ver };
  local id = bus .. "_" .. vend .. "_" .. prod;

  self.dev_type = "unknown";

  if self.quirks[id] and self.quirks[id](self, id) then
    return;
  end

  id = id .. "_" .. ver;

  if self.quirks[id] and self.quirks[id](self, id) then
    return;
  end

  local have_keys = self.info:get_evbit(1);
  if (have_keys and self.info:get_evbit(2)) then
    -- we have relative axes, can be a mouse
    if (self.info:get_relbit(0) and self.info:get_relbit(1)
        and self.info:get_keybit(0x110)) then
      -- x and y axis and at least the left mouse button found
      self.dev_type = "pointer";
    end
  end
  if (have_keys and self.info:get_evbit(3)) then
    -- we have absolute axes, can be a mouse
    if self.info:get_absbit(0) and self.info:get_absbit(1) then
      if self.info:get_keybit(0x110) then
        -- x and y axis and at least the left mouse button found
        self.dev_type = "pointer";
      end
      if self.info:get_keybit(Event.Btn.TOUCH)
         and not self.info:get_keybit(0x110) then
        -- x and y axis and at least touch button found
        self.dev_type = "touchscreen";
      end
    end
  end

  if self.dev_type == "pointer" then
    if self.info:get_keybit(Event.Btn.TOOL_FINGER) then
      self.dev_type = "touchpad";
    end
  end

  if (have_keys) then
    local k;
    for k = 0, 199 do
      if (self.info:get_keybit(k)) then
        self.dev_type = "keyboard";
        return;
      end
    end
  end
end

function Input_device:delete()
  streams[self.global_id] = nil;
  sources[self.source][self.stream] = nil;
  print (string.format([==[Input: remove device (src='%s' stream='%s')]==],
                       tostring(self.source), tostring(self.stream)));
  self = nil;
  collectgarbage();
end


-- -----------------------------------------------------------------------
--  Create a input device (stream) object.
--  A single input stream is managed by a device specific set of
--  methods and data structures.
-- -----------------------------------------------------------------------
local input_device_mt = { __index = Input_device };

function create_input_device(source, stream)
  local dh = {
    -- the device handle itself
    source        = source,
    stream        = stream,
    global_id     = #streams + 1;

    -- number of keys currently pressed on this device
    pressed_keys  = 0,

    -- data according to the current bundle of events (one comound event)
    -- need to set the focus to the pointed view
    set_focus     = false,
    -- the current events stop dragging mode
    stop_drag     = false,
    -- the events to pass to the applications
    events        = {},
    -- event handler for the device
    process       = Input_device.drop_event
  };

  setmetatable(dh, input_device_mt);

  local r;
  r, dh.info = dh:get_stream_info();

  sources[source][stream] = dh;
  streams[dh.global_id] = dh;

  dh:find_device_type();
  dh.handler = Input_device.Devs[dh.dev_type] or {};
  dh.process = dh.handler.process or Input_device.drop_event;
  if dh.handler.init then dh.handler.init(dh); end

  local pdev;
  function pdev(dh)
    local a, b, c, d = dh.info:get_device_id();
    if Input_device.Bus[a] then
      return Input_device.Bus[a], b, c, d;
    end
    return "bus " .. a, b, c, d;
  end

  print (string.format([==[Input: new %s device (src='%s' stream='%s')
                           bus='%s' vendor=0x%x product=0x%x version=%d]==],
                       dh.dev_type, tostring(dh.source), tostring(dh.stream),
                       pdev(dh)));
  return dh;
end
