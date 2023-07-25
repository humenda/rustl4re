package.path = "rom/?.lua";

local L4 = require "L4";
local l = L4.default_loader;

local busses = {
  nic = 1,
}

local io_caps = {
  sigma0 = L4.cast(L4.Proto.Factory, L4.Env.sigma0):create(L4.Proto.Sigma0);
  icu    = L4.Env.icu;
  iommu = L4.Env.iommu;
};

local files = "";

for k, v in pairs(busses) do
  local c = l:new_channel();
  busses[k] = c
  io_caps[k] = c:svr();
  files = files .. " rom/" .. k .. ".vbus";
end

l:start({
  log = { "io", "red" },
  caps = io_caps
}, "rom/io " .. "-vvvvv" .. files);


l:start(
  {
    log = { "verix", "blue" },
    caps =
      {
        vbus = busses.nic,
        jdb = L4.Env.jdb,
      },
  },
  "rom/verix"
);

