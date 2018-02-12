local table = require "table"
local string = require "string"
local debug = require "debug"

local function check_device(dev, depth, err)
  if not Io.swig_instance_of(dev, "Generic_device *") then
    local e = err or "expected device, got: " .. tostring(dev);
    error(e, depth + 1);
  end
  return dev
end

Io.Res = {}

function Io.Res.io(start, _end, flags)
  local f = flags or 0
  return Io.Resource(Io.Resource_Io_res, f, start, _end or start)
end

function Io.Res.mmio(start, _end, flags)
  if start > _end then
    error("Mmio region start > end. This is impossible.", 2);
  end
  local f = flags or 0
  return Io.Resource(Io.Resource_Mmio_res, f, start, _end or start)
end

function Io.Res.irq(start, ...)
  local f = 0
  local xargs = select("#", ...)
  if xargs == 1 then
    f = ...
    if type(f) ~= 'number' then
      error("second, argument of Io.Res.irq() must be a `flags` value", 2)
    end
  end

  local s, e
  if type(start) == "table" then
    s = start[1]
    e = start[2]
  else
    s = start
    e = start
  end
  return Io.Resource(Io.Resource_Irq_res, f, s, e)
end

Io.Dt = {}

function Io.Dt.add_child(parent, name, dev, idx)
  parent:add_child(dev)
  if dev.plugin and (parent:parent() or swig_equals(parent, Io.hw_bus)) then
    dev:plugin()
  end
  if type(name) == "string" then
    if idx ~= nil then
      name = name .. '[' .. idx ..']'
    end
    dev:set_name(name)
  end
end

function Io.Dt.add_children(parent, bus_func)
  check_device(parent, 2);
  if type(bus_func) == "function" then
    local d = {
      Property = {},
      Resource = {},
      Child = {}
    }
    local env_upval = 0
    local name
    local my_env
    repeat
      env_upval = env_upval + 1
      name, my_env = debug.getupvalue(bus_func, env_upval)
    until name == '_ENV' or name == nil
    if name == nil then
      print("Warning: The bus function used for add_children does not have an _ENV");
      print("         Empty vbus? Use 'bus = Io.Vi.System_bus()' instead.")
      return parent;
    end

    setmetatable(d, { __index = my_env })
    debug.upvaluejoin(bus_func, env_upval, function() return d end, 1)
    bus_func();
    Io.Dt.add_device_data(parent, d)
  end
  return parent;
end

function Io.Dt.iterator(dev, max_depth)
  local max_d = (max_depth or 0) + dev:depth()
  local current = dev
  local start = dev
  return function ()
    local c = current
    if c == nil then
      return nil
    end

    local cd = c:depth()
    local cc = c:children()
    if max_d > cd and cc then
      current = cc
      return c
    elseif swig_equals(c, start) then
      current = nil
      return c
    elseif c:next() then
      current = c:next()
      return c
    else
      local p = c
      while (true) do
        p = p:parent()
        if (not p) or swig_equals(p, start) then
          current = nil
          return c
        elseif p:next() then
          current = p:next()
          return c
        end
      end
    end
  end
end

function Io.Dt.match_cids(self, ...)
  local r = {}
  for _, v in ipairs{...} do
    if self:match_cid(v) then
      return true
    end
  end
  return false
end

Io.Dt.PCI_cc =
{
  storage              = "CC_01",
  scsi                 = "CC_0100",
  ide                  = "CC_0101",
  floppy               = "CC_0102",
  raid                 = "CC_0104",
  ata                  = "CC_0105",
  sata                 = "CC_0106",
  sas                  = "CC_0107",
  nvm                  = "CC_0108",

  network              = "CC_02",
  ethernet             = "CC_0200",
  token_ring           = "CC_0201",
  fddi                 = "CC_0202",
  atm                  = "CC_0203",
  isdn                 = "CC_0204",
  picmg                = "CC_0206",
  net_infiniband       = "CC_0207",
  fabric               = "CC_0208",
  network_nw           = "CC_0280", -- often used for wifi

  display              = "CC_03",
  vga                  = "CC_0300",
  xga                  = "CC_0301",

  media                = "CC_04",
  mm_video             = "CC_0400",
  mm_audio             = "CC_0401",
  tekephony            = "CC_0402",
  audio                = "CC_0403",

  bridge               = "CC_06",
  br_host              = "CC_0600",
  br_isa               = "CC_0601",
  br_eisa              = "CC_0602",
  br_microchannel      = "CC_0603",
  br_pci               = "CC_0604",
  br_pcmcia            = "CC_0605",
  br_nubus             = "CC_0606",
  br_cardbus           = "CC_0607",
  br_raceway           = "CC_0608",
  br_semi_pci          = "CC_0609",
  br_infiniband_to_pci = "CC_060a",

  com                  = "CC_07",
  com_serial           = "CC_0700",
  com_parallel         = "CC_0701",
  com_multiport_ser    = "CC_0702",
  com_modem            = "CC_0703",
  com_gpib             = "CC_0704",
  com_smart_card       = "CC_0705",


  serial_bus           = "CC_0c",
  firewire             = "CC_0c00",
  access_bus           = "CC_0c01",
  ssa                  = "CC_0c02",
  usb                  = "CC_0c03",
  fibre_channel        = "CC_0c04",
  smbus                = "CC_0c05",
  bus_infiniband       = "CC_0c06",
  ipmi_smic            = "CC_0c07",
  sercos               = "CC_0c08",
  canbus               = "CC_0c09",

  wireless             = "CC_0d",
  bluetooth            = "CC_0d11",
  w_8021a              = "CC_0d20",
  w_8021b              = "CC_0d21",
}

Io.Dt.MAX_DEPTH = 1000

function Io.Dt.Range(start, stop)
  return { range = true, start, stop }
end

function Io.Dt.match(self, ...)
  local cids = {...}
  for t,v in pairs(Io.Dt.PCI_cc) do
    for i, cid in ipairs(cids) do
      cids[i] = cid:gsub("(PCI/"..t..")", "PCI/" .. v)
    end
  end

  local devs = {}
  for d in self:devices(Io.Dt.MAX_DEPTH) do
    if d:match_cids(table.unpack(cids)) then
      devs[#devs+1] = d
    end
  end
  return devs
end

function Io.Dt.device(self, path)
  for i in string.gmatch(path, "([^%./]+)%.*") do
    self = self:find_by_name(i)
    if self == nil then
      return nil
    end
  end
  return self
end

function Io.Dt.resources(self)
  local n = self:nresources()
  local c = 0
  return function ()
    if c >= n then return nil end
    c = c + 1
    return self:resource(c - 1)
  end
end

local hwfn = Io.swig_class("Hw_device")[".fn"]
local vifn = Io.swig_class("Vi_device")[".fn"]

local dev_fns =
{
  resources = Io.Dt.resources,
  devices = Io.Dt.iterator,
  match_cids = Io.Dt.match_cids,
  match = Io.Dt.match,
  device = Io.Dt.device
}

for name, func in pairs(dev_fns) do
  hwfn[name] = func
  vifn[name] = func
end

function vifn:add_filter_val(tag, value)
  if type(value) == "table" and value.range then
    return self:add_filter(tag, value[1], value[2])
  elseif type(value) == "table" then
    for _, v in ipairs(value) do
      local res = self:add_filter_val(tag, v)
      if res < 0 then
        return res
      end
    end
    return 0
  else
    return self:add_filter(tag, value)
  end
end

local add_child = Io.Dt.add_child
local function set_device_property(dev, name, idx, val)
  -- test whether a property is registered
  local p = dev:property(name);
  if p == nil then
    print("error: no property '" .. name .. "' registered");
    return
  end
  local r = p:set(idx, val)
  if r ~= 0 then
    print("error: setting property " .. tostring(name) .. " failed")
  end
end

local function handle_device_member(dev, val, name)
  local vtype = type(val)
  if name == "compatible" then
    if vtype == "table" then
      for k, v in ipairs(val) do
        dev:add_cid(v)
      end
    elseif vtype == "string" then
      dev:add_cid(val)
    else
      error("'compatible' must be a string or table of strings (probably you assigned something else)", 2)
    end
    return
  elseif name == "Property" then
    if vtype ~= "table" then
      error("'Property' must be a table (probably you assigned something else)", 2)
    end
    for k,v in pairs(val) do
      local r
      if type(v) ~= "table" then
        set_device_property(dev, k, -1, v)
      else
        for i,j in ipairs(v) do
          set_device_property(dev, k, i, j)
        end
      end
    end
    return
  elseif name == "Resource" then
    if vtype ~= "table" then
      error("'Resource' must be a table (probably you assigned something else)", 2)
    end
    for k,v in pairs(val) do
      if Io.swig_instance_of(v, "Resource *") then
        v:set_id(k)
        dev:add_resource(v)
      else
        print("WARNING: ".. tostring(k) .. ") in 'Resource' table is not a resource.")
        print("         Ignoring.")
      end
    end
    return
  elseif name == "Child" then
    if vtype ~= "table" then
      error("'Child' must be a table (probably you assigned something else)", 2)
    end
    for k,v in pairs(val) do
      if Io.swig_instance_of(v, "Generic_device *") then
        add_child(dev, k, v)
      else
        print("WARNING: ".. tostring(k) .. ") in 'Child' table is not a device.")
        print("         Ignoring.")
      end
    end
    return
  elseif Io.swig_instance_of(val, "Resource *") then
    val:set_id(name)
    dev:add_resource(val)
    return
  elseif Io.swig_instance_of(val, "Generic_device *") then
    add_child(dev, name, val)
    return
  elseif vtype == "table" then
    for i, v in pairs(val) do
      handle_device_member(dev, v, name .. '[' .. i .. ']')
    end
    return
  end
  error("cannot handle device member: " .. tostring(name) .. ": " .. tostring(val), 2)
end

function Io.Dt.add_resource(dev, res)
  if not Io.swig_instance_of(dev, "Generic_device *") then
    error("expected a device got: " .. tostring(dev), 2)
  end
  if not Io.swig_instance_of(res, "Resource *") then
    error("expected a resource got: " .. tostring(res), 2)
  end
  dev:add_resource(res)
end

function Io.Dt.add_device_data(dev, data)
  local maxi = 0
  for i, v in ipairs(data) do
    handle_device_member(dev, v, i)
    maxi = i
  end
  for k, v in pairs(data) do
    if (type(k) ~= "number") or (k > maxi) then
      handle_device_member(dev, v, k)
    end
  end
end

local set_dev_data = Io.Dt.add_device_data
local add_children = Io.Dt.add_children

Io.hw_bus = Io.system_bus()
Io.Hw = {}

setmetatable(Io.Hw, { __index = function (self, t)
  return function (data)
    local b = check_device(Io.Hw_dev_factory_create(t), 3, "could not create device: " .. t)
    if type(data) == "function" then
      add_children(b, data)
    end
    return b
  end
end})

function Io.hw_add_devices(data)
  local sb = Io.hw_bus
  local dtype = type(data)
  if dtype == 'function' then
    Io.Dt.add_children(sb, data)
  end
  return data
end

Io.Vi = {}

setmetatable(Io.Vi, { __index = function (self, t)
  return function (data)
    local b = Io.Vi_dev_factory_create(t)
    if type(data) == "function" then
      add_children(b, data)
    elseif type(data) == "table" then
      set_dev_data(b, data)
    end
    return b
  end
end})

function wrap(devs_, filter)
  local devs = devs_
  if type(devs_) ~= "table" then
    devs = { devs_ }
  end
  local v = {}
  for _, d in ipairs(devs) do
    local vd = Io.Vi_dev_factory_create(d)
    if vd then
      if type(filter) == "table" then
        for tag, val in pairs(filter) do
          local res = vd:add_filter_val(tag, val)
          if res < 0 then
            print("ERROR: applying filter expression: "..tag.."=...", debug.traceback(2))
          end
        end
      end
      v[#v + 1] = vd
    end
  end
  if #v == 1 then
    return v[1]
  else
    return v
  end
end

local add_vbus = Io.add_vbus

function Io.add_vbus(name, bus)
  bus:set_name(name)
  add_vbus(bus)
end

function Io.add_vbusses(busses)
  for name, bus in pairs(busses) do
    Io.add_vbus(name, bus)
  end
  return busses
end

