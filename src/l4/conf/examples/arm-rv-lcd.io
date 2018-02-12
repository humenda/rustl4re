-- vi:ft=lua
-- configuration file for io

local hw = Io.system_bus()

Io.add_vbus("gui", Io.Vi.System_bus
{
  INPUT = wrap(hw:match("arm,pl050"));
})

Io.add_vbus("fbdrv", Io.Vi.System_bus
{
  CTRL = wrap(hw:match("arm,sysctl"));
  LCD = wrap(hw:match("arm,pl111"));
})
