-- vi:ft=lua

local hw = Io.system_bus()

-- create a virtual bus 'l4lx'
Io.add_vbus("l4lx", Io.Vi.System_bus
{
  -- add device which matches the compatibility ID (CID)
  -- 'smsc,lan9118'
  NIC = wrap(hw:match("smsc,lan9118"));
})
