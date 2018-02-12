-- this is a configuration to start 'lwip_test'

package.path = "rom/?.lua";

require("L4");
require("Aw");

local ldr = L4.default_loader;

-- channel used for the virtual PCI bus containing NICs
local ankh_vbus = ldr:new_channel();
-- channel for the client to obtain Ankh session
local ankh_clnt = ldr:new_channel();
-- shm area for the Ankh ring buffers
local ankh_shm  = ldr:create_namespace({});

-- start IO server
Aw.io( {ankh = ankh_vbus}, "-vv", "rom/ankh.vbus");

-- start ankh
ldr:startv( {caps = {ankh_service = ankh_clnt:svr(); -- session server
                     vbus = ankh_vbus,               -- the virtual bus
                     shm = ankh_shm:m("rws")},       -- shm namespace read/writable
             log  = {"ankh", "g"},
             l4re_dbg = L4.Dbg.Warn
            },
            "rom/ankh" );

ldr:start(
          { caps = {
                     shm = ankh_shm:m("rws");         -- also r/w the shm namespace
                     -- and this creates the ankh session with some parameters
                     ankh = ankh_clnt:create(0, "debug,phys_mac,shm=shm,bufsize=16384");
                   },
            log = {"lwip", "c"},
            l4re_dbg = L4.Dbg.Boot },
            "rom/lwip_test --shm shm --bufsize 16384");
